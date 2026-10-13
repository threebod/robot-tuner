#pragma once
#include <QString>
#include <QPointF>
#include <QVector>
#include <QMetaType>

struct AvoidanceStatus {
    QString state, target, reason;
    int round{}, region{}, blockedMask{}, replans{}, xMm{}, yMm{};
    double yawDegrees{};
    bool active{}, cameraOnline{};
};
Q_DECLARE_METATYPE(AvoidanceStatus)
