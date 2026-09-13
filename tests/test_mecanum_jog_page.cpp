#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>

#include <iostream>

#include "pages/MecanumJogPage.h"

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

    auto *enable = requiredChild<QPushButton>(&page, "mecanumEnableButton");
    auto *forward = requiredChild<QPushButton>(&page, "mecanumForwardButton");
    auto *lineDistance = requiredChild<QSpinBox>(&page, "mecanumLineDistanceSpin");
    auto *lineRpm = requiredChild<QSpinBox>(&page, "mecanumLineRpmSpin");
    auto *lineSend = requiredChild<QPushButton>(&page, "mecanumLineSendButton");
    auto *servoId = requiredChild<QComboBox>(&page, "mecanumServoIdCombo");
    auto *servoAngle = requiredChild<QSpinBox>(&page, "mecanumServoAngleSpin");
    auto *servoSend = requiredChild<QPushButton>(&page, "mecanumServoSendButton");
    auto *routeMode = requiredChild<QComboBox>(&page, "mecanumRouteModeCombo");
    auto *routeZone = requiredChild<QComboBox>(&page, "mecanumRouteZoneCombo");
    auto *routeStart = requiredChild<QPushButton>(&page, "mecanumRouteStartButton");
    auto *turnDirection = requiredChild<QComboBox>(&page, "mecanumTurnDirectionCombo");
    auto *turnAngle = requiredChild<QSpinBox>(&page, "mecanumTurnAngleSpin");
    auto *turnSend = requiredChild<QPushButton>(&page, "mecanumTurnSendButton");
    if (!enable || !forward || !lineDistance || !lineRpm || !lineSend ||
        !servoId || !servoAngle || !servoSend || !routeMode || !routeZone ||
        !routeStart || !turnDirection || !turnAngle || !turnSend) {
        return 1;
    }

    page.setConnected(false);
    if (!require(!enable->isEnabled(), "commands enabled while disconnected")) {
        return 1;
    }
    page.setConnected(true);
    enable->click();
    forward->click();
    lineDistance->setValue(100);
    lineRpm->setValue(30);
    lineSend->click();
    if (!require(armed == QStringList({QStringLiteral("enable"),
                                       QStringLiteral("W"),
                                       QStringLiteral("line W 100 30")}),
                 "armed command mapping is incorrect")) {
        return 1;
    }
    if (!require(lineDistance->minimum() == 20 && lineDistance->maximum() == 500 &&
                     lineRpm->minimum() == 10 && lineRpm->maximum() == 60,
                 "line parameter bounds are incorrect")) {
        return 1;
    }

    servoId->setCurrentText(QStringLiteral("4"));
    if (!require(servoAngle->maximum() == 360,
                 "servo 4 did not allow 360 degrees")) {
        return 1;
    }
    servoAngle->setValue(360);
    servoSend->click();
    servoId->setCurrentText(QStringLiteral("2"));
    if (!require(servoAngle->maximum() == 270,
                 "servo 2 did not clamp to 270 degrees")) {
        return 1;
    }

    routeMode->setCurrentText(QStringLiteral("step"));
    routeZone->setCurrentText(QStringLiteral("2"));
    routeStart->click();
    routeMode->setCurrentText(QStringLiteral("auto"));
    routeStart->click();
    turnDirection->setCurrentText(QStringLiteral("R"));
    turnAngle->setValue(90);
    turnSend->click();
    if (!require(plain.contains(QStringLiteral("servo 4 360")) &&
                     armed.contains(QStringLiteral("route step 2")) &&
                     armed.contains(QStringLiteral("route auto 2")) &&
                     armed.contains(QStringLiteral("turn R 90")) &&
                     routeMode->count() == 3 && turnAngle->minimum() == 1 &&
                     turnAngle->maximum() == 180,
                 "servo, route auto, or turn command mapping is incorrect")) {
        return 1;
    }

    const QStringList requiredButtons = {
        QStringLiteral("mecanumDisableButton"),
        QStringLiteral("mecanumEnable5Button"),
        QStringLiteral("mecanumDisable5Button"),
        QStringLiteral("mecanumStopButton"),
        QStringLiteral("mecanumBackButton"),
        QStringLiteral("mecanumLeftButton"),
        QStringLiteral("mecanumRightButton"),
        QStringLiteral("mecanumStraightSendButton"),
        QStringLiteral("mecanumMotor5SendButton"),
        QStringLiteral("mecanumWheelSendButton"),
        QStringLiteral("mecanumCanCheckButton"),
        QStringLiteral("mecanumInvertSendButton"),
        QStringLiteral("mecanumTrimSendButton"),
        QStringLiteral("mecanumImuSendButton"),
        QStringLiteral("mecanumYawDirSendButton"),
        QStringLiteral("mecanumStatusButton"),
        QStringLiteral("mecanumRouteNextButton"),
        QStringLiteral("mecanumRouteStatusButton"),
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
        QStringLiteral("mecanumDisableButton"),
        QStringLiteral("mecanumDisable5Button"),
        QStringLiteral("mecanumStopButton"),
        QStringLiteral("mecanumCanCheckButton"),
        QStringLiteral("mecanumInvertSendButton"),
        QStringLiteral("mecanumTrimSendButton"),
        QStringLiteral("mecanumImuSendButton"),
        QStringLiteral("mecanumYawDirSendButton"),
        QStringLiteral("mecanumStatusButton"),
        QStringLiteral("mecanumRouteNextButton"),
        QStringLiteral("mecanumRouteStatusButton"),
        QStringLiteral("mecanumHelpButton")};
    for (const QString &name : plainButtonNames) {
        page.findChild<QPushButton *>(name)->click();
    }
    const QStringList armedButtonNames = {
        QStringLiteral("mecanumEnable5Button"),
        QStringLiteral("mecanumBackButton"),
        QStringLiteral("mecanumLeftButton"),
        QStringLiteral("mecanumRightButton"),
        QStringLiteral("mecanumStraightSendButton"),
        QStringLiteral("mecanumMotor5SendButton"),
        QStringLiteral("mecanumWheelSendButton")};
    for (const QString &name : armedButtonNames) {
        page.findChild<QPushButton *>(name)->click();
    }
    if (!require(
            plain == QStringList({QStringLiteral("disable"),
                                  QStringLiteral("disable5"),
                                  QStringLiteral("stop"),
                                  QStringLiteral("cancheck 1"),
                                  QStringLiteral("invert 1 0"),
                                  QStringLiteral("trim 1 1000"),
                                  QStringLiteral("imu 115200"),
                                  QStringLiteral("yawdir 0"),
                                  QStringLiteral("status"),
                                  QStringLiteral("route next"),
                                  QStringLiteral("route status"),
                                  QStringLiteral("help")}) &&
                armed == QStringList({QStringLiteral("enable5"),
                                      QStringLiteral("S"),
                                      QStringLiteral("A"),
                                      QStringLiteral("D"),
                                      QStringLiteral("straight W 2000 30"),
                                      QStringLiteral("motor5 0"),
                                      QStringLiteral("wheel 1 0")}),
            "one or more mecanum controls emitted the wrong command")) {
        return 1;
    }

    return 0;
}
