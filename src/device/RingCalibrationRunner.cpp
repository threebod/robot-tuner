#include "device/RingCalibrationRunner.h"
#include "device/MecanumJogClient.h"

#include <QRegularExpression>
#include <QStringList>

RingCalibrationRunner::RingCalibrationRunner(MecanumJogClient *client, QObject *parent)
    : QObject(parent), client_(client) {
    timeout_.setSingleShot(true);
    settle_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this,
            [this] { abort(QStringLiteral("标定回复超时；已停止，请检查固件版本与摄像头连接")); });
    connect(&settle_, &QTimer::timeout, this, [this] {
        if (running_ && stage_ == Stage::Settle) { ++sampleIndex_; requestSample(); }
    });
    connect(client_, &MecanumJogClient::lineReceived, this, &RingCalibrationRunner::handleLine);
    connect(client_, &MecanumJogClient::connectionChanged, this, [this](bool connected) {
        if (!connected) finish(false, QStringLiteral("连接断开，标定已取消"));
    });
    connect(client_, &MecanumJogClient::commandFailed, this, &RingCalibrationRunner::abort);
    connect(client_, &MecanumJogClient::stopRequested, this,
            [this] { finish(false, QStringLiteral("标定已停止")); });
}

bool RingCalibrationRunner::start() { return startCalibration(0); }

bool RingCalibrationRunner::startMaterial(int color) {
    if (color < 1 || color > 6) return false;
    return startCalibration(color);
}

bool RingCalibrationRunner::startCalibration(int color) {
    if (running_) return false;
    if (client_->avoidanceRunning() || !client_->beginRouteExecution()) {
        emit failed(QStringLiteral("请连接文本串口，并等待其他运动与视觉流程结束"));
        return false;
    }
    running_ = true;
    materialColor_ = color;
    ++generation_;
    sampleIndex_ = 0;
    samples_.fill(QPointF());
    emit runningChanged(true);
    requestSample();
    return running_;
}

bool RingCalibrationRunner::send(const QString &command, bool armed, int timeoutMs) {
    timeout_.start(timeoutMs);
    if (!client_->sendRouteCommand(command, armed)) {
        abort(QStringLiteral("标定指令发送失败"));
        return false;
    }
    return running_;
}

void RingCalibrationRunner::requestSample() {
    if (!running_) return;
    stage_ = Stage::Sample;
    if (++sampleId_ == 0) ++sampleId_;
    emit progressChanged(QStringLiteral("标定：读取%1稳定中心（%2/5）")
                             .arg(material() ? QStringLiteral("物料颜色 %1 ").arg(materialColor_)
                                             : QStringLiteral("圆环 2 ")).arg(sampleIndex_ + 1));
    send(material() ? QStringLiteral("vision sample material %1 %2").arg(materialColor_).arg(sampleId_)
                    : QStringLiteral("vision sample ring 2 %1").arg(sampleId_), false, 6000);
}

void RingCalibrationRunner::move() {
    if (!running_ || stage_ != Stage::Ready) return;
    static const int forward[4] = {20, -20, 0, 0};
    static const int right[4] = {0, 0, 20, -20};
    const QStringList descriptions{QStringLiteral("前移 20 mm"), QStringLiteral("后移返回"),
                                   QStringLiteral("右移 20 mm"), QStringLiteral("左移返回")};
    stage_ = Stage::Move;
    seenMove_ = false;
    emit progressChanged(QStringLiteral("标定：%1，完成后停稳 300 ms").arg(descriptions[sampleIndex_]));
    send(QStringLiteral("vision jog %1 %2 15").arg(forward[sampleIndex_]).arg(right[sampleIndex_]),
         true, 10000);
}

void RingCalibrationRunner::defer(bool apply) {
    const auto generation = generation_;
    QTimer::singleShot(0, this, [this, generation, apply] {
        if (!running_ || generation_ != generation) return;
        if (apply) {
            send(QStringLiteral("vision scale %1 %2 %3").arg(material() ? "material" : "ring 2")
                     .arg(forwardMilli_).arg(rightMilli_),
                 true, 5000);
        } else move();
    });
}

