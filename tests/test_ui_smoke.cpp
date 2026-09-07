#include <QApplication>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include <iostream>
#include <utility>

#include "app/MainWindow.h"
#include "pages/ChassisPage.h"
#include "pages/ImuPage.h"
#include "pages/MechanismPage.h"
#include "pages/OverviewPage.h"
#include "widgets/TelemetryPlot.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow window;
    auto *nav = window.findChild<QListWidget *>("navigationList");
    auto *portCombo = window.findChild<QComboBox *>("portCombo");
    auto *baudCombo = window.findChild<QComboBox *>("baudCombo");
    auto *refreshPortsButton = window.findChild<QPushButton *>("refreshPortsButton");
    auto *connectButton = window.findChild<QPushButton *>("connectButton");
    auto *connectionStatusLabel =
        window.findChild<QLabel *>("connectionStatusLabel");
    auto *stop = window.findChild<QPushButton *>("emergencyStopButton");
    auto *tuningPage = window.findChild<QWidget *>("底盘与 PID");
    auto *mechanismPage = window.findChild<QWidget *>("机械臂与舵机");
    auto *actionPage = window.findChild<QWidget *>("动作测试");
    auto *pidProfile = window.findChild<QComboBox *>("pidProfileCombo");
    auto *pidKp = window.findChild<QDoubleSpinBox *>("pidKpSpinBox");
    auto *pidWrite = window.findChild<QPushButton *>("pidWriteButton");
    auto *chassisRead = window.findChild<QPushButton *>("chassisReadButton");
    auto *mechanismWrite =
        window.findChild<QPushButton *>("mechanismWriteButton");
    auto *imuRate = window.findChild<QAbstractSpinBox *>("imuRateSpinBox");
    auto *ramOnlyNotice = window.findChild<QLabel *>("ramOnlyNotice");
    auto *imuPage = window.findChild<ImuPage *>();
    auto *overviewPage = window.findChild<QWidget *>("总览");
    auto *overviewLink =
        window.findChild<QLabel *>("overviewLinkStateLabel");
    auto *imuTelemetryRate =
        window.findChild<QSpinBox *>("imuTelemetryRateSpinBox");
    if (!require(nav && nav->count() == 7, "navigation pages changed") ||
        !require(portCombo && baudCombo && refreshPortsButton && connectButton,
                 "connection controls are missing") ||
        !require(connectionStatusLabel && stop && !stop->isEnabled(),
                 "connection status controls are invalid") ||
        !require(tuningPage && !tuningPage->isEnabled() && mechanismPage &&
                     !mechanismPage->isEnabled(),
                 "parameter pages must be disabled before handshake") ||
        !require(actionPage && !actionPage->isEnabled(),
                 "action page must stay disabled before handshake") ||
        !require(imuPage && !imuPage->isEnabled() && overviewPage &&
                     overviewLink &&
                     overviewLink->text() == QStringLiteral("未连接"),
                 "overview or IMU telemetry page is not wired") ||
        !require(imuTelemetryRate && imuTelemetryRate->minimum() == 1 &&
                     imuTelemetryRate->maximum() == 50 &&
                     !imuTelemetryRate->isEnabled(),
                 "IMU telemetry rate control is invalid") ||
        !require(baudCombo->count() == 2 &&
                     connectionStatusLabel->text() == QStringLiteral("未连接"),
                 "connection defaults changed") ||
        !require(pidProfile && pidProfile->count() == 5,
                 "PID profile selector must expose five profiles") ||
        !require(pidKp && pidKp->minimum() == 0.0 && pidKp->maximum() == 20.0,
                 "PID Kp bounds must come from the catalog") ||
        !require(pidWrite && !pidWrite->isEnabled() && chassisRead &&
                     !chassisRead->isEnabled() && mechanismWrite &&
                     !mechanismWrite->isEnabled() && imuRate &&
                     !imuRate->isEnabled(),
                 "parameter controls must be disabled before handshake") ||
        !require(ramOnlyNotice &&
                     ramOnlyNotice->text().contains(QStringLiteral("RAM")) &&
                     ramOnlyNotice->text().contains(QStringLiteral("Flash")),
                 "RAM-only notice is missing")) {
        return 1;
    }

    ChassisPage chassis;
    auto *pageRead = chassis.findChild<QPushButton *>("chassisReadButton");
    auto *pageWrite = chassis.findChild<QPushButton *>("chassisWriteButton");
    auto *pidSelector = chassis.findChild<QComboBox *>("pidProfileCombo");
    auto *pidControl = chassis.findChild<QDoubleSpinBox *>("pidKpSpinBox");
    auto *pidPageWrite = chassis.findChild<QPushButton *>("pidWriteButton");
    auto *vx = chassis.findChild<QSpinBox *>("chassisVxSpinBox");
    auto *restore = chassis.findChild<QPushButton *>("restoreInitialButton");
    int readGroup = 0;
    int writeGroup = 0;
    QVector<ParameterValue> writtenValues;
    QObject::connect(&chassis, &ChassisPage::readRequested,
                     [&](quint8 group) { readGroup = group; });
    QObject::connect(&chassis, &ChassisPage::writeRequested,
                     [&](quint8 group, QVector<ParameterValue> values) {
                         writeGroup = group;
                         writtenValues = std::move(values);
                     });
    if (!require(pageRead && pageWrite && pidSelector && pidControl &&
                     pidPageWrite && vx && restore && !pageRead->isEnabled() &&
                     !pageWrite->isEnabled(),
                 "standalone parameter page is not locked before handshake")) {
        return 1;
    }
    chassis.setConnected(true);
    if (!require(pageRead->isEnabled() && pageWrite->isEnabled(),
                 "handshake did not enable parameter actions")) {
        return 1;
    }
    chassis.setValues({
        {0x2000, ValueType::Int32, QVariant(qint32(12))},
        {0x2001, ValueType::Int32, QVariant(qint32(-4))},
        {0x2002, ValueType::Int32, QVariant(qint32(7))},
        {0x2003, ValueType::UInt16, QVariant(quint16(250))},
        {0x2004, ValueType::UInt16, QVariant(quint16(15))},
    });
    if (!require(!restore->isEnabled(),
                 "an unsolicited chassis response created connection initial values")) {
        return 1;
    }
    chassis.setConnected(false);
    chassis.setValues({
        {0x2000, ValueType::Int32, QVariant(qint32(12))},
    });
    if (!require(!restore->isEnabled(),
                 "a disconnected late chassis response created connection initial values")) {
        return 1;
    }
    chassis.setConnected(true);
    chassis.setValues({
        {0x1000, ValueType::Float32, QVariant(1.5)},
        {0x1001, ValueType::Float32, QVariant(0.25)},
        {0x1002, ValueType::Float32, QVariant(0.5)},
        {0x1003, ValueType::Float32, QVariant(4.0)},
        {0x1004, ValueType::Float32, QVariant(20.0)},
    });
    pidSelector->setCurrentIndex(1);
    if (!require(pidControl->value() == 0.0,
                 "switching PID profiles copied the previous profile values")) {
        return 1;
    }
    pidControl->setValue(2.5);
    pidPageWrite->click();
    if (!require(writeGroup == 0x10 && !writtenValues.isEmpty() &&
                     writtenValues.front().id == 0x1010,
                 "PID write did not use the selected profile ID range")) {
        return 1;
    }
    pidSelector->setCurrentIndex(0);
    if (!require(pidControl->value() == 1.5,
                 "switching back to PID profile 0 lost its readback value")) {
        return 1;
    }
    chassis.setConnected(false);
    chassis.setConnected(true);
    pageWrite->click();
    if (!require(writeGroup == 0x20 && writtenValues.size() == 5,
                 "an enabled page did not emit a chassis RAM write")) {
        return 1;
    }
    const QVector<ParameterValue> initialChassisValues = {
        {0x2000, ValueType::Int32, QVariant(qint32(12))},
        {0x2001, ValueType::Int32, QVariant(qint32(-4))},
        {0x2002, ValueType::Int32, QVariant(qint32(7))},
        {0x2003, ValueType::UInt16, QVariant(quint16(250))},
        {0x2004, ValueType::UInt16, QVariant(quint16(15))},
    };
    chassis.setValues(initialChassisValues);
    if (!require(!restore->isEnabled(),
                 "a write response was incorrectly treated as connection initial values")) {
        return 1;
    }
    writeGroup = 0;
    writtenValues.clear();
    pageRead->click();
    if (!require(readGroup == 0x20, "read button emitted the wrong group")) {
        return 1;
    }
    chassis.setValues(initialChassisValues);
    vx->setValue(30);
    if (!require(restore->isEnabled(),
                 "a successful read did not enable the connection snapshot") ) {
        return 1;
    }
    restore->click();
    if (!require(vx->value() == 12,
                 "restoring connection initial values changed the wrong value") ||
        !require(writeGroup == 0 && writtenValues.isEmpty(),
                 "restoring values transmitted without an explicit write")) {
        return 1;
    }
    pageWrite->click();
    if (!require(writeGroup == 0x20 && writtenValues.size() == 5,
                 "RAM write did not emit the complete chassis group")) {
        return 1;
    }

    MechanismPage mechanism;
    auto *horizontalUnit =
        mechanism.findChild<QLabel *>("horizontalPositionSpinBoxUnit");
    auto *mechanismPageWrite =
        mechanism.findChild<QPushButton *>("mechanismWriteButton");
    auto *mechanismRead =
        mechanism.findChild<QPushButton *>("mechanismReadButton");
    auto *mechanismRestore =
        mechanism.findChild<QPushButton *>("mechanismRestoreInitialButton");
    if (!require(horizontalUnit && horizontalUnit->text() == QStringLiteral("mm") &&
                     mechanismPageWrite && !mechanismPageWrite->isEnabled() &&
                     mechanismRead && mechanismRestore,
                 "mechanism units or handshake lock are missing")) {
        return 1;
    }

    ImuPage imu;
    auto *imuRateControl =
        imu.findChild<QSpinBox *>("imuTelemetryRateSpinBox");
    auto *imuError = imu.findChild<QLabel *>("imuTelemetryErrorLabel");
    auto *imuAcceleration =
        imu.findChild<QLabel *>("imuAccelerationXLabel");
    auto *accelerationPlot = imu.findChild<TelemetryPlot *>("accelerationPlot");
    int requestedRate = 0;
    QObject::connect(&imu, &ImuPage::telemetryRateChanged,
                     [&](quint16 rate) { requestedRate = rate; });
    if (!require(imuRateControl && imuError && imuAcceleration &&
                     accelerationPlot && !imuRateControl->isEnabled(),
                 "standalone IMU page controls are invalid") ) {
        return 1;
    }
    imu.setLinkBaudRate(9600);
    if (!require(imuRateControl->maximum() == 20,
                 "9600-baud IMU rate was not limited to 20 Hz")) {
        return 1;
    }
    imu.setTelemetryConfiguration(0x07, 25);
    if (!require(imu.findChild<QLabel *>("imuTelemetryRateStatusLabel")
                     ->text()
                     .contains(QStringLiteral("40 Hz")),
                 "IMU page did not display the device's actual rate")) {
        return 1;
    }
    imu.setConnected(true);
    imuRateControl->setValue(10);
    if (!require(requestedRate == 10,
                 "IMU rate control did not emit the requested frequency")) {
        return 1;
    }
    ImuSample imuSample;
    imuSample.timestampMs = 1234;
    imuSample.accelerationX = 1.25;
    imuSample.accelerationY = -2.5;
    imuSample.accelerationZ = 0.5;
    imuSample.angularVelocityX = 4.0;
    imuSample.angularVelocityY = -5.0;
    imuSample.angularVelocityZ = 6.0;
    imuSample.rollDegrees = 7.0;
    imuSample.pitchDegrees = -8.0;
    imuSample.yawDegrees = 9.0;
    imu.setImuSample(imuSample);
    if (!require(imuAcceleration->text().contains(QStringLiteral("1.250")) &&
                     imuAcceleration->text().contains(QStringLiteral("g")),
                 "IMU acceleration value or unit was not displayed") ||
        !require(accelerationPlot->sampleCount() == 1,
                 "IMU acceleration was not appended to the plot")) {
        return 1;
    }
    imu.setCalibrationState(1);
    if (!require(imu.findChild<QLabel *>("imuCalibrationStatusLabel")
                     ->text()
                     .contains(QStringLiteral("完成")),
                 "IMU calibration result was not displayed")) {
        return 1;
    }
    imu.setDeviceError(QStringLiteral("设备错误 0x04"));
    if (!require(imuError->text().contains(QStringLiteral("0x04")),
                 "IMU error message was not displayed")) {
        return 1;
    }

    OverviewPage overview;
    DeviceInfo info;
    info.protocolVersion = 1;
    info.firmwareMajor = 2;
    info.firmwareMinor = 3;
    info.firmwarePatch = 4;
    overview.setDeviceInfo(info);
    overview.setStatus(DeviceStatus{2, 1, 0, 0x000a, 1});
    overview.setImuSample(imuSample);
    if (!require(overview.findChild<QLabel *>("firmwareVersionLabel")
                     ->text() == QStringLiteral("v2.3.4") &&
                     overview.findChild<QLabel *>("emergencyStateLabel")
                         ->text()
                         .contains(QStringLiteral("急停")),
                 "overview status fields were not displayed") ) {
        return 1;
    }
    mechanism.setConnected(true);
    const QVector<ParameterValue> mechanismValues = {
        {0x3000, ValueType::Float32, QVariant(10.0)},
    };
    mechanism.setValues(mechanismValues);
    if (!require(!mechanismRestore->isEnabled(),
                 "an unsolicited mechanism response created connection initial values")) {
        return 1;
    }
    mechanism.setConnected(false);
    mechanism.setValues(mechanismValues);
    if (!require(!mechanismRestore->isEnabled(),
                 "a disconnected late mechanism response created connection initial values")) {
        return 1;
    }
    mechanism.setConnected(true);
    mechanismRead->click();
    mechanism.setValues(mechanismValues);
    if (!require(mechanismRestore->isEnabled(),
                 "a requested mechanism read did not create connection initial values")) {
        return 1;
    }
    return 0;
}
