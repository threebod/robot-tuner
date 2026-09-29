#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

#include <iostream>

#include "pages/VisionPage.h"

namespace {
bool require(bool condition, const char *message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    VisionPage page;
    auto *mode = page.findChild<QComboBox *>("visionModeCombo");
    auto *target = page.findChild<QComboBox *>("visionTargetCombo");
    auto *start = page.findChild<QPushButton *>("visionStartButton");
    auto *pause = page.findChild<QPushButton *>("visionPauseButton");
    auto *forward = page.findChild<QPushButton *>("visionForwardButton");
    auto *step = page.findChild<QComboBox *>("visionStepCombo");
    auto *rpm = page.findChild<QSpinBox *>("visionRpmSpinBox");
    auto *forwardScale = page.findChild<QDoubleSpinBox *>("visionForwardScaleSpinBox");
    auto *rightScale = page.findChild<QDoubleSpinBox *>("visionRightScaleSpinBox");
    auto *applyScale = page.findChild<QPushButton *>("visionApplyScaleButton");
    if (!require(mode && target && start && pause && forward && step && rpm,
                 "vision controls are missing")) return 1;
    if (!require(forwardScale && rightScale && applyScale,
                 "ring scale controls are missing")) return 1;
    page.setConnected(true);
    int material = 0;
    int ring = 0;
    int scaleRing = 0;
    int scaleForward = 0;
    int scaleRight = 0;
    int jogForward = 0;
    int jogRight = 0;
    QObject::connect(&page, &VisionPage::materialPickupRequested,
                     [&](int value) { material = value; });
    QObject::connect(&page, &VisionPage::ringAlignmentRequested,
                     [&](int value) { ring = value; });
    QObject::connect(&page, &VisionPage::ringScaleRequested,
                     [&](int value, int forwardMilli, int rightMilli) {
                         scaleRing = value;
                         scaleForward = forwardMilli;
                         scaleRight = rightMilli;
                     });
    QObject::connect(&page, &VisionPage::jogRequested,
                     [&](int forwardMm, int rightMm, int) {
                         jogForward = forwardMm;
                         jogRight = rightMm;
                     });
    start->click();
    if (!require(material == 1, "material pickup signal is incorrect")) return 1;
    mode->setCurrentIndex(1);
    target->setCurrentIndex(2);
    if (!require(!start->isEnabled(),
                 "unconfigured ring allowed automatic alignment")) return 1;
    forwardScale->setValue(0.700);
    rightScale->setValue(0.650);
    target->setCurrentIndex(0);
    if (!require(forwardScale->value() == 0.640 && rightScale->value() == 0.673,
                 "ring scales were not independent")) return 1;
    target->setCurrentIndex(2);
    applyScale->click();
    if (!require(scaleRing == 3 && scaleForward == 700 && scaleRight == 650,
                 "ring scale signal or per-ring values are incorrect")) return 1;
    if (!require(!start->isEnabled(),
                 "ring alignment started before scale confirmation")) return 1;
    page.setRingScaleApplied(3, 700, 650);
    if (!require(start->isEnabled(),
                 "confirmed ring scale did not enable alignment")) return 1;
    target->setCurrentIndex(1);
    if (!require(!start->isEnabled(),
                 "another ring inherited scale confirmation")) return 1;
    forwardScale->setValue(0.720);
    rightScale->setValue(0.810);
    if (!require(page.ring2ForwardMilli() == 720 && page.ring2RightMilli() == 810,
                 "mission did not read current ring 2 scale")) return 1;
    target->setCurrentIndex(2);
    start->click();
    if (!require(ring == 3, "ring alignment signal is incorrect")) return 1;
    page.setVisionRunning(true);
    if (!require(!start->isEnabled() && pause->isEnabled() && !forward->isEnabled(),
                 "running state did not lock manual controls")) return 1;
    page.setVisionState("PAUSED");
    page.setOtherMotionRunning(true);
    if (!require(!start->isEnabled() && !forward->isEnabled(),
                 "other motion did not block vision controls")) return 1;
    page.setOtherMotionRunning(false);
    if (!require(forward->isEnabled(),
                 "manual controls did not recover after other motion")) return 1;
    step->setCurrentIndex(2);
    rpm->setValue(30);
    forward->click();
    if (!require(jogForward == 20 && jogRight == 0,
                 "manual forward jog signal is incorrect")) return 1;
    page.setConnected(false);
    if (!require(!start->isEnabled() && !pause->isEnabled() && !forward->isEnabled(),
                 "disconnected state left controls enabled")) return 1;
    return 0;
}
