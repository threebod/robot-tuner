#pragma once

#include <QMetaType>
#include <QVariant>
#include <QtGlobal>

#include "device/ParameterCatalog.h"

struct ParameterValue {
    quint16 id{};
    ValueType type{ValueType::Float32};
    QVariant value;

    ParameterValue() = default;
    ParameterValue(quint16 parameterId, ValueType valueType,
                   const QVariant &parameterValue)
        : id(parameterId), type(valueType), value(parameterValue) {}
    ParameterValue(quint16 parameterId, const QVariant &parameterValue,
                   ValueType valueType)
        : id(parameterId), type(valueType), value(parameterValue) {}
};

struct DeviceInfo {
    quint8 protocolVersion{};
    quint8 firmwareMajor{};
    quint8 firmwareMinor{};
    quint8 firmwarePatch{};
    quint32 capabilities{};

    // Short aliases are useful when displaying the version in a view.
    quint8 protocol{};
    quint8 fwMajor{};
    quint8 fwMinor{};
    quint8 fwPatch{};
};

struct ImuSample {
    quint32 timestampMs{};
    double accelerationX{};
    double accelerationY{};
    double accelerationZ{};
    double angularVelocityX{};
    double angularVelocityY{};
    double angularVelocityZ{};
    double rollDegrees{};
    double pitchDegrees{};
    double yawDegrees{};

    // Compact aliases mirror the names used by the wire payload.
    double ax{};
    double ay{};
    double az{};
    double gx{};
    double gy{};
    double gz{};
    double roll{};
    double pitch{};
    double yaw{};
};

struct PidSample {
    quint32 timestampMs{};
    double targetDegrees{};
    double actualDegrees{};
    double output{};

    double target{};
    double actual{};
};

struct DeviceStatus {
    quint8 mode{};
    quint8 emergency{};
    quint8 unlocked{};
    quint16 lastError{};
    quint8 activeLink{};
};

using StatusSample = DeviceStatus;

Q_DECLARE_METATYPE(ParameterValue)
Q_DECLARE_METATYPE(QVector<ParameterValue>)
Q_DECLARE_METATYPE(DeviceInfo)
Q_DECLARE_METATYPE(ImuSample)
Q_DECLARE_METATYPE(PidSample)
Q_DECLARE_METATYPE(DeviceStatus)
