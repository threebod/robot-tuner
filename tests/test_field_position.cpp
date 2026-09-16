#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QGroupBox>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSplitter>
#include <QSpinBox>
#include <QTimer>

#include <cmath>
#include <iostream>

#include "pages/FieldPositionPage.h"
#include "widgets/SerialDebugPanel.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool near(double actual, double expected) {
    return std::abs(actual - expected) < 1e-9;
}

}  // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    const QRectF screen(10.0, 20.0, 480.0, 480.0);
    const QPointF bottomLeft = FieldMapWidget::fieldToScreen({0, 0}, screen);
    const QPointF topLeft = FieldMapWidget::fieldToScreen({0, 2400}, screen);
    const QPointF bottomRight = FieldMapWidget::fieldToScreen({2400, 0}, screen);
    const QPointF topRight = FieldMapWidget::fieldToScreen({2400, 2400}, screen);
    const QPointF center = FieldMapWidget::fieldToScreen({1200, 1200}, screen);
    const QPointF zeroHeading = FieldMapWidget::headingVector(0.0);
    const QPointF heading = FieldMapWidget::headingVector(90.0);
    if (!require(bottomLeft == QPointF(10, 500),
                 "field bottom-left mapping is incorrect") ||
        !require(topLeft == QPointF(10, 20),
                 "field top-left mapping is incorrect") ||
        !require(bottomRight == QPointF(490, 500),
                 "field bottom-right mapping is incorrect") ||
        !require(topRight == QPointF(490, 20),
                 "field top-right mapping is incorrect") ||
        !require(center == QPointF(250, 260),
                 "field center mapping is incorrect") ||
        !require(near(zeroHeading.x(), 1.0) && near(zeroHeading.y(), 0.0),
                 "zero heading did not point toward +X") ||
        !require(near(heading.x(), 0.0) && near(heading.y(), -1.0),
                 "screen heading did not flip the field Y axis")) {
        return 1;
    }

    FieldPositionPage page;
    auto *preset1 = page.findChild<QPushButton *>("startZone1Button");
    auto *preset2 = page.findChild<QPushButton *>("startZone2Button");
    auto *apply = page.findChild<QPushButton *>("poseApplyButton");
    auto *simulation = page.findChild<QCheckBox *>("localSimulationCheckBox");
    auto *x = page.findChild<QDoubleSpinBox *>("poseXSpinBox");
    auto *y = page.findChild<QDoubleSpinBox *>("poseYSpinBox");
    auto *yaw = page.findChild<QDoubleSpinBox *>("poseYawSpinBox");
    auto *source = page.findChild<QLabel *>("poseSourceLabel");
    auto *status = page.findChild<QLabel *>("poseStatusLabel");
    auto *updated = page.findChild<QLabel *>("poseUpdatedLabel");
    auto *map = page.findChild<FieldMapWidget *>("fieldMapWidget");
    auto *splitter = page.findChild<QSplitter *>("fieldMonitorSplitter");
    auto *serialPanel =
        page.findChild<SerialDebugPanel *>("fieldSerialPanel");
    auto *steering = page.findChild<QLabel *>("fieldSteeringAngleLabel");
    auto *steeringStatus = page.findChild<QLabel *>("fieldSteeringStatusLabel");
    auto *connection = page.findChild<QLabel *>("fieldConnectionStateLabel");
    auto *emergency = page.findChild<QLabel *>("fieldEmergencyStateLabel");
    auto *errorCode = page.findChild<QLabel *>("fieldErrorCodeLabel");
    auto *poseControls = page.findChild<QGroupBox *>("poseControlGroup");
    auto *navigationControls =
        page.findChild<QGroupBox *>("navigationControlGroup");
    auto *navInit1 = page.findChild<QPushButton *>("navigationInit1Button");
    auto *navMove = page.findChild<QPushButton *>("navigationMoveButton");
    auto *navRpm = page.findChild<QSpinBox *>("navigationRpmSpin");
    auto *navTarget = page.findChild<QLabel *>("navigationTargetLabel");
    auto *navState = page.findChild<QLabel *>("navigationStateLabel");
    if (!require(preset1 && preset2 && apply && simulation && x && y && yaw &&
                     source && status && updated && map && simulation->isChecked() &&
                     splitter && splitter->orientation() == Qt::Vertical &&
                     serialPanel && steering && steeringStatus && connection &&
                     emergency && errorCode && poseControls &&
                     navigationControls && navInit1 && navMove && navRpm && navTarget &&
                     navState && navigationControls->isHidden(),
                 "field position page controls are incomplete")) {
        return 1;
    }

    preset1->click();
    if (!require(near(x->value(), 2250) && near(y->value(), 2250) &&
                     near(yaw->value(), 180),
                 "start zone 1 preset is incorrect") ||
        !require(source->text().contains(QStringLiteral("本地模拟")),
                 "preset did not use the local pose display path")) {
        return 1;
    }
    preset2->click();
    if (!require(near(x->value(), 2250) && near(y->value(), 150) &&
                     near(yaw->value(), 180),
                 "start zone 2 preset is incorrect")) {
        return 1;
    }

    x->setValue(-25);
    y->setValue(2500);
    yaw->setValue(-45.5);
    apply->click();
    if (!require(status->text().contains(QStringLiteral("越界")),
                 "out-of-bounds local pose was not reported") ||
        !require(map->displayedFieldPosition() == QPointF(0, 2400),
                 "out-of-bounds icon was not clamped")) {
        return 1;
    }

    page.selectFieldPoint({600, 1800});
    if (!require(near(x->value(), 600) && near(y->value(), 1800),
                 "field click did not update pose input") ||
        !require(map->displayedFieldPosition() == QPointF(600, 1800),
                 "field click did not update the common pose display")) {
        return 1;
    }
    page.resize(900, 600);
    page.show();
    QApplication::processEvents();
    const QPointF mapCenter = map->rect().center();
    QMouseEvent click(QEvent::MouseButtonPress, mapCenter, mapCenter,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(map, &click);
    if (!require(std::abs(x->value() - 1200.0) < 8.0 &&
                     std::abs(y->value() - 1200.0) < 8.0,
                 "clicking the map center did not select the field center")) {
        return 1;
    }

    PoseSample serialPose;
    serialPose.timestampMs = 42;
    serialPose.xMm = 1200;
    serialPose.yMm = 900;
    serialPose.yawDegrees = 30.0;
    page.setPoseSample(serialPose, QStringLiteral("串口遥测"));
    if (!require(source->text().contains(QStringLiteral("串口遥测")) &&
                     status->text().contains(QStringLiteral("正常")),
                 "serial pose did not update source and status")) {
        return 1;
    }

    page.setConnectionState(QStringLiteral("设备已握手"), true);
    ImuSample imuSample;
    imuSample.timestampMs = 77;
    imuSample.yawDegrees = -35.25;
    page.setImuSample(imuSample);
    DeviceStatus deviceStatus;
    deviceStatus.emergency = 1;
    deviceStatus.lastError = 0x1234;
    page.setDeviceStatus(deviceStatus);
    if (!require(connection->text().contains(QStringLiteral("设备已握手")) &&
                     steering->text().contains(QStringLiteral("-35.25")) &&
                     steeringStatus->text().contains(QStringLiteral("正常")) &&
                     emergency->text().contains(QStringLiteral("急停")) &&
                     errorCode->text().contains(QStringLiteral("1234")),
                 "field monitor state was not updated")) {
        return 1;
    }

    QEventLoop timeoutLoop;
    QTimer::singleShot(560, &timeoutLoop, &QEventLoop::quit);
    timeoutLoop.exec();
    if (!require(status->text().contains(QStringLiteral("数据超时")),
                 "pose was not marked stale after 500 ms") ||
        !require(steeringStatus->text().contains(QStringLiteral("数据超时")),
                 "HWT101 yaw was not marked stale after 500 ms")) {
        return 1;
    }

    int initializedZone = 0;
    QPointF requestedTarget;
    quint16 requestedRpm = 0;
    QObject::connect(&page, &FieldPositionPage::navigationInitRequested,
                     [&](int zone) { initializedZone = zone; });
    QObject::connect(&page, &FieldPositionPage::navigationTargetRequested,
                     [&](qint32 targetX, qint32 targetY, quint16 rpm) {
                         requestedTarget = QPointF(targetX, targetY);
                         requestedRpm = rpm;
                     });
    page.setNavigationMode(true);
    page.setNavigationConnected(true);
    if (!require(poseControls->isHidden() && !navigationControls->isHidden() &&
                     !navMove->isEnabled(),
                 "navigation mode visibility or initial gating is incorrect")) {
        return 1;
    }
    navInit1->click();
    page.setNavigationInitialized(true);
    navRpm->setValue(120);
    page.selectFieldPoint({1160, 2050});
    if (!require(initializedZone == 1 && navMove->isEnabled() &&
                     navTarget->text().contains(QStringLiteral("1200")) &&
                     navTarget->text().contains(QStringLiteral("2080")) &&
                     map->targetFieldPosition() == QPointF(1200, 2080),
                 "map click did not snap to the nearest safe waypoint")) {
        return 1;
    }
    navMove->click();
    if (!require(requestedTarget == QPointF(1200, 2080) && requestedRpm == 120,
                 "move button did not emit the snapped target and speed")) {
        return 1;
    }
    page.setNavigationEstimate(2100, 2200, 89.5, QStringLiteral("RUN"));
    if (!require(map->displayedFieldPosition() == QPointF(2100, 2200) &&
                     navState->text().contains(QStringLiteral("移动中")) &&
                     source->text().contains(QStringLiteral("估计")) &&
                     updated->text().contains(QStringLiteral("上位机接收时间")) &&
                     steering->text().contains(QStringLiteral("89.50")) &&
                     steeringStatus->text().contains(QStringLiteral("正常")) &&
                     status->text().contains(QStringLiteral("移动中")) &&
                     !navMove->isEnabled(),
                 "live navigation estimate was not synchronized to the monitor")) {
        return 1;
    }
    page.setNavigationEstimate(1200, 2080, 90.0, QStringLiteral("IDLE"));
    page.setNavigationInitialized(false);
    if (!require(!navMove->isEnabled() &&
                     navState->text().contains(QStringLiteral("重新初始化")) &&
                     status->text().contains(QStringLiteral("位置无效")),
                 "invalid navigation pose did not disable movement")) {
        return 1;
    }
    page.setNavigationMode(false);
    if (!require(!poseControls->isHidden() && navigationControls->isHidden(),
                 "standard pose controls were not restored")) {
        return 1;
    }
    return 0;
}
