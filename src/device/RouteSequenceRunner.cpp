#include "device/RouteSequenceRunner.h"
#include "device/MecanumJogClient.h"

RouteSequenceRunner::RouteSequenceRunner(MecanumJogClient *client, QObject *parent)
    : QObject(parent), client_(client) {
    timeout_.setSingleShot(true);
    wait_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] { abort(QStringLiteral("等待当前步骤回复超时")); });
    connect(&wait_, &QTimer::timeout, this, [this] {
        if (!running_) return;
        if (sequence_.steps[index_].type == RouteStepType::Action) {
            ++actionIndex_;
            defer(true);
        } else advance();
    });
    connect(client_, &MecanumJogClient::lineReceived, this, &RouteSequenceRunner::handleLine);
    connect(client_, &MecanumJogClient::connectionChanged, this, [this](bool connected) {
        if (!connected) disconnected();
    });
    connect(client_, &MecanumJogClient::commandFailed, this, [this](const QString &error) {
        if (running_) abort(error);
    });
    connect(client_, &MecanumJogClient::stopRequested, this, [this] {
        if (running_) finish(false, QStringLiteral("路线已停止"));
    });
}

bool RouteSequenceRunner::awaitingConfirmation() const {
    return running_ && stage_ == Stage::Confirm;
}

bool RouteSequenceRunner::start(const RouteSequence &sequence, int from, bool selectedOnly) {
    if (running_) return false;
    QString error;
    if (!validateRouteSequence(sequence, &error) || from < 0 || from >= sequence.steps.size()) {
        emit failed(error.isEmpty() ? QStringLiteral("请选择有效路线步骤") : error);
        return false;
    }
    if (!client_->beginRouteExecution()) {
        emit failed(QStringLiteral("请连接文本串口，并等待其他运动、授权或视觉操作结束"));
        return false;
    }
    sequence_ = sequence;
    index_ = from;
    end_ = selectedOnly ? from + 1 : sequence.steps.size();
    ++generation_;
    running_ = true;
    emit runningChanged(true);
    dispatchStep();
    return running_;
}

bool RouteSequenceRunner::send(const QString &command, bool armed, int timeoutMs) {
    seenStart_ = false;
    if (timeoutMs > 0) timeout_.start(timeoutMs);
    if (!client_->sendRouteCommand(command, armed)) {
        if (running_) abort(QStringLiteral("步骤指令发送失败"));
        return false;
    }
    return running_;
}

void RouteSequenceRunner::dispatchStep() {
    if (!running_) return;
    if (index_ >= end_) { finish(true); return; }
    const auto &step = sequence_.steps[index_];
    emit progressChanged(index_, QStringLiteral("执行 %1/%2：%3")
        .arg(index_ + 1).arg(end_).arg(routeStepDescription(step)));
    switch (step.type) {
    case RouteStepType::Move:
        stage_ = Stage::Move;
        send(QStringLiteral("move %1 %2 %3").arg(step.direction).arg(step.distanceMm).arg(step.rpm), true,
             step.distanceMm * 80 + 20000);
        break;
    case RouteStepType::Turn:
        stage_ = Stage::Turn;
        send(QStringLiteral("turn %1 %2").arg(step.direction).arg(step.degrees), true, 22000);
        break;
    case RouteStepType::Vision:
        stage_ = Stage::Scale;
        send(step.ring ? QStringLiteral("vision scale ring %1 %2 %3").arg(step.target).arg(step.forwardMilli).arg(step.rightMilli) :
             QStringLiteral("vision scale material %1 %2").arg(step.forwardMilli).arg(step.rightMilli), true, 5000);
        break;
    case RouteStepType::Action:
        stage_ = Stage::InitialStatus;
        actionIndex_ = 0; cycle_ = 1;
        actionPose_ = step.action.initial.pose;
        initialPoseReceived_ = initialServosReceived_ = false;
        send(QStringLiteral("mech status"), false, 5000);
        break;
    case RouteStepType::Wait:
        stage_ = Stage::Wait;
        wait_.start(step.waitMs);
        break;
    case RouteStepType::Confirm:
        stage_ = Stage::Confirm;
        emit progressChanged(index_, step.prompt);
        emit confirmationChanged(true);
        break;
    }
}

