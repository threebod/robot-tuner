#include <QVariant>

#include <iostream>
#include <set>

#include "device/ParameterCatalog.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    const ParameterCatalog catalog;
    const QVector<ParameterSpec> pid = catalog.group(0x10);
    if (!require(pid.size() == 25,
                 "PID group must contain five complete profiles")) {
        return 1;
    }

    std::set<quint16> ids;
    for (const ParameterSpec &spec : pid) {
        if (!require(ids.insert(spec.id).second, "parameter IDs must be unique") ||
            !require(spec.key.startsWith(QStringLiteral("pid.")),
                     "PID parameter key is missing its namespace")) {
            return 1;
        }
    }

    for (int profile = 0; profile < 5; ++profile) {
        const quint16 base = static_cast<quint16>(0x1000 + profile * 0x10);
        const struct {
            int offset;
            double minimum;
            double maximum;
        } expected[] = {
            {0, 0.0, 20.0}, {1, 0.0, 2.0},  {2, 0.0, 20.0},
            {3, 0.0, 100.0}, {4, 0.0, 230.0},
        };
        for (const auto &item : expected) {
            const ParameterSpec *spec = catalog.find(
                static_cast<quint16>(base + item.offset));
            if (!require(spec != nullptr, "fixed PID ID is missing") ||
                !require(spec->minimum == item.minimum &&
                             spec->maximum == item.maximum,
                         "PID parameter bounds differ from the protocol") ||
                !require(spec->group == 0x10, "PID parameter group is incorrect")) {
                return 1;
            }

            QString error;
            if (!require(!catalog.validate(spec->id,
                                           QVariant(item.minimum - 0.001),
                                           &error),
                         "values below the lower bound must be rejected") ||
                !require(!catalog.validate(spec->id,
                                           QVariant(item.maximum + 0.001),
                                           &error),
                         "values above the upper bound must be rejected") ||
                !require(catalog.validate(spec->id,
                                          QVariant(item.minimum), &error) &&
                             catalog.validate(spec->id,
                                              QVariant(item.maximum), &error),
                         "inclusive parameter bounds must be accepted")) {
                return 1;
            }
        }
    }

    const struct {
        quint16 id;
        ValueType type;
        double minimum;
        double maximum;
        quint8 group;
    } expected[] = {
        {0x2000, ValueType::Int32, -80.0, 80.0, 0x20},
        {0x2001, ValueType::Int32, -80.0, 80.0, 0x20},
        {0x2002, ValueType::Int32, -30.0, 30.0, 0x20},
        {0x2003, ValueType::UInt16, 50.0, 1000.0, 0x20},
        {0x2004, ValueType::UInt16, 1.0, 230.0, 0x20},
        {0x3000, ValueType::Float32, -120.0, 63.0, 0x30},
        {0x3001, ValueType::Float32, 0.0, 50.0, 0x30},
        {0x3002, ValueType::Float32, 135.0, 295.0, 0x30},
        {0x3003, ValueType::UInt16, 100.0, 2000.0, 0x30},
        {0x3004, ValueType::UInt8, 1.0, 220.0, 0x30},
        {0x3005, ValueType::Float32, 1.0, 20.0, 0x30},
        {0x4000, ValueType::UInt16, 1.0, 50.0, 0x40},
    };
    for (const auto &item : expected) {
        const ParameterSpec *spec = catalog.find(item.id);
        if (!require(spec != nullptr, "fixed parameter ID is missing") ||
            !require(spec->type == item.type && spec->group == item.group &&
                         spec->minimum == item.minimum &&
                         spec->maximum == item.maximum,
                     "fixed parameter metadata differs from the protocol")) {
            return 1;
        }
        QString error;
        if (!require(!catalog.validate(item.id, QVariant(item.minimum - 0.001),
                                       &error),
                     "values below a parameter lower bound must be rejected") ||
            !require(!catalog.validate(item.id, QVariant(item.maximum + 0.001),
                                       &error),
                     "values above a parameter upper bound must be rejected")) {
            return 1;
        }
    }

    return 0;
}
