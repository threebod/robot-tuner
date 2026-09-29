#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>

#include <iostream>

#include "pages/MechanismActionPage.h"

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
    MechanismActionPage page;
    auto *initialHorizontal =
        page.findChild<QSpinBox *>("actionInitialHorizontalSpin");
    auto *initialLift = page.findChild<QSpinBox *>("actionInitialLiftSpin");
    auto *initialTurret = page.findChild<QSpinBox *>("actionInitialTurretSpin");
    auto *initialize =
        page.findChild<QPushButton *>("actionInitializeButton");
    auto *poseHorizontal =
        page.findChild<QSpinBox *>("actionPoseHorizontalSpin");
    auto *poseLift = page.findChild<QSpinBox *>("actionPoseLiftSpin");
    auto *poseTurret = page.findChild<QSpinBox *>("actionPoseTurretSpin");
    auto *addPose = page.findChild<QPushButton *>("actionAddPoseButton");
    auto *replaceStep = page.findChild<QPushButton *>("actionReplaceStepButton");
    auto *replaceType = page.findChild<QComboBox *>("actionReplaceTypeCombo");
    auto *waitOnly = page.findChild<QSpinBox *>("actionWaitSpin");
    auto *horizontalSlider =
        page.findChild<QSlider *>("actionHorizontalPositionSlider");
    auto *liftSlider = page.findChild<QSlider *>("actionLiftPositionSlider");
    auto *turretSlider =
        page.findChild<QSlider *>("actionTurretPositionSlider");
    auto *table = page.findChild<QTableWidget *>("actionStepTable");
    auto *playAll = page.findChild<QPushButton *>("actionPlayAllButton");
    auto *gripperSpeed =
        page.findChild<QSpinBox *>("actionGripperSpeedSpin");
    auto *platformSpeed =
        page.findChild<QSpinBox *>("actionPlatformSpeedSpin");
    auto *turretSpeed =
        page.findChild<QSpinBox *>("actionTurretSpeedSpin");
    auto *loopEnabled =
        page.findChild<QCheckBox *>("actionLoopEnabledCheck");
    auto *loopCount = page.findChild<QSpinBox *>("actionLoopCountSpin");
    auto *returnInitial =
        page.findChild<QPushButton *>("actionReturnInitialButton");
    auto *stop = page.findChild<QPushButton *>("actionStopButton");
    if (!require(initialHorizontal && initialLift && initialTurret && initialize &&
                     poseHorizontal && poseLift && poseTurret && addPose &&
                     replaceStep &&
                     replaceType && waitOnly &&
                     horizontalSlider && liftSlider && turretSlider &&
                     table && playAll && gripperSpeed && platformSpeed &&
                     turretSpeed && loopEnabled &&
                     loopCount && returnInitial && stop,
                 "mechanism action page controls are missing")) {
        return 1;
    }
    if (!require(initialLift->maximum() == 1500 && poseLift->maximum() == 1500 &&
                     horizontalSlider->minimum() == -1220 &&
                     horizontalSlider->maximum() == 650 &&
                     liftSlider->minimum() == 0 && liftSlider->maximum() == 1500 &&
                     turretSlider->minimum() == 0 && turretSlider->maximum() == 3600,
                 "lift or adjustment range is incorrect")) {
        return 1;
    }
    if (!require(gripperSpeed->value() == 1200 &&
                     platformSpeed->value() == 1200 &&
                     turretSpeed->value() == 1200 &&
                     gripperSpeed->maximum() == 1800 &&
                     platformSpeed->maximum() == 1800 &&
                     turretSpeed->maximum() == 1800,
                 "servo speed controls do not default to 120 or allow 180 degrees per second")) {
        return 1;
    }

    QList<MechanismPoseData> initialized;
    QList<MechanismPoseData> moved;
    QStringList commands;
    int stops = 0;
    QObject::connect(&page, &MechanismActionPage::initializationRequested,
                     [&](MechanismPoseData pose) { initialized.push_back(pose); });
    QObject::connect(&page, &MechanismActionPage::poseRequested,
                     [&](MechanismPoseData pose) { moved.push_back(pose); });
    QObject::connect(&page, &MechanismActionPage::stopRequested,
                     [&] { ++stops; });
    QObject::connect(&page, &MechanismActionPage::commandRequested,
                     [&](QString command) { commands.push_back(command); });

    page.setConnected(true);
    initialHorizontal->setValue(-100);
    initialLift->setValue(420);
    initialTurret->setValue(684);
    initialize->click();
    if (!require(initialized.size() == 1 &&
                     initialized.front().horizontalDmm == -100 &&
                     initialized.front().liftDmm == 420 &&
                     initialized.front().turretDdeg == 684 &&
                     !horizontalSlider->isEnabled() && !liftSlider->isEnabled() &&
                     !turretSlider->isEnabled(),
                 "manual initial pose was not emitted")) {
        return 1;
    }
    page.handleDeviceLine(QStringLiteral("MECH INIT H=-100 L=420 T=684"));
    page.setMechanismEstimate(initialized.front(), QStringLiteral("IDLE"));
    if (!require(commands == QStringList({QStringLiteral("servo 2 70 1200")}) &&
                     !horizontalSlider->isEnabled(),
                 "initial confirmation did not command the gripper first")) {
        return 1;
    }
    page.handleDeviceLine(QStringLiteral("OK servo=2 angle=70"));
    if (!require(commands.back() == QStringLiteral("servo 3 26 1200") &&
                     !horizontalSlider->isEnabled(),
                 "platform command was sent before the gripper finished")) {
        return 1;
    }
    page.handleDeviceLine(QStringLiteral("DONE servo=3 angle=26"));
    if (!require(horizontalSlider->isEnabled() && liftSlider->isEnabled() &&
                     turretSlider->isEnabled(),
                 "axis sliders were not enabled after firmware confirmation")) {
        return 1;
    }
    commands.clear();
    horizontalSlider->setValue(200);
    QMetaObject::invokeMethod(horizontalSlider, "sliderReleased");
    if (!require(moved.size() == 1 && moved.back().horizontalDmm == 200 &&
                     moved.back().liftDmm == 420 &&
                     moved.back().turretDdeg == 684,
                 "horizontal slider did not preserve the other axes")) {
        return 1;
    }
    page.setMechanismCompleted(moved.back());
    liftSlider->setValue(1500);
    QMetaObject::invokeMethod(liftSlider, "sliderReleased");
    if (!require(moved.size() == 2 && moved.back().horizontalDmm == 200 &&
                     moved.back().liftDmm == 1500 &&
                     moved.back().turretDdeg == 684,
                 "lift slider did not preserve the other axes")) {
        return 1;
    }
    page.setMechanismCompleted(moved.back());
    turretSlider->setValue(1300);
    QMetaObject::invokeMethod(turretSlider, "sliderReleased");
    if (!require(moved.size() == 3 && moved.back().horizontalDmm == 200 &&
                     moved.back().liftDmm == 1500 &&
                     moved.back().turretDdeg == 1300,
                 "turret slider did not preserve the other axes")) {
        return 1;
    }
    page.setMechanismCompleted(moved.back());
    gripperSpeed->setValue(1800);
    poseHorizontal->setValue(200);
    addPose->click();
    if (!require(table->rowCount() == 1,
                 "pose step was not added to the action table")) {
        return 1;
    }
    table->selectRow(0);
    if (!require(replaceStep->isEnabled(),
                 "selecting a step did not enable replacement")) {
        return 1;
    }
    poseHorizontal->setValue(300);
    replaceStep->click();
    if (!require(table->rowCount() == 1 &&
                     table->item(0, 1)->text().contains(QStringLiteral("H=300")),
                 "selected pose was not replaced")) {
        return 1;
    }
    replaceType->setCurrentIndex(
        replaceType->findData(static_cast<int>(MechanismStepType::Wait)));
    waitOnly->setValue(0);
    replaceStep->click();
    if (!require(table->rowCount() == 1 && table->currentRow() == 0 &&
                     table->item(0, 1)->text() == QStringLiteral("等待") &&
                     table->item(0, 2)->text() == QStringLiteral("0 ms"),
                 "cross-type replacement did not preserve the selected row")) {
        return 1;
    }
    replaceType->setCurrentIndex(
        replaceType->findData(static_cast<int>(MechanismStepType::Pose)));
    poseHorizontal->setValue(300);
    poseLift->setValue(1500);
    poseTurret->setValue(684);
    replaceStep->click();
    if (!require(table->rowCount() == 1 && table->currentRow() == 0 &&
                     table->item(0, 1)->text().contains(
                         QStringLiteral("H=300 L=1500 T=684")) &&
                     table->item(0, 2)->text() == QStringLiteral("0 ms"),
                 "wait step was not replaced with a pose at the same index")) {
        return 1;
    }
    playAll->click();
    if (!require(moved.size() == 4 && moved.back().horizontalDmm == 300,
                 "full replay did not dispatch its pose")) {
        return 1;
    }
    page.setMechanismCompleted(moved.front());
    page.findChild<QPushButton *>("actionAddGripperButton")->click();
    table->selectRow(1);
    auto *playSelected =
        page.findChild<QPushButton *>("actionPlaySelectedButton");
    if (!require(playSelected->isEnabled(),
                 "selecting a step did not enable single-step testing")) {
        return 1;
    }
    playSelected->click();
    page.handleDeviceLine(QStringLiteral("OK servo=2 angle=70 pulse=1500us"));
    if (!require(commands == QStringList({QStringLiteral("servo 2 70 1800")}),
                 "an immediate servo step did not complete correctly")) {
        return 1;
    }
    loopEnabled->setChecked(true);
    loopCount->setValue(2);
    playAll->click();
    if (!require(moved.size() == 5,
                 "loop replay did not dispatch its first cycle")) {
        return 1;
    }
    page.setMechanismCompleted(moved.back());
    page.handleDeviceLine(QStringLiteral("DONE servo=2 angle=70"));
    if (!require(moved.size() == 6,
                 "loop replay did not dispatch its second cycle")) {
        return 1;
    }
    page.setMechanismCompleted(moved.back());
    page.handleDeviceLine(QStringLiteral("DONE servo=2 angle=70"));
    if (!require(playAll->isEnabled(),
                 "loop replay did not stop after the configured count")) {
        return 1;
    }
    returnInitial->click();
    if (!require(moved.size() == 7 && moved.back().horizontalDmm == -100,
                 "return-to-initial did not dispatch the recorded initial pose")) {
        return 1;
    }
    stop->click();
    if (!require(stops == 1, "stop did not cancel replay")) {
        return 1;
    }
    page.setConnected(false);
    if (!require(!initialize->isEnabled() && !playAll->isEnabled() &&
                     !horizontalSlider->isEnabled() && !liftSlider->isEnabled() &&
                     !turretSlider->isEnabled(),
                 "disconnect did not lock the action page")) {
        return 1;
    }
    return 0;
}