void RouteSequenceRunner::dispatchAction() {
    if (!running_) return;
    const auto &action = sequence_.steps[index_].action;
    if (actionIndex_ >= action.steps.size()) {
        if (action.loopEnabled && cycle_ < action.loopCount) { ++cycle_; actionIndex_ = 0; }
        else { advance(); return; }
    }
    const auto &step = action.steps[actionIndex_];
    emit progressChanged(index_, QStringLiteral("%1：第 %2 轮，第 %3/%4 个机构动作")
        .arg(action.name).arg(cycle_).arg(actionIndex_ + 1).arg(action.steps.size()));
    if (step.type == MechanismStepType::Wait) {
        stage_ = Stage::Wait;
        wait_.start(step.waitMs);
    } else if (step.type == MechanismStepType::Pose) {
        const auto &p = step.pose;
        expectedPose_ = p;
        stage_ = Stage::Pose;
        send(QStringLiteral("mech pose %1 %2 %3 %4 %5 %6 %7 %8")
            .arg(p.horizontalDmm).arg(p.liftDmm).arg(p.turretDdeg).arg(p.horizontalRpm)
            .arg(p.horizontalAccel).arg(p.liftRpm).arg(p.liftAccel).arg(p.turretDps10), true, 300000);
    } else {
        servoChannel_ = step.channel;
        servoAngle_ = step.value;
        if (step.type == MechanismStepType::Gripper) {
            servoChannel_ = 2;
            servoAngle_ = step.value ? action.initial.gripperOpenDeg : action.initial.gripperCloseDeg;
        } else if (step.type == MechanismStepType::Platform) {
            servoChannel_ = 3; servoAngle_ = action.initial.platformDeg[step.value - 1];
        }
        const int speed = servoChannel_ == 2 ? action.initial.gripperDps10 :
            servoChannel_ == 3 ? action.initial.platformDps10 : action.initial.pose.turretDps10;
        if (servoChannel_ == 4) {
            /* Keep the firmware's turret estimate consistent with later poses. */
            expectedPose_ = actionPose_;
            expectedPose_.turretDdeg = servoAngle_ * 10;
            expectedPose_.turretDps10 = speed;
            const auto &p = expectedPose_;
            stage_ = Stage::Pose;
            send(QStringLiteral("mech pose %1 %2 %3 %4 %5 %6 %7 %8")
                .arg(p.horizontalDmm).arg(p.liftDmm).arg(p.turretDdeg).arg(p.horizontalRpm)
                .arg(p.horizontalAccel).arg(p.liftRpm).arg(p.liftAccel).arg(p.turretDps10), true, 300000);
            return;
        }
        stage_ = Stage::Servo;
        send(QStringLiteral("servo %1 %2 %3").arg(servoChannel_).arg(servoAngle_).arg(speed), false, 370000);
    }
}

