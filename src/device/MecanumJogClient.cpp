#include "device/MecanumJogClient.h"
#include "../../../test/mecanum_jog/task_code.h"

#include <QRegularExpression>

#include <utility>

namespace {

const QString kArmedReply =
    QStringLiteral("ARMED for one enable or motion command");

bool validMechanismPose(const MechanismPoseData &pose, QString *error) {
    MechanismSequence sequence;
    sequence.initial.pose = pose;
    return validateMechanismSequence(sequence, error);
}

}  // namespace

MecanumJogClient::MecanumJogClient(QObject *parent) : QObject(parent) {
    avoidanceStartTimer_.setSingleShot(true);
    avoidanceStartTimer_.setInterval(3000);
    connect(&avoidanceStartTimer_, &QTimer::timeout, this, [this] {
        if (connected_) emit bytesReady(QByteArray("avoid stop\r\n"));
        failPending(QStringLiteral("避障启动未收到主控确认"));
    });
    heartbeatTimer_.setInterval(250);
    heartbeatTimer_.setTimerType(Qt::CoarseTimer);
    connect(&heartbeatTimer_, &QTimer::timeout, this,
            [this] { emit bytesReady(QByteArray("hb\r\n")); });

    armTimer_.setSingleShot(true);
    armTimer_.setInterval(2000);
    armTimer_.setTimerType(Qt::PreciseTimer);
    connect(&armTimer_, &QTimer::timeout, this, [this] {
        failPending(QStringLiteral("等待 arm 回复超时"));
    });
    missionSetupTimer_.setSingleShot(true);
    missionSetupTimer_.setInterval(10000);
    connect(&missionSetupTimer_, &QTimer::timeout, this, [this] {
        failMissionSetup(QStringLiteral("任务启动准备超时"));
    });
}

void MecanumJogClient::setConnected(bool connected) {
    if (connected_ == connected) {
        return;
    }
    connected_ = connected;
    if (!connected) {
        avoidanceStartTimer_.stop();
        setAvoidanceRunning(false);
        avoidancePathVersion_ = -1;
        avoidancePathStaging_.clear();
    }
    if (!connected) {
        otherMotionRunning_ = false;
        materialForwardMilli_ = VISION_MATERIAL_FORWARD_MILLI;
        materialRightMilli_ = VISION_MATERIAL_RIGHT_MILLI;
    }
    emit connectionChanged(connected);
    receiveBuffer_.clear();
    if (connected_) {
        heartbeatTimer_.start();
        emit commandStateChanged(QStringLiteral("文本串口已连接"));
        return;
    }

    heartbeatTimer_.stop();
    missionSetupStage_ = 0;
    missionSetupTimer_.stop();
    invalidateNavigation();
    invalidateMechanism();
    setFullRouteRunning(false);
    setVisionRunning(false);
    if (!pendingCommand_.isEmpty()) {
        failPending(QStringLiteral("连接已断开，授权命令已取消"));
    } else {
        armTimer_.stop();
        emit commandStateChanged(QStringLiteral("未连接"));
    }
}

void MecanumJogClient::ingestBytes(QByteArrayView bytes) {
    if (bytes.isEmpty()) {
        return;
    }
    receiveBuffer_.append(bytes.data(), bytes.size());
    if (receiveBuffer_.size() > kMaximumReceiveBuffer) {
        receiveBuffer_.remove(0, receiveBuffer_.size() - kMaximumReceiveBuffer);
    }

    int carriageReturn = receiveBuffer_.indexOf('\r');
    int lineFeed = receiveBuffer_.indexOf('\n');
    int newline = carriageReturn < 0 ? lineFeed
                                     : lineFeed < 0 ? carriageReturn
                                                    : qMin(carriageReturn, lineFeed);
    while (newline >= 0) {
        const QByteArray rawLine = receiveBuffer_.left(newline);
        receiveBuffer_.remove(0, newline + 1);
        while (receiveBuffer_.startsWith('\r') ||
               receiveBuffer_.startsWith('\n')) {
            receiveBuffer_.remove(0, 1);
        }
        const QString line = QString::fromUtf8(rawLine);
        if (!line.isEmpty()) {
            handleLine(line);
        }
        carriageReturn = receiveBuffer_.indexOf('\r');
        lineFeed = receiveBuffer_.indexOf('\n');
        newline = carriageReturn < 0 ? lineFeed
                                     : lineFeed < 0 ? carriageReturn
                                                    : qMin(carriageReturn,
                                                           lineFeed);
    }
}

