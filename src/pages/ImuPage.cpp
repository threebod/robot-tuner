#include "pages/ImuPage.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

#include <cmath>

#include "widgets/TelemetryPlot.h"

namespace {

QLabel *makeValueLabel(const QString &objectName, QWidget *parent) {
    auto *label = new QLabel(QStringLiteral("—"), parent);
    label->setObjectName(objectName);
    label->setMinimumWidth(92);
    return label;
}

QLabel *makeUnitLabel(const QString &unit, QWidget *parent) {
    auto *label = new QLabel(unit, parent);
    label->setObjectName(QStringLiteral("imuUnitLabel"));
    return label;
}

QString calibrationStateText(quint8 state) {
    switch (state) {
    case 0:
        return QStringLiteral("校准已开始");
    case 1:
        return QStringLiteral("校准完成");
    case 2:
        return QStringLiteral("校准失败");
    default:
        return QStringLiteral("校准状态异常");
    }
}

}  // namespace

ImuPage::ImuPage(QWidget *parent) : QWidget(parent) {
    auto *pageLayout = new QVBoxLayout(this);

    auto *controlGroup = new QGroupBox(QStringLiteral("遥测订阅"), this);
    auto *controlLayout = new QHBoxLayout(controlGroup);
    controlLayout->addWidget(new QLabel(QStringLiteral("IMU 频率"),
                                        controlGroup));
    rateSpinBox_ = new QSpinBox(controlGroup);
    rateSpinBox_->setObjectName(QStringLiteral("imuTelemetryRateSpinBox"));
    rateSpinBox_->setSuffix(QStringLiteral(" Hz"));
    controlLayout->addWidget(rateSpinBox_);
    rateStatusLabel_ = new QLabel(QStringLiteral("设备实际频率：—"),
                                  controlGroup);
    rateStatusLabel_->setObjectName(QStringLiteral("imuTelemetryRateStatusLabel"));
    controlLayout->addWidget(rateStatusLabel_);
    calibrationButton_ = new QPushButton(QStringLiteral("校准 IMU"),
                                          controlGroup);
    calibrationButton_->setObjectName(QStringLiteral("imuCalibrationButton"));
    controlLayout->addWidget(calibrationButton_);
    calibrationStatusLabel_ = new QLabel(QStringLiteral("未校准"),
                                         controlGroup);
    calibrationStatusLabel_->setObjectName(QStringLiteral("imuCalibrationStatusLabel"));
    controlLayout->addWidget(calibrationStatusLabel_, 1);
    pageLayout->addWidget(controlGroup);

    auto *valuesGroup = new QGroupBox(QStringLiteral("IMU 实时值"), this);
    auto *valuesLayout = new QGridLayout(valuesGroup);
    const QStringList names = {
        QStringLiteral("imuAccelerationXLabel"),
        QStringLiteral("imuAccelerationYLabel"),
        QStringLiteral("imuAccelerationZLabel"),
        QStringLiteral("imuAngularVelocityXLabel"),
        QStringLiteral("imuAngularVelocityYLabel"),
        QStringLiteral("imuAngularVelocityZLabel"),
        QStringLiteral("imuRollLabel"),
        QStringLiteral("imuPitchLabel"),
        QStringLiteral("imuYawLabel"),
    };
    const QStringList labels = {
        QStringLiteral("加速度 X"), QStringLiteral("加速度 Y"),
        QStringLiteral("加速度 Z"), QStringLiteral("角速度 X"),
        QStringLiteral("角速度 Y"), QStringLiteral("角速度 Z"),
        QStringLiteral("横滚 Roll"), QStringLiteral("俯仰 Pitch"),
        QStringLiteral("航向 Yaw"),
    };
    const QStringList units = {
        QStringLiteral("g"), QStringLiteral("g"), QStringLiteral("g"),
        QStringLiteral("°/s"), QStringLiteral("°/s"),
        QStringLiteral("°/s"), QStringLiteral("°"), QStringLiteral("°"),
        QStringLiteral("°"),
    };
    valueLabels_.reserve(names.size());
    for (int index = 0; index < names.size(); ++index) {
        const int row = index / 3;
        const int column = index % 3;
        auto *label = new QLabel(labels.at(index), valuesGroup);
        auto *value = makeValueLabel(names.at(index), valuesGroup);
        auto *unit = makeUnitLabel(units.at(index), valuesGroup);
        auto *cell = new QHBoxLayout;
        cell->setContentsMargins(0, 0, 0, 0);
        cell->addWidget(label);
        cell->addWidget(value);
        cell->addWidget(unit);
        cell->addStretch();
        valuesLayout->addLayout(cell, row, column);
        valueLabels_.push_back(value);
    }
    timestampLabel_ = new QLabel(QStringLiteral("时间戳：— ms"), valuesGroup);
    timestampLabel_->setObjectName(QStringLiteral("imuTimestampLabel"));
    valuesLayout->addWidget(timestampLabel_, 3, 0, 1, 3);
    pageLayout->addWidget(valuesGroup);

    auto *plotsLayout = new QGridLayout;
    accelerationPlot_ = new TelemetryPlot(3, 1000, this);
    accelerationPlot_->setObjectName(QStringLiteral("accelerationPlot"));
    angularVelocityPlot_ = new TelemetryPlot(3, 1000, this);
    angularVelocityPlot_->setObjectName(QStringLiteral("angularVelocityPlot"));
    attitudePlot_ = new TelemetryPlot(3, 1000, this);
    attitudePlot_->setObjectName(QStringLiteral("attitudePlot"));
    plotsLayout->addWidget(new QLabel(QStringLiteral("加速度（g）"), this), 0,
                           0);
    plotsLayout->addWidget(new QLabel(QStringLiteral("角速度（°/s）"), this),
                           0, 1);
    plotsLayout->addWidget(new QLabel(QStringLiteral("姿态角（°）"), this), 0,
                           2);
    plotsLayout->addWidget(accelerationPlot_, 1, 0);
    plotsLayout->addWidget(angularVelocityPlot_, 1, 1);
    plotsLayout->addWidget(attitudePlot_, 1, 2);
    pageLayout->addLayout(plotsLayout, 1);

    auto *pidGroup = new QGroupBox(QStringLiteral("PID 遥测"), this);
    auto *pidLayout = new QFormLayout(pidGroup);
    pidTargetLabel_ = makeValueLabel(QStringLiteral("pidTelemetryTargetLabel"),
                                     pidGroup);
    pidActualLabel_ = makeValueLabel(QStringLiteral("pidTelemetryActualLabel"),
                                     pidGroup);
    pidOutputLabel_ = makeValueLabel(QStringLiteral("pidTelemetryOutputLabel"),
                                     pidGroup);
    pidLayout->addRow(QStringLiteral("目标角度"), pidTargetLabel_);
    pidLayout->addRow(QStringLiteral("实际角度"), pidActualLabel_);
    pidLayout->addRow(QStringLiteral("输出"), pidOutputLabel_);
    pageLayout->addWidget(pidGroup);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName(QStringLiteral("imuTelemetryErrorLabel"));
    errorLabel_->setWordWrap(true);
    pageLayout->addWidget(errorLabel_);

    connect(rateSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this,
            &ImuPage::handleRateChanged);
    connect(calibrationButton_, &QPushButton::clicked, this,
            &ImuPage::requestCalibration);

    updateRateRange();
    setConnected(false);
}