void RingCalibrationRunner::handleLine(const QString &line) {
    if (!running_) return;
    if (line.startsWith("ERR") || line.startsWith("VISION ERROR") ||
        line.startsWith("MECH INVALID") || line.startsWith("ROUTE INVALID") ||
        line.startsWith("STOP") || line.startsWith("EMERGENCY STOP")) {
        abort(line);
        return;
    }
    if (stage_ == Stage::Sample) {
        const QRegularExpression measure(QStringLiteral(
            "^VISION MEASURE id=(\\d+) %1 token=(\\d+) du=(-?\\d+) dv=(-?\\d+) quality=(\\d+)$")
                .arg(material() ? QStringLiteral("material=%1").arg(materialColor_) : QStringLiteral("ring=2")));
        const auto match = measure.match(line);
        if (!match.hasMatch() || match.captured(1).toUInt() != sampleId_) return;
        const int du = match.captured(3).toInt(), dv = match.captured(4).toInt();
        if ((!material() && match.captured(5).toInt() < 60) || du < -160 || du > 159 || dv < -120 || dv > 119) {
            abort(QStringLiteral("视觉样本质量或坐标异常，未应用比例"));
            return;
        }
        timeout_.stop();
        samples_[sampleIndex_] = QPointF(du, dv);
        if (sampleIndex_ < 4) {
            stage_ = Stage::Ready;
            defer(false);
        } else if (calculateScales()) {
            stage_ = Stage::Apply;
            emit scalesCalculated(forwardMilli_, rightMilli_);
            emit progressChanged(QStringLiteral("标定：比例已计算，等待主控应用确认"));
            defer(true);
        }
    } else if (stage_ == Stage::Move) {
        if (line == "VISION STATE state=MOVE mode=MANUAL selector=0 target=0 iteration=0")
            seenMove_ = true;
        if (seenMove_ && line == "VISION PAUSED") {
            timeout_.stop();
            stage_ = Stage::Settle;
            settle_.start(300);
        }
    } else if (stage_ == Stage::Apply &&
               line == QStringLiteral("VISION SCALE %1 forward_milli=%2 right_milli=%3")
                           .arg(material() ? "material" : "ring=2").arg(forwardMilli_).arg(rightMilli_)) {
        finish(true);
    }
}

bool RingCalibrationRunner::calculateScales() {
    const QPointF forwardOut = samples_[1] - samples_[0];
    const QPointF forwardBack = samples_[1] - samples_[2];
    const QPointF rightOut = samples_[3] - samples_[2];
    const QPointF rightBack = samples_[3] - samples_[4];
    const double forwardPixels = (forwardOut.x() + forwardBack.x()) / 2.0;
    const double rightPixels = (rightOut.y() + rightBack.y()) / 2.0;
    const QPointF forwardDrift = samples_[2] - samples_[0];
    const QPointF rightDrift = samples_[4] - samples_[2];
    if (forwardPixels < 5 || rightPixels < 5 || forwardOut.x() <= 0 || forwardBack.x() <= 0 ||
        rightOut.y() <= 0 || rightBack.y() <= 0) {
        abort(QStringLiteral("像素变化过小或方向与当前映射不符，未应用比例"));
        return false;
    }
    if (qAbs(forwardDrift.x()) > 3 || qAbs(forwardDrift.y()) > 3 ||
        qAbs(rightDrift.x()) > 3 || qAbs(rightDrift.y()) > 3) {
        abort(QStringLiteral("往返回位偏差超过 3 像素：前后 (%1, %2)，左右 (%3, %4)；未应用比例")
                  .arg(forwardDrift.x()).arg(forwardDrift.y()).arg(rightDrift.x()).arg(rightDrift.y()));
        return false;
    }
    if (qAbs((forwardOut.y() + forwardBack.y()) / 2.0) > qMax(3.0, forwardPixels * 0.2) ||
        qAbs((rightOut.x() + rightBack.x()) / 2.0) > qMax(3.0, rightPixels * 0.2)) {
        abort(QStringLiteral("往返平均两轴耦合超过门槛，请检查摄像头姿态；未应用比例"));
        return false;
    }
    forwardMilli_ = qRound(20000.0 / forwardPixels);
    rightMilli_ = qRound(20000.0 / rightPixels);
    if (forwardMilli_ < 50 || forwardMilli_ > 2000 || rightMilli_ < 50 || rightMilli_ > 2000) {
        abort(QStringLiteral("计算比例超出 0.050～2.000 mm/像素，未应用"));
        return false;
    }
    return true;
}

void RingCalibrationRunner::finish(bool successful, const QString &reason) {
    if (!running_) return;
    running_ = false;
    ++generation_;
    timeout_.stop();
    settle_.stop();
    stage_ = Stage::Idle;
    client_->endRouteExecution();
    emit runningChanged(false);
    if (successful) emit completed();
    else emit failed(reason);
}

void RingCalibrationRunner::abort(const QString &reason) {
    if (!running_) return;
    finish(false, reason);
    if (client_->connected()) client_->sendCommand(QStringLiteral("stop"));
}

void RingCalibrationRunner::stop() { abort(QStringLiteral("标定已取消；当前位置需重新确认")); }
