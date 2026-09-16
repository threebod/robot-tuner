#include <QApplication>
#include <QPushButton>
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
    auto *initialize =
        page.findChild<QPushButton *>("actionInitializeButton");
    auto *poseHorizontal =
        page.findChild<QSpinBox *>("actionPoseHorizontalSpin");
    auto *addPose = page.findChild<QPushButton *>("actionAddPoseButton");
    auto *table = page.findChild<QTableWidget *>("actionStepTable");
    auto *playAll = page.findChild<QPushButton *>("actionPlayAllButton");
    auto *returnInitial =
        page.findChild<QPushButton *>("actionReturnInitialButton");
    auto *stop = page.findChild<QPushButton *>("actionStopButton");
    if (!require(initialHorizontal && initialize && poseHorizontal && addPose &&
                     table && playAll && returnInitial && stop,
                 "mechanism action page controls are missing")) {
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
    initialize->click();
    if (!require(initialized.size() == 1 &&
                     initialized.front().horizontalDmm == -100,
                 "manual initial pose was not emitted")) {
        return 1;
    }
    page.setMechanismInitialized(true);
    poseHorizontal->setValue(200);
    addPose->click();
    if (!require(table->rowCount() == 1,
                 "pose step was not added to the action table")) {
        return 1;
    }
    playAll->click();
    if (!require(moved.size() == 1 && moved.front().horizontalDmm == 200,
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
    page.handleDeviceLine(QStringLiteral("OK servo=2 angle=45 pulse=1500us"));
    if (!require(commands == QStringList({QStringLiteral("servo 2 45")}),
                 "an immediate servo step did not complete correctly")) {
        return 1;
    }
    returnInitial->click();
    if (!require(moved.size() == 2 && moved.back().horizontalDmm == -100,
                 "return-to-initial did not dispatch the recorded initial pose")) {
        return 1;
    }
    stop->click();
    if (!require(stops == 1, "stop did not cancel replay")) {
        return 1;
    }
    page.setConnected(false);
    if (!require(!initialize->isEnabled() && !playAll->isEnabled(),
                 "disconnect did not lock the action page")) {
        return 1;
    }
    return 0;
}
