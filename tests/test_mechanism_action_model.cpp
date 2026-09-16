#include <QFile>
#include <QTemporaryDir>

#include <iostream>

#include "device/MechanismActionModel.h"

namespace {
bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}
}

int main() {
    QTemporaryDir directory;
    MechanismSequence source;
    source.name = QStringLiteral("抓取 red 1");
    source.initial.pose = {-10, 20, 684, 30, 50, 40, 60, 130};
    source.initial.gripperOpen = true;
    source.initial.platform = 2;
    source.initial.gripperOpenDeg = 45;
    source.initial.gripperCloseDeg = 6;
    source.initial.platformDeg = {20, 139, 256};

    MechanismStep pose;
    pose.type = MechanismStepType::Pose;
    pose.pose = {-480, 250, 1350, 1000, 230, 2000, 240, 130};
    pose.waitMs = 500;
    source.steps.push_back(pose);
    MechanismStep gripper;
    gripper.type = MechanismStepType::Gripper;
    gripper.value = 0;
    gripper.waitMs = 300;
    source.steps.push_back(gripper);
    MechanismStep platform;
    platform.type = MechanismStepType::Platform;
    platform.value = 3;
    source.steps.push_back(platform);
    MechanismStep servo;
    servo.type = MechanismStepType::Servo;
    servo.channel = 4;
    servo.value = 180;
    servo.waitMs = 200;
    source.steps.push_back(servo);
    MechanismStep wait;
    wait.type = MechanismStepType::Wait;
    wait.waitMs = 750;
    source.steps.push_back(wait);

    QString error;
    const QString path = directory.filePath(QStringLiteral("action.json"));
    if (!require(directory.isValid(), "temporary directory unavailable") ||
        !require(saveMechanismSequence(path, source, &error),
                 "valid sequence was not saved")) {
        return 1;
    }
    MechanismSequence loaded;
    if (!require(loadMechanismSequence(path, &loaded, &error),
                 "saved sequence was not loaded") ||
        !require(loaded.name == source.name && loaded.steps.size() == 5,
                 "JSON round trip lost sequence data") ||
        !require(loaded.initial.platformDeg[2] == 256 &&
                     loaded.steps[0].pose.horizontalDmm == -480 &&
                     loaded.steps[3].channel == 4,
                 "JSON round trip lost action parameters")) {
        return 1;
    }

    QFile invalid(path);
    if (!invalid.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 1;
    invalid.write("{\"version\":2,\"name\":\"bad\"}");
    invalid.close();
    const QString preservedName = loaded.name;
    if (!require(!loadMechanismSequence(path, &loaded, &error),
                 "unknown JSON version was accepted") ||
        !require(loaded.name == preservedName,
                 "failed load overwrote the current sequence")) {
        return 1;
    }

    source.initial.pose.horizontalDmm = -1221;
    if (!require(!validateMechanismSequence(source, &error),
                 "out-of-range initial pose was accepted")) {
        return 1;
    }
    source.initial.pose.horizontalDmm = -10;
    const QString exported = exportMechanismActionC(source, &error);
    if (!require(!exported.isEmpty(), "valid sequence did not export") ||
        !require(exported.contains(QStringLiteral("MECH_INITIAL_STATE")) &&
                     exported.contains(QStringLiteral("MECH_POSE(-480, 250, 1350")) &&
                     exported.contains(QStringLiteral("MECH_GRIPPER_CLOSE(300)")) &&
                     exported.contains(QStringLiteral("MECH_PLATFORM(3, 0)")) &&
                     exported.contains(QStringLiteral("MECH_SERVO(4, 180, 200)")) &&
                     exported.contains(QStringLiteral("MECH_WAIT(750)")),
                 "C export is missing action macros") ||
        !require(exported.contains(QStringLiteral("action_red_1")),
                 "C export did not sanitize the identifier")) {
        return 1;
    }

    std::cout << "PASS: mechanism action JSON and C export\n";
    return 0;
}
