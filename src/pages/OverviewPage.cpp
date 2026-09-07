#include "pages/OverviewPage.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <cmath>

namespace {

QLabel *valueLabel(const QString &objectName, QWidget *parent) {
    auto *label = new QLabel(QStringLiteral("—"), parent);
    label->setObjectName(objectName);
    return label;
}

QString modeText(quint8 mode) {
    switch (mode) {
    case 0:
        return QStringLiteral("空闲");
    case 1:
        return QStringLiteral("运行");
    case 2:
        return QStringLiteral("调试");
    default:
        return QStringLiteral("未知 (0x%1)").arg(mode, 2, 16,
                                                    QLatin1Char('0'));
    }
}

}  // namespace

OverviewPage::OverviewPage(QWidget *parent) : QWidget(parent) {
    auto *pageLayout = new QVBoxLayout(this);

    auto *deviceGroup = new QGroupBox(QStringLiteral("设备信息"), this);
    auto *deviceLayout = new QFormLayout(deviceGroup);
    protocolVersionLabel_ = valueLabel(QStringLiteral("protocolVersionLabel"),
                                       deviceGroup);
    firmwareVersionLabel_ = valueLabel(QStringLiteral("firmwareVersionLabel"),
                                       deviceGroup);
    deviceLayout->addRow(QStringLiteral("协议版本"), protocolVersionLabel_);
    deviceLayout->addRow(QStringLiteral("固件版本"), firmwareVersionLabel_);
    pageLayout->addWidget(deviceGroup);

    auto *linkGroup = new QGroupBox(QStringLiteral("链路与状态"), this);
    auto *linkLayout = new QFormLayout(linkGroup);
    linkStateLabel_ = valueLabel(QStringLiteral("overviewLinkStateLabel"),
                                 linkGroup);
    latencyLabel_ = valueLabel(QStringLiteral("latencyLabel"), linkGroup);
    modeLabel_ = valueLabel(QStringLiteral("modeLabel"), linkGroup);
    debugStateLabel_ = valueLabel(QStringLiteral("debugStateLabel"), linkGroup);
    lockedStateLabel_ = valueLabel(QStringLiteral("lockedStateLabel"), linkGroup);
    emergencyStateLabel_ = valueLabel(QStringLiteral("emergencyStateLabel"),
                                      linkGroup);
    lastErrorCodeLabel_ = valueLabel(QStringLiteral("lastErrorCodeLabel"),
                                     linkGroup);
    linkLayout->addRow(QStringLiteral("链路状态"), linkStateLabel_);
    linkLayout->addRow(QStringLiteral("最近响应延迟"), latencyLabel_);
    linkLayout->addRow(QStringLiteral("设备模式"), modeLabel_);
    linkLayout->addRow(QStringLiteral("调试权限"), debugStateLabel_);
    linkLayout->addRow(QStringLiteral("锁定状态"), lockedStateLabel_);
    linkLayout->addRow(QStringLiteral("急停状态"), emergencyStateLabel_);
    linkLayout->addRow(QStringLiteral("设备错误码"), lastErrorCodeLabel_);
    pageLayout->addWidget(linkGroup);

    auto *imuGroup = new QGroupBox(QStringLiteral("最新姿态"), this);
    auto *imuLayout = new QFormLayout(imuGroup);
    rollLabel_ = valueLabel(QStringLiteral("overviewRollLabel"), imuGroup);
    pitchLabel_ = valueLabel(QStringLiteral("overviewPitchLabel"), imuGroup);
    yawLabel_ = valueLabel(QStringLiteral("overviewYawLabel"), imuGroup);
    imuLayout->addRow(QStringLiteral("Roll"), rollLabel_);
    imuLayout->addRow(QStringLiteral("Pitch"), pitchLabel_);
    imuLayout->addRow(QStringLiteral("Yaw"), yawLabel_);
    pageLayout->addWidget(imuGroup);

    auto *pidGroup = new QGroupBox(QStringLiteral("最新 PID 遥测"), this);
    auto *pidLayout = new QFormLayout(pidGroup);
    pidTargetLabel_ = valueLabel(QStringLiteral("overviewPidTargetLabel"),
                                 pidGroup);
    pidActualLabel_ = valueLabel(QStringLiteral("overviewPidActualLabel"),
                                 pidGroup);
    pidOutputLabel_ = valueLabel(QStringLiteral("overviewPidOutputLabel"),
                                 pidGroup);
    pidLayout->addRow(QStringLiteral("目标角度"), pidTargetLabel_);
    pidLayout->addRow(QStringLiteral("实际角度"), pidActualLabel_);
    pidLayout->addRow(QStringLiteral("输出"), pidOutputLabel_);
    pageLayout->addWidget(pidGroup);

    deviceErrorLabel_ = new QLabel(QStringLiteral("暂无设备错误"), this);
    deviceErrorLabel_->setObjectName(QStringLiteral("lastDeviceErrorLabel"));
    deviceErrorLabel_->setWordWrap(true);
    pageLayout->addWidget(deviceErrorLabel_);
    pageLayout->addStretch();

    setLinkState(QStringLiteral("未连接"));
}