void ImuPage::setConnected(bool connected) {
    connected_ = connected;
    rateSpinBox_->setEnabled(connected_);
    calibrationButton_->setEnabled(connected_);
    if (!connected_) {
        rateStatusLabel_->setText(QStringLiteral("设备实际频率：—"));
        calibrationStatusLabel_->setText(QStringLiteral("未校准"));
        errorLabel_->clear();
        accelerationPlot_->clear();
        angularVelocityPlot_->clear();
        attitudePlot_->clear();
    }
}

void ImuPage::setLinkBaudRate(qint32 baudRate) {
    baudRate_ = baudRate;
    updateRateRange();
}

void ImuPage::setTelemetryConfiguration(quint8 acceptedMask,
                                        quint16 periodMs) {
    Q_UNUSED(acceptedMask);
    if (periodMs == 0) {
        setAbnormal(QStringLiteral("设备返回了无效的遥测周期"));
        return;
    }
    const quint16 actualRate = static_cast<quint16>(qMax(
        1, qRound(1000.0 / static_cast<double>(periodMs))));
    {
        const QSignalBlocker blocker(rateSpinBox_);
        rateSpinBox_->setValue(qBound(1, static_cast<int>(actualRate),
                                      maximumRate_));
    }
    rateStatusLabel_->setText(
        QStringLiteral("设备实际频率：%1 Hz").arg(actualRate));
}

void ImuPage::setCalibrationState(quint8 state) {
    calibrationStatusLabel_->setText(calibrationStateText(state));
    if (state == 2 || state > 2) {
        setAbnormal(calibrationStateText(state));
    } else if (state == 1) {
        errorLabel_->clear();
    }
}