bool MecanumJogClient::sendCommand(QString command) {
    command = command.trimmed();
    if (command.contains('!')) {
        return emergencyStop();
    }
    if (!connected_) {
        emit commandFailed(QStringLiteral("串口未连接"));
        return false;
    }
    if (!validCommand(command)) {
        emit commandFailed(QStringLiteral("请发送一条 1–79 字节的 ASCII 命令"));
        return false;
    }
    const bool visionPause = command == QStringLiteral("vision pause");
    const bool stopping =
        command == QStringLiteral("stop") || command == QStringLiteral("X") ||
        command == QStringLiteral("x") ||
        command == QStringLiteral("disable") || command == QStringLiteral("avoid stop") || visionPause;
    if (avoidanceRunning_ && !stopping && command != "avoid status" && command != "hb") {
        emit commandFailed(QStringLiteral("避障跑图中，请先停止"));
        return false;
    }
    if (routeExecution_ && !routeDispatch_ && !stopping && command != "hb") {
        emit commandStateChanged(QStringLiteral("路线执行中，请先停止路线再发送其他命令"));
        return false;
    }
    if (stopping) {
        avoidanceStartTimer_.stop();
        setAvoidanceRunning(false);
        missionSetupStage_ = 0;
        missionSetupTimer_.stop();
        armTimer_.stop();
        pendingCommand_.clear();
        if (visionPause) {
            setVisionRunning(false);
        } else {
            invalidateNavigation();
            invalidateMechanism();
            setFullRouteRunning(false);
            setVisionRunning(false);
            emit stopRequested();
        }
    } else if ((!pendingCommand_.isEmpty() || missionSetupStage_ != 0) &&
               !missionSetupDispatch_) {
        emit commandFailed(QStringLiteral("正在等待授权，请稍后发送；停止命令仍可使用"));
        return false;
    }
    emit bytesReady(command.toUtf8() + QByteArray("\r\n"));
    emit commandStateChanged(QStringLiteral("已发送：%1").arg(command));
    return true;
}

bool MecanumJogClient::sendArmedCommand(QString command) {
    command = command.trimmed();
    if (avoidanceRunning_) {
        emit commandFailed(QStringLiteral("避障跑图中，请先停止"));
        return false;
    }
    if (routeExecution_ && !routeDispatch_) {
        emit commandStateChanged(QStringLiteral("路线执行中，请先停止路线"));
        return false;
    }
    if (!connected_) {
        emit commandFailed(QStringLiteral("串口未连接"));
        return false;
    }
    if (!validCommand(command)) {
        emit commandFailed(QStringLiteral("请发送一条 1–79 字节的 ASCII 命令"));
        return false;
    }
    if (!pendingCommand_.isEmpty() ||
        (missionSetupStage_ != 0 && !missionSetupDispatch_)) {
        emit commandFailed(QStringLiteral("已有命令正在等待授权"));
        return false;
    }

    pendingCommand_ = std::move(command);
    emit bytesReady(QByteArray("arm\r\n"));
    emit commandStateChanged(QStringLiteral("等待单次授权：%1")
                                 .arg(pendingCommand_));
    armTimer_.start();
    return true;
}

bool MecanumJogClient::emergencyStop() {
    avoidanceStartTimer_.stop();
    setAvoidanceRunning(false);
    missionSetupStage_ = 0;
    missionSetupTimer_.stop();
    armTimer_.stop();
    pendingCommand_.clear();
    invalidateNavigation();
    invalidateMechanism();
    setFullRouteRunning(false);
    emit stopRequested();
    if (!connected_) {
        emit commandFailed(QStringLiteral("串口未连接"));
        return false;
    }
    emit bytesReady(QByteArray("!"));
    emit commandStateChanged(QStringLiteral("已发送紧急停止"));
    return true;
}

bool MecanumJogClient::initializeNavigation(int startZone) {
    if (startZone != 1 && startZone != 2) {
        const QString error = QStringLiteral("启停区必须为 1 或 2");
        emit navigationError(error);
        emit commandFailed(error);
        return false;
    }
    return sendCommand(QStringLiteral("nav init %1").arg(startZone));
}

bool MecanumJogClient::navigateTo(qint32 xMm, qint32 yMm, quint16 rpm) {
    if (xMm < 0 || xMm > 2400 || yMm < 0 || yMm > 2400) {
        const QString error = QStringLiteral("目标坐标超出地图范围");
        emit navigationError(error);
        emit commandFailed(error);
        return false;
    }
    if (rpm < 10 || rpm > 250) {
        const QString error = QStringLiteral("导航转速必须为 10～250 RPM");
        emit navigationError(error);
        emit commandFailed(error);
        return false;
    }
    return sendArmedCommand(
        QStringLiteral("nav goto %1 %2 %3").arg(xMm).arg(yMm).arg(rpm));
}

bool MecanumJogClient::navigationInitialized() const {
    return navigationInitialized_;
}

bool MecanumJogClient::startFullRoute(int startZone, quint16 rpm) {
    if ((startZone != 1 && startZone != 2) || rpm < 10 || rpm > 250) {
        const QString error = QStringLiteral("完整跑图要求启停区1/2、速度10～250 RPM");
        emit fullRouteError(error);
        emit commandFailed(error);
        return false;
    }
    if (!sendArmedCommand(
            QStringLiteral("route auto %1 %2").arg(startZone).arg(rpm))) {
        return false;
    }
    setFullRouteRunning(true);
    return true;
}

void MecanumJogClient::setAvoidanceRunning(bool running) {
    if (avoidanceRunning_ == running) return;
    avoidanceRunning_ = running;
    emit avoidanceRunningChanged(running);
}

bool MecanumJogClient::startAvoidanceRun(int startZone, quint16 rpm) {
    if ((startZone != 1 && startZone != 2) || rpm < 10 || rpm > 250) {
        emit avoidanceError(QStringLiteral("避障跑图要求启停区1/2、速度10～250 RPM"));
        return false;
    }
    if (otherMotionRunning_ || fullRouteRunning_ || visionRunning_ || routeExecution_ || avoidanceRunning_) {
        emit avoidanceError(QStringLiteral("已有动作运行，请先停止"));
        return false;
    }
    if (!sendArmedCommand(QStringLiteral("avoid start %1 %2").arg(startZone).arg(rpm))) return false;
    setAvoidanceRunning(true);
    avoidanceStartTimer_.start();
    return true;
}

