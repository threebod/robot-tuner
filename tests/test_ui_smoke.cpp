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
#include "pages/MechanismPage.h"

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
