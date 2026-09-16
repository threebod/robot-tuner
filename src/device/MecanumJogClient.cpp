#include "device/MecanumJogClient.h"

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
}

void MecanumJogClient::setConnected(bool connected) {
    if (connected_ == connected) {
        return;
    }
    connected_ = connected;
    receiveBuffer_.clear();
    if (connected_) {
        heartbeatTimer_.start();
        emit commandStateChanged(QStringLiteral("文本串口已连接"));
        return;
    }

    heartbeatTimer_.stop();
    invalidateNavigation();
    invalidateMechanism();
    setFullRouteRunning(false);
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
    const bool stopping =
        command == QStringLiteral("stop") || command == QStringLiteral("X") ||
        command == QStringLiteral("x") ||
        command == QStringLiteral("disable");
    if (stopping) {
        armTimer_.stop();
        pendingCommand_.clear();
        invalidateNavigation();
        invalidateMechanism();
        setFullRouteRunning(false);
        emit stopRequested();
    } else if (!pendingCommand_.isEmpty()) {
        emit commandFailed(QStringLiteral("正在等待授权，请稍后发送；停止命令仍可使用"));
        return false;
    }
    emit bytesReady(command.toUtf8() + QByteArray("\r\n"));
    emit commandStateChanged(QStringLiteral("已发送：%1").arg(command));
    return true;
}

bool MecanumJogClient::sendArmedCommand(QString command) {
    command = command.trimmed();
    if (!connected_) {
        emit commandFailed(QStringLiteral("串口未连接"));
        return false;
    }
    if (!validCommand(command)) {
        emit commandFailed(QStringLiteral("请发送一条 1–79 字节的 ASCII 命令"));
        return false;
    }
    if (!pendingCommand_.isEmpty()) {
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
    if (rpm < 10 || rpm > 120) {
        const QString error = QStringLiteral("导航转速必须为 10～120 RPM");
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
    if ((startZone != 1 && startZone != 2) || rpm < 10 || rpm > 120) {
        const QString error = QStringLiteral("完整跑图要求启停区1/2、速度10～120 RPM");
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
        "state=(RUN|TURN) stage=([A-Z0-9_]+) target_x=(\\d+) target_y=(\\d+)$"));
    static const QRegularExpression routeStageExpression(
        QStringLiteral("^ROUTE STAGE index=(\\d+) name=([A-Z0-9_]+)$"));
    static const QRegularExpression routeDoneExpression(
        QStringLiteral("^ROUTE DONE x=(\\d+) y=(\\d+) yaw_cdeg=(-?\\d+)$"));

    emit lineReceived(line);
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
    match = routeDoneExpression.match(line);
    if (match.hasMatch()) {
        setFullRouteRunning(false);
        emit fullRouteCompleted(match.captured(1).toInt(),
                                match.captured(2).toInt());
        return;
    }
    if (line.startsWith(QStringLiteral("ROUTE INVALID"))) {
        setFullRouteRunning(false);
        emit fullRouteError(line);
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
            setFullRouteRunning(false);
            emit fullRouteError(line);
        }
        failPending(line);
        return;
    }
    if (line.startsWith(QStringLiteral("STOP")) ||
        line.startsWith(QStringLiteral("EMERGENCY STOP"))) {
        armTimer_.stop();
        pendingCommand_.clear();
        invalidateNavigation();
        invalidateMechanism();
        setFullRouteRunning(false);
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

void MecanumJogClient::failPending(const QString &reason) {
    const bool navigationPending =
        pendingCommand_.startsWith(QStringLiteral("nav goto "));
    const bool mechanismPending =
        pendingCommand_.startsWith(QStringLiteral("mech pose "));
    const bool fullRoutePending =
        pendingCommand_.startsWith(QStringLiteral("route auto "));
    armTimer_.stop();
    pendingCommand_.clear();
    if (navigationPending) {
        emit navigationError(reason);
    }
    if (mechanismPending) {
        emit mechanismError(reason);
    }
    if (fullRoutePending) {
        setFullRouteRunning(false);
        emit fullRouteError(reason);
    }
    emit commandFailed(reason);
    emit commandStateChanged(QStringLiteral("命令失败：%1").arg(reason));
}
