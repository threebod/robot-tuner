#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
    source.initial.gripperDps10 = 1800;
    source.initial.platformDps10 = 1700;
    source.initial.platformDeg = {20, 139, 256};
    source.loopEnabled = true;
    source.loopCount = 3;

    MechanismStep pose;
    pose.type = MechanismStepType::Pose;
    pose.pose = {-480, 1500, 684, 1000, 230, 2000, 240, 130};
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
                     loaded.initial.gripperDps10 == 1800 &&
                     loaded.initial.platformDps10 == 1700 &&
                     loaded.loopEnabled && loaded.loopCount == 3 &&
                     loaded.steps[0].pose.horizontalDmm == -480 &&
                     loaded.steps[3].channel == 4,
                 "JSON round trip lost action parameters")) {
        return 1;
    }

    QFile legacyFile(path);
    if (!legacyFile.open(QIODevice::ReadOnly)) return 1;
    QJsonObject legacyRoot = QJsonDocument::fromJson(legacyFile.readAll()).object();
    legacyFile.close();
    legacyRoot.remove(QStringLiteral("loopEnabled"));
    legacyRoot.remove(QStringLiteral("loopCount"));
    QJsonObject legacyInitial = legacyRoot.value(QStringLiteral("initialState")).toObject();
    legacyInitial.remove(QStringLiteral("gripperDps10"));
    legacyInitial.remove(QStringLiteral("platformDps10"));
    legacyRoot.insert(QStringLiteral("initialState"), legacyInitial);
    if (!legacyFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 1;
    legacyFile.write(QJsonDocument(legacyRoot).toJson());
    legacyFile.close();
    MechanismSequence legacy;
    if (!require(loadMechanismSequence(path, &legacy, &error),
                 "legacy sequence was not loaded") ||
        !require(legacy.initial.gripperDps10 == 1200 &&
                     legacy.initial.platformDps10 == 1200 &&
                     !legacy.loopEnabled && legacy.loopCount == 2,
                 "legacy defaults were not applied")) {
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
    source.initial.gripperDps10 = 1801;
    if (!require(!validateMechanismSequence(source, &error),
                 "out-of-range gripper speed was accepted")) {
        return 1;
    }
    source.initial.gripperDps10 = 1800;
    source.steps[0].pose.liftDmm = 1501;
    if (!require(!validateMechanismSequence(source, &error),
                 "out-of-range lift pose was accepted")) {
        return 1;
    }
    source.steps[0].pose.liftDmm = 1500;
    const QString exported = exportMechanismActionC(source, &error);
    if (!require(!exported.isEmpty(), "valid sequence did not export") ||
        !require(exported.contains(QStringLiteral("MECH_INITIAL_STATE")),
                 "C export is missing the initial-state macro") ||
        !require(exported.contains(QStringLiteral("130, 1800, 1700, 1, 2")),
                 "C export is missing initial speeds or servo state") ||
        !require(exported.contains(QStringLiteral("MECH_POSE(-480, 1500, 684")),
                 "C export is missing the maximum lift pose") ||
        !require(exported.contains(QStringLiteral("MECH_GRIPPER_CLOSE(300)")),
                 "C export is missing the gripper macro") ||
        !require(exported.contains(QStringLiteral("MECH_PLATFORM(3, 0)")),
                 "C export is missing the platform macro") ||
        !require(exported.contains(QStringLiteral("MECH_SERVO(4, 180, 200)")),
                 "C export is missing the servo macro") ||
        !require(exported.contains(QStringLiteral("MECH_WAIT(750)")),
                 "C export is missing the wait macro") ||
        !require(exported.contains(QStringLiteral("action_red_1")),
                 "C export did not sanitize the identifier")) {
        return 1;
    }

    std::cout << "PASS: mechanism action JSON and C export\n";
    return 0;
}
