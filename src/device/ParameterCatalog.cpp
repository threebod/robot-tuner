#include "device/ParameterCatalog.h"

#include <QMetaType>

#include <cmath>

namespace {

ParameterSpec makeSpec(quint16 id, const QString &key, const QString &label,
                       const QString &unit, ValueType type, double minimum,
                       double maximum, quint8 group) {
    return ParameterSpec{id, key, label, unit, type, minimum, maximum, group};
}

bool isNumeric(const QVariant &value) {
    switch (value.metaType().id()) {
    case QMetaType::Bool:
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Float:
    case QMetaType::Double:
        return value.metaType().id() != QMetaType::Bool;
    default:
        return false;
    }
}

bool isIntegral(const QVariant &value) {
    switch (value.metaType().id()) {
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    default:
        return false;
    }
}

bool numericValue(const QVariant &value, double *result) {
    if (!isNumeric(value)) {
        return false;
    }
    bool ok = false;
    const double converted = value.toDouble(&ok);
    if (!ok || !std::isfinite(converted)) {
        return false;
    }
    *result = converted;
    return true;
}

void addPidSpecs(QVector<ParameterSpec> *specs) {
    struct PidField {
        const char *suffix;
        const char *label;
        double minimum;
        double maximum;
    };
    constexpr PidField fields[] = {
        {"kp", "Kp", 0.0, 20.0},
        {"ki", "Ki", 0.0, 2.0},
        {"kd", "Kd", 0.0, 20.0},
        {"integral_limit", "积分限幅", 0.0, 100.0},
        {"output_limit", "输出限幅", 0.0, 230.0},
    };

    for (int profile = 0; profile < 5; ++profile) {
        const quint16 base = static_cast<quint16>(0x1000 + profile * 0x10);
        for (int offset = 0; offset < 5; ++offset) {
            const auto &field = fields[offset];
            specs->push_back(makeSpec(
                static_cast<quint16>(base + offset),
                QStringLiteral("pid.%1.%2").arg(profile).arg(field.suffix),
                QStringLiteral("PID %1 %2").arg(profile).arg(field.label), {},
                ValueType::Float32, field.minimum, field.maximum, 0x10));
        }
    }
}

}  // namespace

ParameterCatalog::ParameterCatalog() {
    addPidSpecs(&specs_);

    specs_.push_back(makeSpec(0x2000, QStringLiteral("chassis.vx_test_limit"),
                              QStringLiteral("底盘 vx 测试限幅"),
                              QStringLiteral("mm/s"), ValueType::Int32, -80.0,
                              80.0, 0x20));
    specs_.push_back(makeSpec(0x2001, QStringLiteral("chassis.vy_test_limit"),
                              QStringLiteral("底盘 vy 测试限幅"),
                              QStringLiteral("mm/s"), ValueType::Int32, -80.0,
                              80.0, 0x20));
    specs_.push_back(makeSpec(0x2002, QStringLiteral("chassis.w_test_limit"),
                              QStringLiteral("底盘 w 测试限幅"),
                              QStringLiteral("degree/s"), ValueType::Int32,
                              -30.0, 30.0, 0x20));
    specs_.push_back(makeSpec(0x2003, QStringLiteral("chassis.test_duration_ms"),
                              QStringLiteral("底盘测试持续时间"),
                              QStringLiteral("ms"), ValueType::UInt16, 50.0,
                              1000.0, 0x20));
    specs_.push_back(makeSpec(0x2004, QStringLiteral("motion.acceleration"),
                              QStringLiteral("运动加速度"), QStringLiteral("mm/s²"),
                              ValueType::UInt16, 1.0, 230.0, 0x20));

    specs_.push_back(makeSpec(0x3000, QStringLiteral("mechanism.horizontal_position"),
                              QStringLiteral("水平机构位置"), QStringLiteral("mm"),
                              ValueType::Float32, -120.0, 63.0, 0x30));
    specs_.push_back(makeSpec(0x3001, QStringLiteral("mechanism.lift_position"),
                              QStringLiteral("升降机构位置"), QStringLiteral("mm"),
                              ValueType::Float32, 0.0, 50.0, 0x30));
    specs_.push_back(makeSpec(0x3002, QStringLiteral("mechanism.turret_angle"),
                              QStringLiteral("云台角度"), QStringLiteral("degree"),
                              ValueType::Float32, 135.0, 295.0, 0x30));
    specs_.push_back(makeSpec(0x3003, QStringLiteral("mechanism.motor_speed"),
                              QStringLiteral("机构电机速度"), QStringLiteral("step/s"),
                              ValueType::UInt16, 100.0, 2000.0, 0x30));
    specs_.push_back(makeSpec(0x3004, QStringLiteral("mechanism.acceleration"),
                              QStringLiteral("机构加速度"), QStringLiteral("mm/s²"),
                              ValueType::UInt8, 1.0, 220.0, 0x30));
    specs_.push_back(makeSpec(0x3005, QStringLiteral("mechanism.turret_interpolation_speed"),
                              QStringLiteral("云台插补速度"), QStringLiteral("degree/s"),
                              ValueType::Float32, 1.0, 20.0, 0x30));

    specs_.push_back(makeSpec(0x4000, QStringLiteral("imu.telemetry_rate_hz"),
                              QStringLiteral("IMU 遥测频率"), QStringLiteral("Hz"),
                              ValueType::UInt16, 1.0, 50.0, 0x40));
}

const ParameterCatalog &ParameterCatalog::instance() {
    static const ParameterCatalog catalog;
    return catalog;
}

const ParameterSpec *ParameterCatalog::find(quint16 id) const {
    for (const ParameterSpec &spec : specs_) {
        if (spec.id == id) {
            return &spec;
        }
    }
    return nullptr;
}

QVector<ParameterSpec> ParameterCatalog::group(quint8 groupId) const {
    QVector<ParameterSpec> result;
    for (const ParameterSpec &spec : specs_) {
        if (spec.group == groupId) {
            result.push_back(spec);
        }
    }
    return result;
}

bool ParameterCatalog::validate(quint16 id, const QVariant &value,
                                QString *error) const {
    const ParameterSpec *spec = find(id);
    if (spec == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("参数不存在 (0x05)");
        }
        return false;
    }

    double number = 0.0;
    if (!numericValue(value, &number)) {
        if (error != nullptr) {
            *error = QStringLiteral("参数类型错误 (0x06)");
        }
        return false;
    }

    if (spec->type != ValueType::Float32 && !isIntegral(value)) {
        if (error != nullptr) {
            *error = QStringLiteral("参数类型错误 (0x06)");
        }
        return false;
    }
    if (spec->type != ValueType::Float32 && std::trunc(number) != number) {
        if (error != nullptr) {
            *error = QStringLiteral("参数类型错误 (0x06)");
        }
        return false;
    }
    if (number < spec->minimum || number > spec->maximum) {
        if (error != nullptr) {
            *error = QStringLiteral("参数越界 (0x07)");
        }
        return false;
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}