void OverviewPage::setDeviceInfo(const DeviceInfo &info) {
    protocolVersionLabel_->setText(
        QStringLiteral("v%1").arg(info.protocolVersion));
    firmwareVersionLabel_->setText(
        QStringLiteral("v%1.%2.%3")
            .arg(info.firmwareMajor)
            .arg(info.firmwareMinor)
            .arg(info.firmwarePatch));
}

void OverviewPage::setLinkState(const QString &state) {
    const bool abnormal = state == QStringLiteral("未连接") ||
                          state.contains(QStringLiteral("错误"));
    setStateLabel(linkStateLabel_, state, abnormal);
}

void OverviewPage::setLatency(qint64 latencyMs) {
    if (latencyMs < 0) {
        latencyLabel_->setText(QStringLiteral("—"));
        return;
    }
    latencyLabel_->setText(QStringLiteral("%1 ms").arg(latencyMs));
}

void OverviewPage::setImuSample(const ImuSample &sample) {
    rollLabel_->setText(QStringLiteral("%1 °")
                            .arg(sample.rollDegrees, 0, 'f', 2));
    pitchLabel_->setText(QStringLiteral("%1 °")
                             .arg(sample.pitchDegrees, 0, 'f', 2));
    yawLabel_->setText(QStringLiteral("%1 °")
                           .arg(sample.yawDegrees, 0, 'f', 2));
    const bool abnormal = !std::isfinite(sample.rollDegrees) ||
                          !std::isfinite(sample.pitchDegrees) ||
                          !std::isfinite(sample.yawDegrees);
    if (abnormal) {
        setStateLabel(deviceErrorLabel_, QStringLiteral("姿态遥测数值异常"),
                      true);
    }
}

void OverviewPage::setPidSample(const PidSample &sample) {
    pidTargetLabel_->setText(QStringLiteral("%1 °")
                                 .arg(sample.targetDegrees, 0, 'f', 2));
    pidActualLabel_->setText(QStringLiteral("%1 °")
                                .arg(sample.actualDegrees, 0, 'f', 2));
    pidOutputLabel_->setText(QStringLiteral("%1 unit")
                                 .arg(sample.output, 0, 'f', 2));
}

void OverviewPage::setStatus(const DeviceStatus &status) {
    modeLabel_->setText(modeText(status.mode));
    const bool emergency = status.emergency != 0;
    const bool unlocked = status.unlocked != 0;
    setStateLabel(debugStateLabel_, unlocked ? QStringLiteral("已解锁")
                                             : QStringLiteral("已锁定"),
                  false);
    setStateLabel(lockedStateLabel_, unlocked ? QStringLiteral("未锁定")
                                              : QStringLiteral("锁定"),
                  !unlocked);
    setStateLabel(emergencyStateLabel_,
                  emergency ? QStringLiteral("急停锁定")
                            : QStringLiteral("正常"),
                  emergency);
    lastErrorCodeLabel_->setText(
        status.lastError == 0
            ? QStringLiteral("无")
            : QStringLiteral("0x%1")
                  .arg(status.lastError, 4, 16, QLatin1Char('0')));
    if (status.lastError != 0) {
        setStateLabel(deviceErrorLabel_,
                      QStringLiteral("设备状态错误码：0x%1")
                          .arg(status.lastError, 4, 16, QLatin1Char('0')),
                      true);
    }
}

void OverviewPage::setDeviceError(const QString &message) {
    setStateLabel(deviceErrorLabel_, message, true);
}

void OverviewPage::setStateLabel(QLabel *label, const QString &text,
                                 bool abnormal) {
    if (label == nullptr) {
        return;
    }
    label->setText(text);
    label->setStyleSheet(abnormal ? QStringLiteral("color: #b00020;")
                                  : QString());
}
