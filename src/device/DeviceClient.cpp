#include "device/DeviceClient.h"

#include <QByteArray>

#include <cmath>
#include <cstring>

namespace {

constexpr int kImuPayloadWithTimestamp = 4 + 9 * 2;
constexpr int kPidPayloadSize = 4 + 3 * 2;

enum class TypedDecodeResult {
    Ok,
    Truncated,
    Invalid,
};

quint16 readU16(const QByteArray &bytes, int offset) {
    return static_cast<quint16>(
        static_cast<quint8>(bytes.at(offset)) |
        (static_cast<quint16>(static_cast<quint8>(bytes.at(offset + 1))) << 8));
}

qint16 readI16(const QByteArray &bytes, int offset) {
    return static_cast<qint16>(readU16(bytes, offset));
}

quint32 readU32(const QByteArray &bytes, int offset) {
    return static_cast<quint32>(
        static_cast<quint8>(bytes.at(offset)) |
        (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 1))) << 8) |
        (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 2))) << 16) |
        (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 3))) << 24));
}

void appendU16(QByteArray *bytes, quint16 value) {
    bytes->append(static_cast<char>(value & 0xff));
    bytes->append(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(QByteArray *bytes, quint32 value) {
    bytes->append(static_cast<char>(value & 0xff));
    bytes->append(static_cast<char>((value >> 8) & 0xff));
    bytes->append(static_cast<char>((value >> 16) & 0xff));
    bytes->append(static_cast<char>((value >> 24) & 0xff));
}

bool readNumeric(const QVariant &value, ValueType type, quint32 *bits,
                 int *width) {
    bool ok = false;
    switch (type) {
    case ValueType::UInt8:
        *bits = value.toUInt(&ok);
        *width = 1;
        return ok;
    case ValueType::UInt16:
        *bits = value.toUInt(&ok);
        *width = 2;
        return ok;
    case ValueType::UInt32:
        *bits = value.toUInt(&ok);
        *width = 4;
        return ok;
    case ValueType::Int8:
        *bits = static_cast<quint32>(static_cast<qint8>(value.toInt(&ok)));
        *width = 1;
        return ok;
    case ValueType::Int16:
        *bits = static_cast<quint32>(static_cast<qint16>(value.toInt(&ok)));
        *width = 2;
        return ok;
    case ValueType::Int32:
        *bits = static_cast<quint32>(value.toInt(&ok));
        *width = 4;
        return ok;
    case ValueType::Float32: {
        const float converted = value.toFloat(&ok);
        if (!ok || !std::isfinite(converted)) {
            return false;
        }
        std::memcpy(bits, &converted, sizeof(converted));
        *width = 4;
        return true;
    }
    }
    return false;
}

TypedDecodeResult decodeTypedValue(const QByteArray &payload, int *offset,
                                   ValueType type, QVariant *value) {
    int width = 0;
    switch (type) {
    case ValueType::UInt8:
    case ValueType::Int8:
        width = 1;
        break;
    case ValueType::UInt16:
    case ValueType::Int16:
        width = 2;
        break;
    case ValueType::UInt32:
    case ValueType::Int32:
    case ValueType::Float32:
        width = 4;
        break;
    default:
        return TypedDecodeResult::Invalid;
    }
    if (*offset < 0 || *offset + width > payload.size()) {
        return TypedDecodeResult::Truncated;
    }

    switch (type) {
    case ValueType::UInt8:
        *value = QVariant::fromValue(static_cast<quint8>(
            static_cast<quint8>(payload.at(*offset))));
        break;
    case ValueType::UInt16:
        *value = QVariant::fromValue(readU16(payload, *offset));
        break;
    case ValueType::UInt32:
        *value = QVariant::fromValue(readU32(payload, *offset));
        break;
    case ValueType::Int8:
        *value = QVariant::fromValue(static_cast<qint8>(payload.at(*offset)));
        break;
    case ValueType::Int16:
        *value = QVariant::fromValue(readI16(payload, *offset));
        break;
    case ValueType::Int32:
        *value = QVariant::fromValue(static_cast<qint32>(readU32(payload, *offset)));
        break;
    case ValueType::Float32: {
        const quint32 bits = readU32(payload, *offset);
        float converted = 0.0f;
        std::memcpy(&converted, &bits, sizeof(converted));
        if (!std::isfinite(converted)) {
            return TypedDecodeResult::Invalid;
        }
        *value = QVariant::fromValue(converted);
        break;
    }
    }
    *offset += width;
    return TypedDecodeResult::Ok;
}

QString errorText(quint8 code) {
    switch (code) {
    case 0x01:
        return QStringLiteral("协议版本不支持");
    case 0x02:
        return QStringLiteral("命令不支持");
    case 0x03:
        return QStringLiteral("CRC 校验失败");
    case 0x04:
        return QStringLiteral("帧长度错误");
    case 0x05:
        return QStringLiteral("参数不存在");
    case 0x06:
        return QStringLiteral("参数类型错误");
    case 0x07:
        return QStringLiteral("参数越界");
    case 0x08:
        return QStringLiteral("设备忙");
    case 0x09:
        return QStringLiteral("调试动作未解锁");
    case 0x0a:
        return QStringLiteral("急停锁定");
    default:
        return QStringLiteral("未知设备错误");
    }
}

}  // namespace

DeviceClient::DeviceClient(ProtocolClient *protocol, QObject *parent)
    : QObject(parent), protocol_(protocol) {
    if (protocol_ == nullptr) {
        reportError(0x02, QStringLiteral("协议客户端为空"));
        return;
    }
    connect(protocol_, &ProtocolClient::responseReceived, this,
            &DeviceClient::handleResponse);
    connect(protocol_, &ProtocolClient::eventReceived, this,
            &DeviceClient::handleEvent);
    connect(protocol_, &ProtocolClient::requestFailed, this,
            &DeviceClient::handleRequestFailure);
    connect(protocol_, &ProtocolClient::connectionCleared, this,
            &DeviceClient::handleConnectionCleared);
}

DeviceClient::DeviceClient(ProtocolClient &protocol, QObject *parent)
    : DeviceClient(&protocol, parent) {}

bool DeviceClient::hello() {
    handshakeComplete_ = false;
    if (protocol_ == nullptr) {
        helloPending_ = false;
        return reject(QStringLiteral("协议客户端为空"), 0x02);
    }
    helloPending_ = true;
    helloSequence_ = protocol_->sendRequest(protocol::Command::Hello, {});
    return true;
}

bool DeviceClient::getStatus() {
    if (!handshakeComplete_) {
        return reject(QStringLiteral("设备尚未完成握手"), 0x09);
    }
    return send(protocol::Command::GetStatus, {});
}

bool DeviceClient::getParameterGroup(quint8 group) {
    if (catalog_.group(group).isEmpty()) {
        return reject(QStringLiteral("参数组不存在"), 0x05);
    }
    return send(protocol::Command::GetParamGroup,
                QByteArray(1, static_cast<char>(group)));
}

bool DeviceClient::setParameterGroup(
    quint8 group, const QVector<ParameterValue> &values) {
    if (!handshakeComplete_) {
        return reject(QStringLiteral("设备尚未完成握手"), 0x09);
    }
    if (catalog_.group(group).isEmpty()) {
        return reject(QStringLiteral("参数组不存在"), 0x05);
    }
    if (values.isEmpty() || values.size() > 255) {
        return reject(QStringLiteral("参数组数量错误"), 0x04);
    }

    QByteArray payload;
    payload.reserve(2 + values.size() * 8);
    payload.append(static_cast<char>(group));
    payload.append(static_cast<char>(values.size()));
    for (const ParameterValue &parameter : values) {
        const ParameterSpec *spec = catalog_.find(parameter.id);
        QString validationError;
        if (spec == nullptr) {
            return reject(QStringLiteral("参数不存在"), 0x05);
        }
        if (spec->group != group) {
            return reject(QStringLiteral("参数不属于该参数组"), 0x05);
        }
        if (spec->type != parameter.type) {
            return reject(QStringLiteral("参数类型错误"), 0x06);
        }
        if (!catalog_.validate(parameter.id, parameter.value, &validationError)) {
            const quint8 errorCode =
                validationError.contains(QStringLiteral("0x07")) ? 0x07 : 0x06;
            return reject(validationError.isEmpty()
                              ? QStringLiteral("参数不存在")
                              : validationError,
                          errorCode);
        }

        quint32 bits = 0;
        int width = 0;
        if (!readNumeric(parameter.value, parameter.type, &bits, &width)) {
            return reject(QStringLiteral("参数类型错误"), 0x06);
        }
        appendU16(&payload, parameter.id);
        payload.append(static_cast<char>(parameter.type));
        if (width == 1) {
            payload.append(static_cast<char>(bits & 0xff));
        } else if (width == 2) {
            appendU16(&payload, static_cast<quint16>(bits));
        } else {
            appendU32(&payload, bits);
        }
    }

    if (payload.size() > 128) {
        return reject(QStringLiteral("参数组数据过长"), 0x04);
    }
    return send(protocol::Command::SetParamGroup, payload);
}

bool DeviceClient::setTelemetry(quint8 mask, quint16 periodMs) {
    if (!handshakeComplete_) {
        return reject(QStringLiteral("设备尚未完成握手"), 0x09);
    }
    QByteArray payload;
    payload.append(static_cast<char>(mask));
    appendU16(&payload, periodMs);
    return send(protocol::Command::SetTelemetry, payload);
}

bool DeviceClient::calibrateImu() {
    if (!handshakeComplete_) {
        return reject(QStringLiteral("设备尚未完成握手"), 0x09);
    }
    return send(protocol::Command::ImuCalibrate, {});
}

bool DeviceClient::handshakeComplete() const {
    return handshakeComplete_;
}

const ParameterCatalog &DeviceClient::parameterCatalog() const {
    return catalog_;
}

bool DeviceClient::send(protocol::Command command, const QByteArray &payload) {
    if (protocol_ == nullptr) {
        return reject(QStringLiteral("协议客户端为空"), 0x02);
    }
    protocol_->sendRequest(command, payload);
    return true;
}

bool DeviceClient::reject(const QString &message, quint8 code) {
    reportError(code, message);
    return false;
}

void DeviceClient::reportError(quint8 code, const QString &detail) {
    const QString message =
        QStringLiteral("设备错误 0x%1：%2").arg(code, 2, 16, QLatin1Char('0'))
            .arg(detail.isEmpty() ? errorText(code) : detail);
    emit deviceError(message);
    emit terminalLog(message);
}

void DeviceClient::handleResponse(protocol::Frame frame) {
    if (frame.command == static_cast<quint8>(protocol::Command::Hello)) {
        if (!helloPending_ || frame.sequence != helloSequence_) {
            return;
        }
        helloPending_ = false;
        handshakeComplete_ = false;
    }
    if ((frame.flags & protocol::Error) != 0) {
        decodeResponseError(frame.payload);
        return;
    }
    switch (static_cast<protocol::Command>(frame.command)) {
    case protocol::Command::Hello:
        decodeHello(frame.payload);
        break;
    case protocol::Command::GetStatus:
        decodeStatusResponse(frame.payload);
        break;
    case protocol::Command::GetParamGroup:
    case protocol::Command::SetParamGroup:
        decodeParameterGroup(frame.payload);
        break;
    case protocol::Command::SetTelemetry:
        decodeTelemetryConfiguration(frame.payload);
        break;
    case protocol::Command::ImuCalibrate:
        decodeCalibrationState(frame.payload);
        break;
    default:
        break;
    }
}

void DeviceClient::handleEvent(protocol::Frame frame) {
    switch (static_cast<protocol::Command>(frame.command)) {
    case protocol::Command::ImuTelemetry:
        decodeImu(frame.payload);
        break;
    case protocol::Command::PidTelemetry:
        decodePid(frame.payload);
        break;
    case protocol::Command::StatusTelemetry:
        decodeStatus(frame.payload);
        break;
    default:
        break;
    }
}

void DeviceClient::handleRequestFailure(quint8 sequence, QString reason) {
    if (helloPending_ && sequence == helloSequence_) {
        helloPending_ = false;
        handshakeComplete_ = false;
    }
    if (reason == QStringLiteral("连接已断开")) {
        handshakeComplete_ = false;
        helloPending_ = false;
    }
    emit deviceError(reason);
    emit terminalLog(reason);
}

void DeviceClient::handleConnectionCleared() {
    handshakeComplete_ = false;
    helloPending_ = false;
}

void DeviceClient::decodeHello(const QByteArray &payload) {
    if (payload.size() != 8) {
        reportError(0x04, QStringLiteral("HELLO 响应长度错误"));
        return;
    }
    DeviceInfo info;
    info.protocolVersion = static_cast<quint8>(payload.at(0));
    info.firmwareMajor = static_cast<quint8>(payload.at(1));
    info.firmwareMinor = static_cast<quint8>(payload.at(2));
    info.firmwarePatch = static_cast<quint8>(payload.at(3));
    info.capabilities = readU32(payload, 4);
    info.protocol = info.protocolVersion;
    info.fwMajor = info.firmwareMajor;
    info.fwMinor = info.firmwareMinor;
    info.fwPatch = info.firmwarePatch;
    if (info.protocolVersion != 1) {
        handshakeComplete_ = false;
        reportError(0x01, QStringLiteral("设备协议版本不兼容"));
        return;
    }
    handshakeComplete_ = true;
    emit handshakeCompleted(info);
}

void DeviceClient::decodeParameterGroup(const QByteArray &payload) {
    if (payload.size() < 2) {
        reportError(0x04, QStringLiteral("参数组响应长度错误"));
        return;
    }
    const quint8 group = static_cast<quint8>(payload.at(0));
    const quint8 count = static_cast<quint8>(payload.at(1));
    if (catalog_.group(group).isEmpty()) {
        reportError(0x05, QStringLiteral("参数组不存在"));
        return;
    }
    int offset = 2;
    QVector<ParameterValue> values;
    values.reserve(count);
    for (int index = 0; index < count; ++index) {
        if (offset + 3 > payload.size()) {
            reportError(0x04, QStringLiteral("参数组项目头长度错误"));
            return;
        }
        const quint16 id = readU16(payload, offset);
        offset += 2;
        const auto type = static_cast<ValueType>(
            static_cast<quint8>(payload.at(offset++)));
        QVariant value;
        const TypedDecodeResult decodeResult =
            decodeTypedValue(payload, &offset, type, &value);
        if (decodeResult == TypedDecodeResult::Truncated) {
            reportError(0x04, QStringLiteral("参数组项目值长度错误"));
            return;
        }
        if (decodeResult != TypedDecodeResult::Ok) {
            reportError(0x06, QStringLiteral("参数组项目类型错误"));
            return;
        }

        const ParameterSpec *spec = catalog_.find(id);
        if (spec == nullptr) {
            reportError(0x05, QStringLiteral("参数不存在"));
            return;
        }
        if (spec->group != group) {
            reportError(0x05, QStringLiteral("参数不属于该参数组"));
            return;
        }
        if (spec->type != type) {
            reportError(0x06, QStringLiteral("参数类型错误"));
            return;
        }
        QString validationError;
        if (!catalog_.validate(id, value, &validationError)) {
            const quint8 code = validationError.contains(QStringLiteral("0x07"))
                ? 0x07
                : validationError.contains(QStringLiteral("0x06")) ? 0x06 : 0x05;
            reportError(code, validationError);
            return;
        }
        values.push_back(ParameterValue{id, type, value});
    }
    if (offset != payload.size()) {
        reportError(0x04, QStringLiteral("参数组响应包含多余数据"));
        return;
    }
    emit parameterGroupReceived(group, values);
}

void DeviceClient::decodeImu(const QByteArray &payload) {
    if (payload.size() != kImuPayloadWithTimestamp) {
        reportError(0x04, QStringLiteral("IMU 遥测长度错误"));
        return;
    }

    constexpr int offset = 4;
    ImuSample sample;
    sample.timestampMs = readU32(payload, 0);

    const qint16 rawAx = readI16(payload, offset);
    const qint16 rawAy = readI16(payload, offset + 2);
    const qint16 rawAz = readI16(payload, offset + 4);
    const qint16 rawGx = readI16(payload, offset + 6);
    const qint16 rawGy = readI16(payload, offset + 8);
    const qint16 rawGz = readI16(payload, offset + 10);
    const qint16 rawRoll = readI16(payload, offset + 12);
    const qint16 rawPitch = readI16(payload, offset + 14);
    const qint16 rawYaw = readI16(payload, offset + 16);
    sample.accelerationX = static_cast<double>(rawAx) / 32768.0 * 16.0;
    sample.accelerationY = static_cast<double>(rawAy) / 32768.0 * 16.0;
    sample.accelerationZ = static_cast<double>(rawAz) / 32768.0 * 16.0;
    sample.angularVelocityX = static_cast<double>(rawGx) / 32768.0 * 2000.0;
    sample.angularVelocityY = static_cast<double>(rawGy) / 32768.0 * 2000.0;
    sample.angularVelocityZ = static_cast<double>(rawGz) / 32768.0 * 2000.0;
    sample.rollDegrees = static_cast<double>(rawRoll) / 32768.0 * 180.0;
    sample.pitchDegrees = static_cast<double>(rawPitch) / 32768.0 * 180.0;
    sample.yawDegrees = static_cast<double>(rawYaw) / 32768.0 * 180.0;
    sample.ax = sample.accelerationX;
    sample.ay = sample.accelerationY;
    sample.az = sample.accelerationZ;
    sample.gx = sample.angularVelocityX;
    sample.gy = sample.angularVelocityY;
    sample.gz = sample.angularVelocityZ;
    sample.roll = sample.rollDegrees;
    sample.pitch = sample.pitchDegrees;
    sample.yaw = sample.yawDegrees;
    emit imuSampleReceived(sample);
}

void DeviceClient::decodePid(const QByteArray &payload) {
    if (payload.size() != kPidPayloadSize) {
        reportError(0x04, QStringLiteral("PID 遥测长度错误"));
        return;
    }
    PidSample sample;
    sample.timestampMs = readU32(payload, 0);
    sample.targetDegrees = static_cast<double>(readI16(payload, 4)) / 100.0;
    sample.actualDegrees = static_cast<double>(readI16(payload, 6)) / 100.0;
    sample.output = static_cast<double>(readI16(payload, 8)) / 100.0;
    sample.target = sample.targetDegrees;
    sample.actual = sample.actualDegrees;
    emit pidSampleReceived(sample);
}

void DeviceClient::decodeStatus(const QByteArray &payload) {
    if (payload.size() != 5) {
        reportError(0x04, QStringLiteral("状态遥测长度错误"));
        return;
    }
    DeviceStatus status;
    status.mode = static_cast<quint8>(payload.at(0));
    status.emergency = static_cast<quint8>(payload.at(1));
    status.unlocked = static_cast<quint8>(payload.at(2));
    status.lastError = readU16(payload, 3);
    emit statusReceived(status);
}

void DeviceClient::decodeStatusResponse(const QByteArray &payload) {
    if (payload.size() != 6) {
        reportError(0x04, QStringLiteral("状态响应长度错误"));
        return;
    }
    DeviceStatus status;
    status.mode = static_cast<quint8>(payload.at(0));
    status.emergency = static_cast<quint8>(payload.at(1));
    status.unlocked = static_cast<quint8>(payload.at(2));
    status.activeLink = static_cast<quint8>(payload.at(3));
    status.lastError = readU16(payload, 4);
    emit statusReceived(status);
}

void DeviceClient::decodeTelemetryConfiguration(const QByteArray &payload) {
    if (payload.size() != 3) {
        reportError(0x04, QStringLiteral("遥测配置响应长度错误"));
        return;
    }
    emit telemetryConfigured(static_cast<quint8>(payload.at(0)),
                             readU16(payload, 1));
}

void DeviceClient::decodeCalibrationState(const QByteArray &payload) {
    if (payload.size() != 1) {
        reportError(0x04, QStringLiteral("IMU 校准响应长度错误"));
        return;
    }
    const quint8 state = static_cast<quint8>(payload.at(0));
    if (state > 2) {
        reportError(0x04, QStringLiteral("IMU 校准状态无效"));
        return;
    }
    emit imuCalibrationStateChanged(state);
}

void DeviceClient::decodeResponseError(const QByteArray &payload) {
    if (payload.isEmpty()) {
        reportError(0x04, QStringLiteral("错误响应缺少错误码"));
        return;
    }
    const quint8 code = static_cast<quint8>(payload.at(0));
    reportError(code, errorText(code));
}
