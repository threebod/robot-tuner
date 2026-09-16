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
    int turretDps10{130};
};

struct MechanismInitialData {
    MechanismPoseData pose;
    bool gripperOpen{true};
    int platform{1};
    int gripperOpenDeg{45};
    int gripperCloseDeg{6};
    std::array<int, 3> platformDeg{20, 139, 256};
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
