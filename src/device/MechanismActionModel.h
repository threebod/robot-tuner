#pragma once

#include <QString>
#include <QVector>

#include <array>

struct MechanismPoseData {
    int horizontalDmm{};
    int liftDmm{};
    int turretDdeg{};
    int horizontalRpm{30};
    int horizontalAccel{50};
    int liftRpm{30};
    int liftAccel{50};
    int turretDps10{1200};
};

struct MechanismInitialData {
    MechanismPoseData pose;
    bool gripperOpen{true};
    int platform{1};
    int gripperOpenDeg{70};
    int gripperCloseDeg{35};
    int gripperDps10{1200};
    int platformDps10{1200};
    std::array<int, 3> platformDeg{26, 146, 264};
};

enum class MechanismStepType { Pose, Gripper, Platform, Servo, Wait };

struct MechanismStep {
    MechanismStepType type{MechanismStepType::Pose};
    MechanismPoseData pose;
    int channel{};
    int value{};
    int waitMs{};
};

struct MechanismSequence {
    QString name{QStringLiteral("mechanism_action")};
    MechanismInitialData initial;
    QVector<MechanismStep> steps;
    bool loopEnabled{};
    int loopCount{2};
};

bool validateMechanismSequence(const MechanismSequence &sequence,
                               QString *error = nullptr);
bool loadMechanismSequence(const QString &path, MechanismSequence *sequence,
                           QString *error = nullptr);
bool saveMechanismSequence(const QString &path,
                           const MechanismSequence &sequence,
                           QString *error = nullptr);
QString exportMechanismActionC(const MechanismSequence &sequence,
                               QString *error = nullptr);