bool MecanumJogClient::stopAvoidanceRun() {
    return sendCommand(QStringLiteral("avoid stop"));
}

bool MecanumJogClient::requestAvoidanceStatus() {
    return sendCommand(QStringLiteral("avoid status"));
}

bool MecanumJogClient::clearAvoidanceMemory() {
    if (avoidanceRunning_ || otherMotionRunning_ || fullRouteRunning_ || visionRunning_ || routeExecution_) {
        emit avoidanceError(QStringLiteral("请先停车，再清空障碍记录"));
        return false;
    }
    return sendCommand(QStringLiteral("avoid clear"));
}

bool MecanumJogClient::handleAvoidanceLine(const QString &line) {
    if (!line.startsWith(QStringLiteral("AV "))) return false;
    static const QRegularExpression stateExpression(QStringLiteral(
        "^AV STATE state=(IDLE|CAMERA|PLAN|TURN|MOVE|CONFIRM|RETREAT|DWELL|DONE|FAULT) "
        "active=([01]) round=([12]) target=([A-Z0-9_]+) region=([0-9]) blocked=(\\d+) "
        "replans=(\\d+) camera=([01]) x=(-?\\d+) y=(-?\\d+) yaw_cdeg=(-?\\d+) reason=([A-Z_]+)$"));
    static const QRegularExpression beginExpression(QStringLiteral("^AV PATH_BEGIN version=(\\d+) count=(\\d+)$"));
    static const QRegularExpression pointExpression(QStringLiteral("^AV PATH_POINT version=(\\d+) index=(\\d+) x=(\\d+) y=(\\d+)$"));
    static const QRegularExpression endExpression(QStringLiteral("^AV PATH_END version=(\\d+)$"));
    auto match = stateExpression.match(line);
    if (match.hasMatch()) {
        AvoidanceStatus status;
        status.state = match.captured(1); status.active = match.captured(2) == "1";
        status.round = match.captured(3).toInt(); status.target = match.captured(4);
        status.region = match.captured(5).toInt(); status.blockedMask = match.captured(6).toInt();
        status.replans = match.captured(7).toInt(); status.cameraOnline = match.captured(8) == "1";
        status.xMm = match.captured(9).toInt(); status.yMm = match.captured(10).toInt();
        status.yawDegrees = match.captured(11).toInt() / 100.0; status.reason = match.captured(12);
        if (status.blockedMask < 0 || status.blockedMask > 511 || status.replans < 0 || status.replans > 65535) return true;
        avoidanceStartTimer_.stop();
        setAvoidanceRunning(status.active);
        otherMotionRunning_ = status.active;
        emit avoidanceStateReceived(status);
        if (status.state == "FAULT") emit avoidanceError(status.reason);
        return true;
    }
    match = beginExpression.match(line);
    if (match.hasMatch()) {
        const int version = match.captured(1).toInt(), count = match.captured(2).toInt();
        avoidancePathVersion_ = -1;
        avoidancePathStaging_.clear();
        if (version >= 0 && version <= 65535 && count >= 0 && count <= 180) {
            avoidancePathVersion_ = version;
            avoidancePathExpected_ = count;
        }
        return true;
    }
    match = pointExpression.match(line);
    if (match.hasMatch()) {
        const int version = match.captured(1).toInt(), index = match.captured(2).toInt();
        const int x = match.captured(3).toInt(), y = match.captured(4).toInt();
        if (version != avoidancePathVersion_) return true;
        if (index != avoidancePathStaging_.size() || index >= avoidancePathExpected_ ||
            x < 0 || x > 2400 || y < 0 || y > 2400) {
            avoidancePathVersion_ = -1;
            return true;
        }
        avoidancePathStaging_.append(QPointF(x, y));
        return true;
    }
    match = endExpression.match(line);
    if (match.hasMatch() && match.captured(1).toInt() == avoidancePathVersion_ &&
        avoidancePathStaging_.size() == avoidancePathExpected_) {
        emit avoidancePathReceived(avoidancePathStaging_);
        avoidancePathVersion_ = -1;
    }
    return true;
}

bool MecanumJogClient::startRawPickRoute(int startZone, quint16 rpm) {
    if ((startZone != 1 && startZone != 2) || rpm < 10 || rpm > 250) {
        const QString error = QStringLiteral("路线取料要求启停区1/2、速度10～250 RPM");
        emit fullRouteError(error);
        emit commandFailed(error);
        return false;
    }
    if (!sendArmedCommand(
            QStringLiteral("route rawpick %1 %2").arg(startZone).arg(rpm))) {
        return false;
    }
    setFullRouteRunning(true);
    return true;
}