void RouteSequenceRunner::handleLine(const QString &line) {
    if (!running_) return;
    if (line.startsWith("ERR") || line.startsWith("VISION ERROR") || line.startsWith("MECH INVALID") ||
        line.startsWith("STOP") || line.startsWith("EMERGENCY STOP") || line.startsWith("ROUTE INVALID")) {
        abort(line); return;
    }
    if (index_ >= end_) return;
    const auto &step = sequence_.steps[index_];
    if (stage_ == Stage::Move) {
        const QString suffix = QStringLiteral("dir=%1 distance_mm=%2").arg(step.direction).arg(step.distanceMm);
        if (line.startsWith("MOVE RUN " + suffix + " rpm=")) seenStart_ = true;
        if (seenStart_ && line == "MOVE DONE " + suffix) advance();
    } else if (stage_ == Stage::Turn) {
        if (line == "TURN running") seenStart_ = true;
        if (seenStart_ && line.startsWith("TURN DONE ")) advance();
    } else if (stage_ == Stage::Scale) {
        const QString expected = step.ring ? QStringLiteral("VISION SCALE ring=%1 forward_milli=%2 right_milli=%3")
            .arg(step.target).arg(step.forwardMilli).arg(step.rightMilli) :
            QStringLiteral("VISION SCALE material forward_milli=%1 right_milli=%2").arg(step.forwardMilli).arg(step.rightMilli);
        if (line == expected) {
            timeout_.stop();
            stage_ = Stage::Vision;
            send(QStringLiteral("vision align %1 %2").arg(step.ring ? "ring" : "material").arg(step.target), true, 5000);
        }
    } else if (stage_ == Stage::Vision) {
        const QString identity = QStringLiteral("mode=%1 selector=%2 target=%3")
            .arg(step.ring ? "RING" : "MATERIAL").arg(step.ring ? 0 : step.target).arg(step.ring ? step.target : 0);
        if (line.startsWith("VISION STATE ") && line.contains(identity + " iteration=")) {
            seenStart_ = true;
            timeout_.stop(); // Camera retries are bounded in firmware; absence of material is not.
        }
        if (seenStart_ && line.startsWith("VISION DONE forward_mm=")) advance();
        if (line == "VISION PAUSED" || line.contains("state=FAILED")) abort(QStringLiteral("视觉对准已停止"));
    } else if (stage_ == Stage::InitialStatus) {
        const auto &initial = step.action.initial;
        const auto &p = initial.pose;
        if (line.startsWith("MECH RUN ")) { abort(QStringLiteral("机构仍在运动")); return; }
        if (line.startsWith("MECH POS ")) {
            const QString expected = QStringLiteral("MECH POS h=%1 l=%2 t=%3").arg(p.horizontalDmm).arg(p.liftDmm).arg(p.turretDdeg);
            if (line != expected) { abort(QStringLiteral("机构姿态与动作组初始状态不一致，请先准备机构")); return; }
            initialPoseReceived_ = true;
        }
        if (line.startsWith("MECH SERVOS ")) {
            const QString expected = QStringLiteral("MECH SERVOS gripper_mdeg=%1 platform_mdeg=%2 turret_mdeg=%3 moving=0")
                .arg((initial.gripperOpen ? initial.gripperOpenDeg : initial.gripperCloseDeg) * 1000)
                .arg(initial.platformDeg[initial.platform - 1] * 1000).arg(p.turretDdeg * 100);
            if (line != expected) { abort(QStringLiteral("夹爪或平台与动作组初始状态不一致，请先准备机构")); return; }
            initialServosReceived_ = true;
        }
        if (initialPoseReceived_ && initialServosReceived_) { timeout_.stop(); stage_ = Stage::Idle; defer(true); }
    } else if (stage_ == Stage::Pose) {
        const auto &p = expectedPose_;
        const QString fields = QStringLiteral("h=%1 l=%2 t=%3").arg(p.horizontalDmm).arg(p.liftDmm).arg(p.turretDdeg);
        if (line == "MECH RUN " + fields) seenStart_ = true;
        if (seenStart_ && line == "MECH DONE " + fields) { actionPose_ = p; actionDone(); }
    } else if (stage_ == Stage::Servo) {
        const QString fields = QStringLiteral("servo=%1 angle=%2").arg(servoChannel_).arg(servoAngle_);
        if (line.startsWith(QStringLiteral("MOVING servo=%1 target=%2 ").arg(servoChannel_).arg(servoAngle_))) seenStart_ = true;
        if ((seenStart_ && line == "DONE " + fields) || line.startsWith("OK " + fields + " ")) actionDone();
    }
}

void RouteSequenceRunner::actionDone() {
    timeout_.stop();
    stage_ = Stage::Wait;
    wait_.start(sequence_.steps[index_].action.steps[actionIndex_].waitMs);
}

void RouteSequenceRunner::advance() {
    timeout_.stop(); wait_.stop();
    stage_ = Stage::Idle;
    ++index_;
    defer(false);
}

void RouteSequenceRunner::defer(bool action) {
    const auto generation = generation_;
    QTimer::singleShot(0, this, [this, generation, action] {
        if (!running_ || generation != generation_) return;
        if (action) dispatchAction(); else dispatchStep();
    });
}

void RouteSequenceRunner::confirm() {
    if (!awaitingConfirmation()) return;
    emit confirmationChanged(false);
    advance();
}

void RouteSequenceRunner::finish(bool successful, const QString &error) {
    if (!running_) return;
    running_ = false; ++generation_;
    timeout_.stop(); wait_.stop(); stage_ = Stage::Idle;
    client_->endRouteExecution();
    emit confirmationChanged(false);
    emit runningChanged(false);
    if (successful) emit completed();
    else emit failed(error);
}

void RouteSequenceRunner::abort(const QString &error) {
    if (!running_) return;
    finish(false, error);
    if (client_->connected()) client_->sendCommand(QStringLiteral("stop"));
}

void RouteSequenceRunner::stop() { abort(QStringLiteral("路线已停止")); }
void RouteSequenceRunner::disconnected() { finish(false, QStringLiteral("连接已断开，路线已取消")); }
