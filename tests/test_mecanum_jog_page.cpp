#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

#include <iostream>

#include "pages/MecanumJogPage.h"
#include "widgets/TelemetryPlot.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

template <typename T>
T *requiredChild(QObject *parent, const char *name) {
    T *child = parent->findChild<T *>(QString::fromLatin1(name));
    if (child == nullptr) {
        std::cerr << "missing widget: " << name << '\n';
    }
    return child;
}

}  // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MecanumJogPage page;
    QStringList plain;
    QStringList armed;
    QObject::connect(&page, &MecanumJogPage::commandRequested,
                     [&](QString command) { plain.push_back(command); });
    QObject::connect(&page, &MecanumJogPage::armedCommandRequested,
                     [&](QString command) { armed.push_back(command); });

    auto *forward = requiredChild<QPushButton>(&page, "mecanumForwardButton");
    auto *lineDistance = requiredChild<QSpinBox>(&page, "mecanumLineDistanceSpin");
    auto *lineRpm = requiredChild<QSpinBox>(&page, "mecanumLineRpmSpin");
    auto *lineSend = requiredChild<QPushButton>(&page, "mecanumLineSendButton");
    auto *servoId = requiredChild<QComboBox>(&page, "mecanumServoIdCombo");
    auto *servoAngle = requiredChild<QSlider>(&page, "mecanumServoAngleSlider");
    auto *auxMotorId = requiredChild<QComboBox>(&page, "mecanumAuxMotorIdCombo");
    auto *auxMotorDirection =
        requiredChild<QComboBox>(&page, "mecanumAuxMotorDirectionCombo");
    auto *auxMotorSend =
        requiredChild<QPushButton>(&page, "mecanumAuxMotorSendButton");
    auto *auxDistanceMotorId =
        requiredChild<QComboBox>(&page, "mecanumAuxDistanceMotorIdCombo");
    auto *auxDistance =
        requiredChild<QDoubleSpinBox>(&page, "mecanumAuxDistanceSpin");
    auto *auxDistanceRpm =
        requiredChild<QSpinBox>(&page, "mecanumAuxDistanceRpmSpin");
    auto *auxDistanceAccel =
        requiredChild<QSpinBox>(&page, "mecanumAuxDistanceAccelSpin");
    auto *auxDistanceSend =
        requiredChild<QPushButton>(&page, "mecanumAuxDistanceSendButton");
    auto *routeMode = requiredChild<QComboBox>(&page, "mecanumRouteModeCombo");
    auto *routeZone = requiredChild<QComboBox>(&page, "mecanumRouteZoneCombo");
    auto *routeStart = requiredChild<QPushButton>(&page, "mecanumRouteStartButton");
    auto *routeRpm = requiredChild<QSpinBox>(&page, "mecanumRouteRpmSpin");
    auto *routeScale =
        requiredChild<QDoubleSpinBox>(&page, "mecanumRouteLateralScaleSpin");
    auto *routeForwardScale =
        requiredChild<QDoubleSpinBox>(&page, "mecanumRouteForwardScaleSpin");
    auto *routeTurnRpm =
        requiredChild<QSpinBox>(&page, "mecanumRouteTurnRpmSpin");
    auto *routeLateralRpm =
        requiredChild<QSpinBox>(&page, "mecanumRouteLateralRpmSpin");
    auto *routeScaleSend =
        requiredChild<QPushButton>(&page, "mecanumRouteLateralScaleButton");
    auto *headingKp =
        requiredChild<QDoubleSpinBox>(&page, "mecanumHeadingKpSpin");
    auto *headingKi =
        requiredChild<QDoubleSpinBox>(&page, "mecanumHeadingKiSpin");
    auto *headingKd =
        requiredChild<QDoubleSpinBox>(&page, "mecanumHeadingKdSpin");
    auto *headingPidRead =
        requiredChild<QPushButton>(&page, "mecanumHeadingPidReadButton");
    auto *headingPidApply =
        requiredChild<QPushButton>(&page, "mecanumHeadingPidApplyButton");
    auto *headingPidStatus =
        requiredChild<QLabel>(&page, "mecanumHeadingPidStatusLabel");
    auto *headingAnglePlot =
        requiredChild<TelemetryPlot>(&page, "mecanumHeadingAnglePlot");
    auto *headingOutputPlot =
        requiredChild<TelemetryPlot>(&page, "mecanumHeadingOutputPlot");
    auto *pidMoveDuration =
        requiredChild<QSpinBox>(&page, "mecanumPidMoveDurationSpin");
    auto *pidMoveRpm = requiredChild<QSpinBox>(&page, "mecanumPidMoveRpmSpin");
    auto *pidMoveLeft =
        requiredChild<QPushButton>(&page, "mecanumPidMoveLeftButton");
    auto *pidMoveRight =
        requiredChild<QPushButton>(&page, "mecanumPidMoveRightButton");
    auto *turnDirection = requiredChild<QComboBox>(&page, "mecanumTurnDirectionCombo");
    auto *turnAngle = requiredChild<QSpinBox>(&page, "mecanumTurnAngleSpin");
    auto *turnSend = requiredChild<QPushButton>(&page, "mecanumTurnSendButton");
    if (!forward || !lineDistance || !lineRpm || !lineSend || !servoId ||
        !servoAngle || !auxMotorId || !auxMotorDirection || !auxMotorSend ||
        !auxDistanceMotorId || !auxDistance || !auxDistanceRpm ||
        !auxDistanceAccel || !auxDistanceSend ||
        !routeMode || !routeZone || !routeStart || !routeRpm ||
        !routeScale || !routeForwardScale || !routeTurnRpm ||
        !routeLateralRpm ||
        !routeScaleSend || !headingKp || !headingKi || !headingKd ||
        !headingPidRead || !headingPidApply || !headingPidStatus ||
        !headingAnglePlot || !headingOutputPlot || !pidMoveDuration ||
        !pidMoveRpm || !pidMoveLeft || !pidMoveRight ||
        !turnDirection || !turnAngle || !turnSend) {
        return 1;
    }
    if (!require(routeTurnRpm->value() == 45 &&
                     routeLateralRpm->value() == 60 &&
                     routeLateralRpm->minimum() == 10 &&
                     routeLateralRpm->maximum() == 120,
                 "route speed limit controls are incorrect")) {
        return 1;
    }

    page.setConnected(false);
    if (!require(!forward->isEnabled(), "commands enabled while disconnected") ||
        !require(page.findChild<QPushButton *>("mecanumEnableButton") == nullptr,
                 "manual enable control was not removed")) {
        return 1;
    }
    page.setConnected(true);
    forward->click();
    lineDistance->setValue(100);
    lineRpm->setValue(30);
    lineSend->click();
    if (!require(armed == QStringList({QStringLiteral("W"),
                                       QStringLiteral("line W 100 30")}),
                 "armed command mapping is incorrect")) {
        return 1;
    }
    if (!require(lineDistance->minimum() == 100 && lineDistance->maximum() == 500 &&
                     lineDistance->singleStep() == 100 &&
                     lineRpm->minimum() == 10 && lineRpm->maximum() == 120,
                 "line parameter bounds are incorrect")) {
        return 1;
    }
    if (!require(auxDistance->minimum() == -135.0 &&
                     auxDistance->maximum() == 135.0 &&
                     auxDistanceRpm->minimum() == 10 &&
                     auxDistanceRpm->maximum() == 2000 &&
                     auxDistanceAccel->minimum() == 1 &&
                     auxDistanceAccel->maximum() == 240,
                 "auxiliary distance parameter bounds are incorrect")) {
        return 1;
    }
    auxDistanceMotorId->setCurrentText(QStringLiteral("6"));
    auxDistance->setValue(-12.3);
    auxDistanceRpm->setValue(120);
    auxDistanceAccel->setValue(80);
    auxDistanceSend->click();
    if (!require(auxDistance->minimum() == -187.0 &&
                     auxDistance->maximum() == 187.0 &&
                     armed.back() == QStringLiteral("auxmove 6 -123 120 80"),
                 "auxiliary distance command mapping is incorrect")) {
        return 1;
    }
    const qsizetype auxCommandCount = armed.size();
    auxDistance->setValue(0.0);
    auxDistanceSend->click();
    if (!require(armed.size() == auxCommandCount,
                 "zero-distance auxiliary move was emitted")) {
        return 1;
    }

    servoId->setCurrentText(QStringLiteral("4"));
    if (!require(servoAngle->maximum() == 360,
                 "servo 4 did not allow 360 degrees")) {
        return 1;
    }
    servoAngle->setValue(360);
    QMetaObject::invokeMethod(servoAngle, "sliderReleased");
    servoId->setCurrentText(QStringLiteral("2"));
    if (!require(servoAngle->maximum() == 270,
                 "servo 2 did not clamp to 270 degrees")) {
        return 1;
    }

    routeMode->setCurrentText(QStringLiteral("step"));
    routeZone->setCurrentText(QStringLiteral("2"));
    routeRpm->setValue(120);
    routeStart->click();
    routeMode->setCurrentText(QStringLiteral("auto"));
    routeStart->click();
    turnDirection->setCurrentText(QStringLiteral("R"));
    turnAngle->setValue(90);
    turnSend->click();
    routeForwardScale->setValue(102.00);
    routeScale->setValue(90.00);
    routeTurnRpm->setValue(45);
    routeLateralRpm->setValue(80);
    routeScaleSend->click();
    headingKp->setValue(2.35);
    headingKi->setValue(0.40);
    headingKd->setValue(0.18);
    headingPidApply->click();
    page.showError(QStringLiteral("串口未连接"));
    if (!require(headingPidStatus->text() == QStringLiteral("串口未连接"),
                 "PID command failure was not shown")) {
        return 1;
    }
    headingPidRead->click();
    page.appendLine(QStringLiteral("PID kp_x100=200 ki_x100=25 kd_x100=12 RAM_only"));
    if (!require(headingKp->value() == 2.00 && headingKi->value() == 0.25 &&
                     headingKd->value() == 0.12,
                 "PID readback did not update the controls")) {
        return 1;
    }
    if (!require(headingKp->minimum() == 0.00 && headingKp->maximum() == 10.00 &&
                     headingKi->minimum() == 0.00 && headingKi->maximum() == 5.00 &&
                     headingKd->minimum() == 0.00 && headingKd->maximum() == 5.00,
                 "PID gain range is incorrect")) {
        return 1;
    }
    page.appendLine(QStringLiteral("PID kp_x100=1000 ki_x100=500 kd_x100=500 RAM_only"));
    if (!require(headingKp->value() == 10.00 && headingKi->value() == 5.00 &&
                     headingKd->value() == 5.00,
                 "PID maximum readback was clipped")) {
        return 1;
    }
    headingPidApply->click();
    if (!require(plain.back() == QStringLiteral("pid set 1000 500 500"),
                 "PID maximum values were not sent")) {
        return 1;
    }
    page.appendLine(QStringLiteral("PID kp_x100=200 ki_x100=25 kd_x100=12 RAM_only"));
    pidMoveDuration->setValue(1500);
    pidMoveRpm->setValue(20);
    pidMoveLeft->click();
    pidMoveRight->click();
    if (!require(pidMoveDuration->minimum() == 1000 &&
                     pidMoveDuration->maximum() == 5000 &&
                     pidMoveRpm->minimum() == 10 && pidMoveRpm->maximum() == 120 &&
                     armed.at(armed.size() - 2) == QStringLiteral("pid move A 1500 20") &&
                     armed.back() == QStringLiteral("pid move D 1500 20"),
                 "PID lateral test controls emitted the wrong commands")) {
        return 1;
    }
    page.appendLine(QStringLiteral("PID TRACE target_cdeg=9000 actual_cdeg=8950 output_rpm=3"));
    page.appendLine(QStringLiteral("PID TRACE target_cdeg=9000 actual_cdeg=9030 output_rpm=-2"));
    if (!require(headingAnglePlot->sampleCount() == 2 &&
                     headingOutputPlot->sampleCount() == 2,
                 "PID telemetry was not plotted")) {
        return 1;
    }
    page.appendLine(QStringLiteral("RUN PID lateral heading hold; auto-disarmed"));
    if (!require(headingAnglePlot->sampleCount() == 0 &&
                     headingOutputPlot->sampleCount() == 0,
                 "new PID lateral test did not clear old curves")) {
        return 1;
    }
    page.appendLine(QStringLiteral("PID TRACE target_cdeg=9000 actual_cdeg=9030 output_rpm=-2"));
    page.appendLine(QStringLiteral("PID TRACE malformed"));
    if (!require(headingAnglePlot->sampleCount() == 1,
                 "malformed PID telemetry was plotted")) {
        return 1;
    }
    page.appendLine(QStringLiteral("NAV INIT x=2250 y=2250 yaw_cdeg=9000"));
    if (!require(headingAnglePlot->sampleCount() == 0 &&
                     headingOutputPlot->sampleCount() == 0,
                 "new navigation did not clear PID curves")) {
        return 1;
    }
    if (!require(plain.contains(QStringLiteral("servo 4 360")) &&
                     plain.contains(QStringLiteral("route tune 10200 9000 45 80")) &&
                     plain.contains(QStringLiteral("pid set 235 40 18")) &&
                     plain.contains(QStringLiteral("pid get")) &&
                     armed.contains(QStringLiteral("route step 2 120")) &&
                     armed.contains(QStringLiteral("route auto 2 120")) &&
                     armed.contains(QStringLiteral("turn R 90")) &&
                     routeMode->count() == 3 && turnAngle->minimum() == 1 &&
                     turnAngle->maximum() == 180,
                 "servo, route auto, or turn command mapping is incorrect")) {
        return 1;
    }

    const QStringList requiredButtons = {
        QStringLiteral("mecanumStopButton"),
        QStringLiteral("mecanumBackButton"),
        QStringLiteral("mecanumLeftButton"),
        QStringLiteral("mecanumRightButton"),
        QStringLiteral("mecanumStraightSendButton"),
        QStringLiteral("mecanumAuxMotorSendButton"),
        QStringLiteral("mecanumAuxDistanceSendButton"),
        QStringLiteral("mecanumWheelSendButton"),
        QStringLiteral("mecanumCanCheckButton"),
        QStringLiteral("mecanumInvertSendButton"),
        QStringLiteral("mecanumTrimSendButton"),
        QStringLiteral("mecanumImuSendButton"),
        QStringLiteral("mecanumYawDirSendButton"),
        QStringLiteral("mecanumStatusButton"),
        QStringLiteral("mecanumRouteNextButton"),
        QStringLiteral("mecanumRouteStatusButton"),
        QStringLiteral("mecanumRouteLateralScaleButton"),
        QStringLiteral("mecanumHeadingPidReadButton"),
        QStringLiteral("mecanumHeadingPidApplyButton"),
        QStringLiteral("mecanumHelpButton")};
    for (const QString &name : requiredButtons) {
        if (!require(page.findChild<QPushButton *>(name) != nullptr,
                     "a required mecanum command button is missing")) {
            return 1;
        }
    }

    plain.clear();
    armed.clear();
    const QStringList plainButtonNames = {
        QStringLiteral("mecanumStopButton"),
        QStringLiteral("mecanumCanCheckButton"),
        QStringLiteral("mecanumInvertSendButton"),
        QStringLiteral("mecanumTrimSendButton"),
        QStringLiteral("mecanumImuSendButton"),
        QStringLiteral("mecanumYawDirSendButton"),
        QStringLiteral("mecanumStatusButton"),
        QStringLiteral("mecanumRouteNextButton"),
        QStringLiteral("mecanumRouteStatusButton"),
        QStringLiteral("mecanumRouteLateralScaleButton"),
        QStringLiteral("mecanumHeadingPidReadButton"),
        QStringLiteral("mecanumHeadingPidApplyButton"),
        QStringLiteral("mecanumHelpButton")};
    for (const QString &name : plainButtonNames) {
        page.findChild<QPushButton *>(name)->click();
    }
    const QStringList armedButtonNames = {
        QStringLiteral("mecanumBackButton"),
        QStringLiteral("mecanumLeftButton"),
        QStringLiteral("mecanumRightButton"),
        QStringLiteral("mecanumStraightSendButton"),
        QStringLiteral("mecanumWheelSendButton")};
    for (const QString &name : armedButtonNames) {
        page.findChild<QPushButton *>(name)->click();
    }
    auxMotorId->setCurrentText(QStringLiteral("6"));
    auxMotorDirection->setCurrentText(QStringLiteral("1"));
    auxMotorSend->click();
    if (!require(
            plain == QStringList({QStringLiteral("stop"),
                                  QStringLiteral("cancheck 1"),
                                  QStringLiteral("invert 1 0"),
                                  QStringLiteral("trim 1 1000"),
                                  QStringLiteral("imu 115200"),
                                  QStringLiteral("yawdir 0"),
                                  QStringLiteral("status"),
                                  QStringLiteral("route next"),
                                  QStringLiteral("route status"),
                                  QStringLiteral("route tune 10200 9000 45 80"),
                                  QStringLiteral("pid get"),
                                  QStringLiteral("pid set 200 25 12"),
                                  QStringLiteral("help")}) &&
                armed == QStringList({QStringLiteral("S"),
                                      QStringLiteral("A"),
                                      QStringLiteral("D"),
                                      QStringLiteral("straight W 2000 30"),
                                      QStringLiteral("wheel 1 0"),
                                      QStringLiteral("motor 6 1")}),
            "one or more mecanum controls emitted the wrong command")) {
        return 1;
    }

    return 0;
}