bool MecanumJogClient::startMissionRoute(int startZone, const QString &taskCode, quint16 rpm,
                                         int forwardMilli, int rightMilli, bool waitForButton) {
    MissionTask task;
    const QByteArray encodedTask = taskCode.toUtf8();
    if (encodedTask.size() != 15 || !missionTaskParse(encodedTask.constData(), &task)) {
        const QString error = QStringLiteral("任务码格式为CCC+PPP+CCC+PPP；颜色1～6，每组位置须为1、2、3的排列");
        emit fullRouteError(error);
        emit commandFailed(error);
        return false;
    }
    if ((startZone != 1 && startZone != 2) || rpm < 10 || rpm > 250 ||
        forwardMilli < 50 || forwardMilli > 2000 ||
        rightMilli < 50 || rightMilli > 2000) {
        const QString error = QStringLiteral("完整任务要求启停区1/2、速度10～250 RPM、视觉比例50～2000");
        emit fullRouteError(error);
        emit commandFailed(error);
        return false;
    }
    if (!connected_ || otherMotionRunning_ || fullRouteRunning_ ||
        missionSetupStage_ != 0 || !pendingCommand_.isEmpty()) {
        const QString error = QStringLiteral("请先连接，等待其他动作或命令完成");
        emit fullRouteError(error);
        emit commandFailed(error);
        return false;
    }
    missionStartZone_ = startZone;
    missionTaskCode_ = taskCode;
    missionRpm_ = rpm;
    missionForwardMilli_ = forwardMilli;
    missionRightMilli_ = rightMilli;
    missionWaitForButton_ = waitForButton;
    // At power-on the axes are physically at zero and the turret is at 65 degrees.
    missionSetupStage_ = mechanismInitialized_ ? 1 : 7;
    dispatchMissionSetup(mechanismInitialized_
                             ? QStringLiteral("task %1").arg(missionTaskCode_)
                             : QStringLiteral("mech init 0 0 650"), false);
    if (missionSetupStage_ == 0) return false;
    setFullRouteRunning(true);
    return true;
}

bool MecanumJogClient::continueRawPickRoute() {
    return sendCommand(QStringLiteral("route next"));
}

bool MecanumJogClient::fullRouteRunning() const {
    return fullRouteRunning_;
}

bool MecanumJogClient::initializeMechanism(const MechanismPoseData &pose) {
    QString error;
    if (!validMechanismPose(pose, &error)) {
        emit mechanismError(error);
        emit commandFailed(error);
        return false;
    }
    return sendCommand(QStringLiteral("mech init %1 %2 %3")
                           .arg(pose.horizontalDmm)
                           .arg(pose.liftDmm)
                           .arg(pose.turretDdeg));
}

bool MecanumJogClient::moveMechanism(const MechanismPoseData &pose) {
    QString error;
    if (!validMechanismPose(pose, &error)) {
        emit mechanismError(error);
        emit commandFailed(error);
        return false;
    }
    return sendArmedCommand(QStringLiteral("mech pose %1 %2 %3 %4 %5 %6 %7 %8")
                                .arg(pose.horizontalDmm)
                                .arg(pose.liftDmm)
                                .arg(pose.turretDdeg)
                                .arg(pose.horizontalRpm)
                                .arg(pose.horizontalAccel)
                                .arg(pose.liftRpm)
                                .arg(pose.liftAccel)
                                .arg(pose.turretDps10));
}

bool MecanumJogClient::requestMechanismStatus() {
    return sendCommand(QStringLiteral("mech status"));
}

bool MecanumJogClient::mechanismInitialized() const {
    return mechanismInitialized_;
}

bool MecanumJogClient::startVisionMaterialPickup(int color) {
    if (color < 1 || color > 6) {
        emit commandFailed(QStringLiteral("物料颜色编号必须为 1～6"));
        return false;
    }
    const bool started = sendArmedCommand(
        QStringLiteral("vision pick material %1").arg(color));
    if (started) setVisionRunning(true);
    return started;
}

bool MecanumJogClient::startVisionRingAlignment(int ring) {
    if (ring < 1 || ring > 3) {
        emit commandFailed(QStringLiteral("圆环编号必须为 1～3"));
        return false;
    }
    const bool started =
        sendArmedCommand(QStringLiteral("vision align ring %1").arg(ring));
    if (started) setVisionRunning(true);
    return started;
}

bool MecanumJogClient::setVisionRingScale(int ring, int forwardMilli,
                                          int rightMilli) {
    if (ring < 1 || ring > 3 || forwardMilli < 50 || forwardMilli > 2000 ||
        rightMilli < 50 || rightMilli > 2000) {
        emit commandFailed(QStringLiteral("圆环比例超出范围"));
        return false;
    }
    return sendArmedCommand(QStringLiteral("vision scale ring %1 %2 %3")
                                .arg(ring).arg(forwardMilli).arg(rightMilli));
}

bool MecanumJogClient::pauseVision() {
    return sendCommand(QStringLiteral("vision pause"));
}

bool MecanumJogClient::requestVisionStatus() {
    return sendCommand(QStringLiteral("vision status"));
}

bool MecanumJogClient::jogVision(int forwardMm, int rightMm, int rpm) {
    if ((forwardMm == 0 && rightMm == 0) || forwardMm < -20 ||
        forwardMm > 20 || rightMm < -20 || rightMm > 20 || rpm < 10 ||
        rpm > 30) {
        emit commandFailed(QStringLiteral("视觉点动范围为每轴 ±20 mm、10～30 RPM"));
        return false;
    }
    return sendArmedCommand(QStringLiteral("vision jog %1 %2 %3")
                                .arg(forwardMm)
                                .arg(rightMm)
                                .arg(rpm));
}

