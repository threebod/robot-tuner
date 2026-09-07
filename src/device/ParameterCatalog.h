#pragma once

#include <QVariant>
#include <QVector>
#include <QtGlobal>

enum class ValueType : quint8 {
    UInt8 = 0x01,
    UInt16 = 0x02,
    UInt32 = 0x03,
    Int8 = 0x04,
    Int16 = 0x05,
    Int32 = 0x06,
    Float32 = 0x07,

    // Spelling aliases keep the wire type names convenient at call sites.
    Uint8 = UInt8,
    Uint16 = UInt16,
    Uint32 = UInt32,
    U8 = UInt8,
    U16 = UInt16,
    U32 = UInt32,
    I8 = Int8,
    I16 = Int16,
    I32 = Int32,
    Float = Float32,
};

struct ParameterSpec {
    quint16 id{};
    QString key;
    QString label;
    QString unit;
    ValueType type{ValueType::Float32};
    double minimum{};
    double maximum{};
    quint8 group{};
};

class ParameterCatalog {
public:
    ParameterCatalog();

    static const ParameterCatalog &instance();

    const ParameterSpec *find(quint16 id) const;
    QVector<ParameterSpec> group(quint8 groupId) const;
    bool validate(quint16 id, const QVariant &value,
                  QString *error = nullptr) const;

private:
    QVector<ParameterSpec> specs_;
};
