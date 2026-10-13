#pragma once

#include "device/MechanismActionModel.h"

enum class RouteStepType { Move, Turn, Vision, Action, Wait, Confirm };

struct RouteStep {
    RouteStepType type{RouteStepType::Move};
    QString direction{QStringLiteral("W")};
    int distanceMm{500};
    int rpm{100};
    int degrees{90};
    bool ring{};
    int target{1};
    int forwardMilli{640};
    int rightMilli{673};
    MechanismSequence action;
    int waitMs{1000};
    QString prompt{QStringLiteral("检查车辆和机构后继续")};
};

struct RouteSequence {
    QString name{QStringLiteral("新路线")};
    QVector<RouteStep> steps;
};

QString routeStepDescription(const RouteStep &step);
bool validateRouteSequence(const RouteSequence &sequence, QString *error = nullptr);
bool saveRouteSequence(const QString &path, const RouteSequence &sequence, QString *error = nullptr);
bool loadRouteSequence(const QString &path, RouteSequence *sequence, QString *error = nullptr);