bool MecanumJogClient::visionRunning() const {
    return visionRunning_;
}

bool MecanumJogClient::beginRouteExecution() {
    if (!connected_ || routeExecution_ || otherMotionRunning_ || fullRouteRunning_ ||
        visionRunning_ || !pendingCommand_.isEmpty() || missionSetupStage_ != 0) return false;
    routeExecution_ = true;
    return true;
}

void MecanumJogClient::endRouteExecution() {
    routeExecution_ = false;
}

bool MecanumJogClient::sendRouteCommand(const QString &command, bool armed) {
    if (!routeExecution_) return false;
    routeDispatch_ = true;
    const bool sent = armed ? sendArmedCommand(command) : sendCommand(command);
    routeDispatch_ = false;
    return sent;
}

bool MecanumJogClient::validCommand(const QString &command) const {
    if (command.isEmpty() || command.size() > 79) {
        return false;
    }
    for (const QChar character : command) {
        if (character.unicode() < 0x20 || character.unicode() > 0x7e ||
            character == QLatin1Char('!')) {
            return false;
        }
    }
    return true;
}

void MecanumJogClient::handleLine(const QString &line) {
    if (line.startsWith("MOVE RUN ") || line.startsWith("TURN running") ||
        line.startsWith("RUN ") || line.startsWith("TX queued:") ||
        line.startsWith("MOVING servo=") || line.startsWith("MECH RUN ") ||
        (line.startsWith("NAV POS ") && !line.contains("state=IDLE"))) otherMotionRunning_ = true;
    if (line.startsWith("MOVE DONE ") || line.startsWith("TURN DONE") ||
        line.startsWith("MECH DONE ") || line.startsWith("DONE servo=") ||
        line.startsWith("NAV DONE ") || line.startsWith("STOP") ||
        line.startsWith("ERR") || line.startsWith("EMERGENCY STOP")) otherMotionRunning_ = false;
    static const QRegularExpression initExpression(
        QStringLiteral("^NAV INIT x=(\\d+) y=(\\d+) yaw_cdeg=(-?\\d+)$"));
    static const QRegularExpression positionExpression(QStringLiteral(
        "^NAV POS x=(\\d+) y=(\\d+) yaw_cdeg=(-?\\d+) "
        "state=(IDLE|RUN|TURN) target_x=\\d+ target_y=\\d+$"));
    static const QRegularExpression doneExpression(
        QStringLiteral("^NAV DONE x=(\\d+) y=(\\d+) yaw_cdeg=(-?\\d+)$"));
    static const QRegularExpression mechanismExpression(QStringLiteral(
        "^MECH (INIT|RUN|POS|DONE) h=(-?\\d+) l=(\\d+) t=(\\d+)$"));
    static const QRegularExpression routePositionExpression(QStringLiteral(
        "^ROUTE POS x=(\\d+) y=(\\d+) yaw_cdeg=(-?\\d+) "
        "state=(RUN|TURN|WAIT) stage=([A-Z0-9_]+) target_x=(\\d+) target_y=(\\d+)$"));
    static const QRegularExpression routeDistanceExpression(QStringLiteral(
        "^ROUTE DIST index=(\\d+) direction=(UP|DOWN|LEFT|RIGHT) "
        "distance_mm=(\\d+) progress_mm=(\\d+) yaw_cdeg=(-?\\d+) "
        "state=(RUN|TURN|WAIT) stage=([A-Z0-9_]+)$"));
    static const QRegularExpression routeStageExpression(
        QStringLiteral("^ROUTE STAGE index=(\\d+) name=([A-Z0-9_]+)$"));
    static const QRegularExpression routeDoneExpression(
        QStringLiteral("^ROUTE DONE x=(\\d+) y=(\\d+) yaw_cdeg=(-?\\d+)$"));
    static const QRegularExpression rawPickExpression(QStringLiteral(
        "^ROUTE RAWPICK state=([A-Z_]+) color=([0-6]) slot=([0-3])$"));
    static const QRegularExpression missionExpression(QStringLiteral(
        "^ROUTE MISSION phase=([A-Z_]+) station=([A-Z0-9_]+)$"));
    static const QRegularExpression visionStateExpression(QStringLiteral(
        "^VISION STATE state=([A-Z_]+) mode=([A-Z]+) selector=(\\d+) "
        "target=(\\d+) iteration=(\\d+)$"));
    static const QRegularExpression visionSampleExpression(QStringLiteral(
        "^VISION SAMPLE token=(\\d+) du=(-?\\d+) dv=(-?\\d+) "
        "forward_mm=(-?\\d+(?:\\.\\d+)?) right_mm=(-?\\d+(?:\\.\\d+)?) "
        "quality=(\\d+) iteration=(\\d+)$"));
    static const QRegularExpression visionDoneExpression(QStringLiteral(
        "^VISION DONE forward_mm=(-?\\d+(?:\\.\\d+)?) "
        "right_mm=(-?\\d+(?:\\.\\d+)?) iterations=(\\d+)$"));

    emit lineReceived(line);
    if (handleAvoidanceLine(line)) return;
    if (avoidanceRunning_ && !avoidanceStartTimer_.isActive() && line.startsWith(QStringLiteral("ERR"))) {
        otherMotionRunning_ = true;
        emit avoidanceError(line);
        emit commandFailed(line);
        return;
    }
    const QString qrPrefix = QStringLiteral("QR RESULT value=");
    if (line.startsWith(qrPrefix) && line.size() > qrPrefix.size()) {
        emit qrResultReceived(line.mid(qrPrefix.size()));
        return;
    }
    if (missionSetupStage_ != 0) {
        if (line.startsWith(QStringLiteral("STOP")) ||
            line.startsWith(QStringLiteral("EMERGENCY STOP"))) {
            missionSetupStage_ = 0;
            missionSetupTimer_.stop();
        } else if (line.startsWith(QStringLiteral("MECH INVALID")) ||
                   line.startsWith(QStringLiteral("ROUTE INVALID"))) {
            failMissionSetup(line);
            return;
        }
    }
    if (missionSetupStage_ != 0) {
        if (line.startsWith(QStringLiteral("ERR")) ||
            line.startsWith(QStringLiteral("VISION ERROR"))) {
            failMissionSetup(line);
            return;
        }
        if (missionSetupStage_ == 7 &&
            line == QStringLiteral("MECH INIT h=0 l=0 t=650")) {
            missionSetupStage_ = 1;
            dispatchMissionSetup(QStringLiteral("task %1").arg(missionTaskCode_), false);
        }
        if (missionSetupStage_ == 1 &&
            line == QStringLiteral("TASK SET code=%1").arg(missionTaskCode_)) {
            missionSetupStage_ = 2;
            dispatchMissionSetup(QStringLiteral("servo 2 90 2700"), false);
            return;
        }
        if (missionSetupStage_ == 2 &&
            (line.startsWith(QStringLiteral("DONE servo=2 angle=90")) ||
             line.startsWith(QStringLiteral("OK servo=2 angle=90")))) {
            missionSetupStage_ = 3;
            dispatchMissionSetup(QStringLiteral("servo 3 26 1200"), false);
            return;
        }
        if (missionSetupStage_ == 3 &&
            (line.startsWith(QStringLiteral("DONE servo=3 angle=26")) ||
             line.startsWith(QStringLiteral("OK servo=3 angle=26")))) {
            missionSetupStage_ = 4;
            dispatchMissionSetup(QStringLiteral("vision scale material %1 %2")
                                     .arg(materialForwardMilli_)
                                     .arg(materialRightMilli_), true);
            return;
        }
        if (missionSetupStage_ == 4 &&
            line == QStringLiteral("VISION SCALE material forward_milli=%1 right_milli=%2")
                        .arg(materialForwardMilli_).arg(materialRightMilli_)) {
            missionSetupStage_ = 5;
            dispatchMissionSetup(QStringLiteral("vision scale ring 2 %1 %2")
                                     .arg(missionForwardMilli_)
                                     .arg(missionRightMilli_), true);
            return;
        }
        if (missionSetupStage_ == 5 &&
            line == QStringLiteral("VISION SCALE ring=2 forward_milli=%1 right_milli=%2")
                        .arg(missionForwardMilli_).arg(missionRightMilli_)) {
            if (missionWaitForButton_) {
                missionSetupStage_ = 6;
                dispatchMissionSetup(QStringLiteral("route button %1").arg(missionRpm_), true);
            } else {
                missionSetupStage_ = 0;
                missionSetupTimer_.stop();
                sendArmedCommand(QStringLiteral("route mission %1 %2")
                                     .arg(missionStartZone_).arg(missionRpm_));
            }
        }
        if (missionSetupStage_ == 6 &&
            line == QStringLiteral("BUTTON READY rpm=%1").arg(missionRpm_)) {
            missionSetupStage_ = 0;
            missionSetupTimer_.stop();
            return;
        }
    }
    QRegularExpressionMatch match = initExpression.match(line);
    if (match.hasMatch()) {
        navigationInitialized_ = true;
        emit navigationValidityChanged(true);
        emit navigationEstimateReceived(
            match.captured(1).toInt(), match.captured(2).toInt(),
            match.captured(3).toDouble() / 100.0, QStringLiteral("IDLE"));
        return;
    }
    match = positionExpression.match(line);
    if (match.hasMatch()) {
        emit navigationEstimateReceived(
            match.captured(1).toInt(), match.captured(2).toInt(),
            match.captured(3).toDouble() / 100.0, match.captured(4));
        return;
    }
    match = doneExpression.match(line);
    if (match.hasMatch()) {
        emit navigationEstimateReceived(
            match.captured(1).toInt(), match.captured(2).toInt(),
            match.captured(3).toDouble() / 100.0, QStringLiteral("IDLE"));
        emit navigationCompleted(match.captured(1).toInt(),
                                 match.captured(2).toInt());
        return;
    }
    if (line.startsWith(QStringLiteral("NAV INVALID"))) {
        invalidateNavigation();
        return;
    }
    match = mechanismExpression.match(line);
    if (match.hasMatch()) {
        MechanismPoseData pose;
        pose.horizontalDmm = match.captured(2).toInt();
        pose.liftDmm = match.captured(3).toInt();
        pose.turretDdeg = match.captured(4).toInt();
        const QString event = match.captured(1);
        if (event == QStringLiteral("INIT") && !mechanismInitialized_) {
            mechanismInitialized_ = true;
            emit mechanismValidityChanged(true);
        }
        emit mechanismEstimateReceived(
            pose, event == QStringLiteral("INIT") ||
                          event == QStringLiteral("POS") ||
                          event == QStringLiteral("DONE")
                      ? QStringLiteral("IDLE")
                      : QStringLiteral("RUN"));
        if (event == QStringLiteral("DONE")) {
            emit mechanismCompleted(pose);
        }
        return;
    }
    if (line.startsWith(QStringLiteral("MECH INVALID"))) {
        invalidateMechanism();
        return;
    }
    match = routeDistanceExpression.match(line);
    if (match.hasMatch()) {
        setFullRouteRunning(true);
        emit fullRouteDistanceReceived(
            match.captured(1).toInt(), match.captured(2),
            match.captured(3).toInt(), match.captured(4).toInt(),
            match.captured(5).toDouble() / 100.0, match.captured(6),
            match.captured(7));
        return;
    }
    if (line == QStringLiteral("ROUTE DONE mode=DISTANCE")) {
        emit fullRouteDistanceCompleted();
        setFullRouteRunning(false);
        return;
    }
    match = routePositionExpression.match(line);
    if (match.hasMatch()) {
        setFullRouteRunning(true);
        emit fullRouteEstimateReceived(
            match.captured(1).toInt(), match.captured(2).toInt(),
            match.captured(3).toDouble() / 100.0, match.captured(4),
            match.captured(5), match.captured(6).toInt(),
            match.captured(7).toInt());
        return;
    }
    match = routeStageExpression.match(line);
    if (match.hasMatch()) {
        emit fullRouteStageChanged(match.captured(1).toInt(),
                                   match.captured(2));
        return;
    }
    match = rawPickExpression.match(line);
    if (match.hasMatch()) {
        emit rawPickRouteStateChanged(match.captured(1),
                                      match.captured(2).toInt(),
                                      match.captured(3).toInt());
        return;
    }
    match = missionExpression.match(line);
    if (match.hasMatch()) {
        emit missionPhaseChanged(match.captured(1), match.captured(2));
        return;
    }
    if (line == QStringLiteral("ROUTE continuing")) {
        emit rawPickRouteContinued();
        return;
    }
    match = routeDoneExpression.match(line);
    if (match.hasMatch()) {
        emit fullRouteCompleted(match.captured(1).toInt(),
                                match.captured(2).toInt());
        setFullRouteRunning(false);
        return;
    }
    if (line.startsWith(QStringLiteral("ROUTE INVALID"))) {
        emit fullRouteError(line);
        setFullRouteRunning(false);
        return;
    }
    static const QRegularExpression materialScaleExpression(QStringLiteral(
        "^VISION SCALE material forward_milli=(\\d+) right_milli=(\\d+)$"));
    match = materialScaleExpression.match(line);
    if (match.hasMatch() && match.captured(1).toInt() >= 50 && match.captured(1).toInt() <= 2000 &&
        match.captured(2).toInt() >= 50 && match.captured(2).toInt() <= 2000) {
        if (missionSetupStage_ == 0) {
            materialForwardMilli_ = match.captured(1).toInt();
            materialRightMilli_ = match.captured(2).toInt();
        }
        emit visionMaterialScaleApplied(match.captured(1).toInt(), match.captured(2).toInt());
        return;
    }
    static const QRegularExpression visionScaleExpression(QStringLiteral(
        "^VISION SCALE ring=([1-3]) forward_milli=(\\d+) right_milli=(\\d+)$"));
    match = visionScaleExpression.match(line);
    if (match.hasMatch()) {
        emit visionRingScaleApplied(match.captured(1).toInt(),
                                    match.captured(2).toInt(),
                                    match.captured(3).toInt());
        return;
    }
    match = visionStateExpression.match(line);
    if (match.hasMatch()) {
        const QString state = match.captured(1);
        setVisionRunning(state != QStringLiteral("IDLE") &&
                         state != QStringLiteral("ALIGNED") &&
                         state != QStringLiteral("FAILED") &&
                         state != QStringLiteral("PAUSED") &&
                         state != QStringLiteral("PICK_DONE"));
        emit visionStateChanged(state);
        return;
    }
    match = visionSampleExpression.match(line);
    if (match.hasMatch()) {
        emit visionSampleReceived(match.captured(2).toInt(),
                                  match.captured(3).toInt(),
                                  match.captured(4).toDouble(),
                                  match.captured(5).toDouble(),
                                  match.captured(6).toInt(),
                                  match.captured(7).toInt());
        return;
    }
    match = visionDoneExpression.match(line);
    if (match.hasMatch()) {
        setVisionRunning(false);
        emit visionStateChanged(QStringLiteral("ALIGNED"));
        emit visionCompleted(match.captured(1).toDouble(),
                             match.captured(2).toDouble(),
                             match.captured(3).toInt());
        return;
    }
    if (line == QStringLiteral("VISION PAUSED")) {
        setVisionRunning(false);
        emit visionStateChanged(QStringLiteral("PAUSED"));
        return;
    }
    if (line.startsWith(QStringLiteral("VISION ERROR"))) {
        setVisionRunning(false);
        emit visionStateChanged(QStringLiteral("FAILED"));
        emit visionError(line);
        if (!pendingCommand_.isEmpty()) failPending(line);
        return;
    }
    if (line.startsWith(QStringLiteral("ERR"))) {
        if (line.startsWith(QStringLiteral("ERR NAV:"))) {
            emit navigationError(line);
        }
        if (line.startsWith(QStringLiteral("ERR NAV:")) &&
            line.contains(QStringLiteral("position invalid"))) {
            invalidateNavigation();
        }
        if (line.startsWith(QStringLiteral("ERR MECH:"))) {
            emit mechanismError(line);
        }
        if (line.startsWith(QStringLiteral("ERR ROUTE:"))) {
            emit fullRouteError(line);
            setFullRouteRunning(false);
        }
        failPending(line);
        return;
    }
    if (line.startsWith(QStringLiteral("STOP")) ||
        line.startsWith(QStringLiteral("EMERGENCY STOP"))) {
        avoidanceStartTimer_.stop();
        setAvoidanceRunning(false);
        armTimer_.stop();
        pendingCommand_.clear();
        invalidateNavigation();
        invalidateMechanism();
        setFullRouteRunning(false);
        setVisionRunning(false);
        emit stopRequested();
        emit commandStateChanged(QStringLiteral("设备已停止：%1").arg(line));
        return;
    }
    if (pendingCommand_.isEmpty()) {
        if (line.startsWith(QStringLiteral("OK")) ||
            line.startsWith(QStringLiteral("RUN")) ||
            line.startsWith(QStringLiteral("TX queued")) ||
            line.startsWith(QStringLiteral("DONE")) ||
            line.startsWith(QStringLiteral("ROUTE")) ||
            line.startsWith(QStringLiteral("TURN"))) {
            emit commandStateChanged(QStringLiteral("设备回复：%1").arg(line));
        }
        return;
    }
    if (line != kArmedReply) {
        return;
    }

    armTimer_.stop();
    const QString command = pendingCommand_;
    pendingCommand_.clear();
    emit bytesReady(command.toUtf8() + QByteArray("\r\n"));
    emit commandStateChanged(QStringLiteral("已授权并发送：%1").arg(command));
}