void ImuPage::setImuSample(const ImuSample &sample) {
    setValue(valueLabels_.value(0), sample.accelerationX, QStringLiteral("g"),
             std::abs(sample.accelerationX) > 16.0);
    setValue(valueLabels_.value(1), sample.accelerationY, QStringLiteral("g"),
             std::abs(sample.accelerationY) > 16.0);
    setValue(valueLabels_.value(2), sample.accelerationZ, QStringLiteral("g"),
             std::abs(sample.accelerationZ) > 16.0);
    setValue(valueLabels_.value(3), sample.angularVelocityX,
             QStringLiteral("°/s"), std::abs(sample.angularVelocityX) > 2000.0);
    setValue(valueLabels_.value(4), sample.angularVelocityY,
             QStringLiteral("°/s"), std::abs(sample.angularVelocityY) > 2000.0);
    setValue(valueLabels_.value(5), sample.angularVelocityZ,
             QStringLiteral("°/s"), std::abs(sample.angularVelocityZ) > 2000.0);
    setValue(valueLabels_.value(6), sample.rollDegrees, QStringLiteral("°"),
             std::abs(sample.rollDegrees) > 180.0);
    setValue(valueLabels_.value(7), sample.pitchDegrees, QStringLiteral("°"),
             std::abs(sample.pitchDegrees) > 180.0);
    setValue(valueLabels_.value(8), sample.yawDegrees, QStringLiteral("°"),
             std::abs(sample.yawDegrees) > 180.0);
    timestampLabel_->setText(
        QStringLiteral("时间戳：%1 ms").arg(sample.timestampMs));
    accelerationPlot_->append(sample.timestampMs,
                              {sample.accelerationX, sample.accelerationY,
                               sample.accelerationZ});
    angularVelocityPlot_->append(sample.timestampMs,
                                 {sample.angularVelocityX,
                                  sample.angularVelocityY,
                                  sample.angularVelocityZ});
    attitudePlot_->append(sample.timestampMs,
                          {sample.rollDegrees, sample.pitchDegrees,
                           sample.yawDegrees});
}

void ImuPage::setPidSample(const PidSample &sample) {
    setValue(pidTargetLabel_, sample.targetDegrees, QStringLiteral("°"));
    setValue(pidActualLabel_, sample.actualDegrees, QStringLiteral("°"));
    setValue(pidOutputLabel_, sample.output, QStringLiteral("unit"));
}

void ImuPage::setStatus(const DeviceStatus &status) {
    if (status.emergency != 0) {
        setAbnormal(QStringLiteral("设备处于急停状态"));
    } else if (status.lastError != 0) {
        setAbnormal(QStringLiteral("设备状态错误码：0x%1")
                        .arg(status.lastError, 4, 16, QLatin1Char('0')));
    }
}

void ImuPage::setDeviceError(const QString &message) {
    setAbnormal(message);
}

void ImuPage::requestCalibration() {
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("确认 IMU 校准"),
        QStringLiteral("校准期间请保持设备静止，确定开始吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    calibrationStatusLabel_->setText(QStringLiteral("正在请求校准…"));
    emit calibrationRequested();
}

void ImuPage::handleRateChanged(int hz) {
    if (connected_) {
        emit telemetryRateChanged(static_cast<quint16>(hz));
    }
}

void ImuPage::setValue(QLabel *label, double value, const QString &unit,
                       bool abnormal) {
    if (label == nullptr) {
        return;
    }
    if (!std::isfinite(value)) {
        label->setText(QStringLiteral("异常 (%1)").arg(unit));
        label->setStyleSheet(QStringLiteral("color: #b00020;"));
        setAbnormal(QStringLiteral("遥测包含非有限数值"));
        return;
    }
    label->setText(QStringLiteral("%1 %2").arg(value, 0, 'f', 3).arg(unit));
    label->setStyleSheet(abnormal ? QStringLiteral("color: #b00020;")
                                  : QString());
    if (abnormal) {
        setAbnormal(QStringLiteral("IMU 遥测值超出量程"));
    }
}

void ImuPage::setAbnormal(const QString &message) {
    errorLabel_->setText(QStringLiteral("异常：%1").arg(message));
    errorLabel_->setStyleSheet(QStringLiteral("color: #b00020;"));
}

void ImuPage::updateRateRange() {
    maximumRate_ = 50;
    const QSignalBlocker blocker(rateSpinBox_);
    rateSpinBox_->setRange(1, maximumRate_);
    if (rateSpinBox_->value() > maximumRate_) {
        rateSpinBox_->setValue(maximumRate_);
    }
}