void MecanumJogClient::invalidateNavigation() {
    if (!navigationInitialized_) {
        return;
    }
    navigationInitialized_ = false;
    emit navigationValidityChanged(false);
}

void MecanumJogClient::invalidateMechanism() {
    if (!mechanismInitialized_) {
        return;
    }
    mechanismInitialized_ = false;
    emit mechanismValidityChanged(false);
}

void MecanumJogClient::setFullRouteRunning(bool running) {
    if (fullRouteRunning_ == running) return;
    fullRouteRunning_ = running;
    emit fullRouteRunningChanged(running);
}

void MecanumJogClient::setVisionRunning(bool running) {
    if (visionRunning_ == running) return;
    visionRunning_ = running;
    emit visionRunningChanged(running);
}

void MecanumJogClient::failPending(const QString &reason) {
    if (avoidanceRunning_) {
        avoidanceStartTimer_.stop();
        setAvoidanceRunning(false);
        emit avoidanceError(reason);
    }
    const bool missionSetupPending = missionSetupStage_ != 0;
    const bool navigationPending =
        pendingCommand_.startsWith(QStringLiteral("nav goto "));
    const bool mechanismPending =
        pendingCommand_.startsWith(QStringLiteral("mech pose "));
    const bool fullRoutePending =
        pendingCommand_.startsWith(QStringLiteral("route auto ")) ||
        pendingCommand_.startsWith(QStringLiteral("route rawpick ")) ||
        pendingCommand_.startsWith(QStringLiteral("route button ")) ||
        pendingCommand_.startsWith(QStringLiteral("route mission "));
    const bool visionPending =
        pendingCommand_.startsWith(QStringLiteral("vision "));
    armTimer_.stop();
    pendingCommand_.clear();
    if (navigationPending) {
        emit navigationError(reason);
    }
    if (mechanismPending) {
        emit mechanismError(reason);
    }
    if (fullRoutePending) {
        emit fullRouteError(reason);
        setFullRouteRunning(false);
    }
    if (visionPending) {
        setVisionRunning(false);
        emit visionError(reason);
    }
    emit commandFailed(reason);
    emit commandStateChanged(QStringLiteral("命令失败：%1").arg(reason));
    if (missionSetupPending) failMissionSetup(reason);
}

void MecanumJogClient::dispatchMissionSetup(const QString &command, bool armed) {
    missionSetupTimer_.start();
    missionSetupDispatch_ = true;
    const bool sent = armed ? sendArmedCommand(command) : sendCommand(command);
    missionSetupDispatch_ = false;
    if (!sent) failMissionSetup(QStringLiteral("任务启动准备命令发送失败"));
}

void MecanumJogClient::failMissionSetup(const QString &reason) {
    if (missionSetupStage_ == 0) return;
    missionSetupStage_ = 0;
    missionSetupTimer_.stop();
    armTimer_.stop();
    pendingCommand_.clear();
    emit fullRouteError(reason);
    setFullRouteRunning(false);
    emit commandFailed(reason);
    if (connected_) sendCommand(QStringLiteral("stop"));
}
