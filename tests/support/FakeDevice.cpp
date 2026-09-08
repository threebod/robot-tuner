#include "support/FakeDevice.h"

#include <QTimer>

#include "protocol/FrameCodec.h"

#include <cmath>
#include <cstring>

namespace {

constexpr quint8 kPidGroup = 0x10;
constexpr int kParameterPageSize = 18;
constexpr quint8 kTelemetryStatus = 0x01;
constexpr quint8 kTelemetryImu = 0x02;
constexpr quint8 kTelemetryPid = 0x04;

quint16 readU16(const QByteArray &bytes, int offset) {
    return static_cast<quint16>(
        static_cast<quint8>(bytes.at(offset)) |
        (static_cast<quint16>(static_cast<quint8>(bytes.at(offset + 1))) <<
         8));
}

quint32 readU32(const QByteArray &bytes, int offset) {
    return static_cast<quint32>(
        static_cast<quint8>(bytes.at(offset)) |
        (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 1))) <<
         8) |
        (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 2))) <<
         16) |
        (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 3))) <<
         24));
}

qint16 readI16(const QByteArray &bytes, int offset) {
    return static_cast<qint16>(readU16(bytes, offset));
}

float readF32(const QByteArray &bytes, int offset) {
    const quint32 bits = readU32(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool f32InRange(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
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

void appendI16(QByteArray *bytes, qint16 value) {
    appendU16(bytes, static_cast<quint16>(value));
}

int valueWidth(ValueType type) {
    switch (type) {
    case ValueType::UInt8:
    case ValueType::Int8:
        return 1;
    case ValueType::UInt16:
    case ValueType::Int16:
        return 2;
    case ValueType::UInt32:
    case ValueType::Int32:
    case ValueType::Float32:
        return 4;
    }
    return 0;
}

bool decodeValue(const QByteArray &payload, int *offset, ValueType type,
                 QVariant *value) {
    const int width = valueWidth(type);
    if (width == 0 || *offset < 0 || *offset + width > payload.size()) {
        return false;
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
        *value = QVariant::fromValue(
            static_cast<qint32>(readU32(payload, *offset)));
        break;
    case ValueType::Float32: {
        const quint32 bits = readU32(payload, *offset);
        float converted = 0.0f;
        std::memcpy(&converted, &bits, sizeof(converted));
        if (!std::isfinite(converted)) {
            return false;
        }
        *value = QVariant::fromValue(converted);
        break;
    }
    }
    *offset += width;
    return true;
}

bool encodeValue(QByteArray *payload, ValueType type,
                 const QVariant &value) {
    bool ok = false;
    switch (type) {
    case ValueType::UInt8:
        payload->append(static_cast<char>(value.toUInt(&ok)));
        return ok;
    case ValueType::UInt16:
        appendU16(payload, static_cast<quint16>(value.toUInt(&ok)));
        return ok;
    case ValueType::UInt32:
        appendU32(payload, value.toUInt(&ok));
        return ok;
    case ValueType::Int8:
        payload->append(static_cast<char>(value.toInt(&ok)));
        return ok;
    case ValueType::Int16:
        appendU16(payload, static_cast<quint16>(value.toInt(&ok)));
        return ok;
    case ValueType::Int32:
        appendU32(payload, static_cast<quint32>(value.toInt(&ok)));
        return ok;
    case ValueType::Float32: {
        const float converted = value.toFloat(&ok);
        if (!ok || !std::isfinite(converted)) {
            return false;
        }
        quint32 bits = 0;
        std::memcpy(&bits, &converted, sizeof(converted));
        appendU32(payload, bits);
        return true;
    }
    }
    return false;
}

QVariant defaultValue(const ParameterSpec &spec, int index) {
    static constexpr float pidDefaults[5][5] = {
        {2.0f, 0.0f, 0.8f, 7.0f, 230.0f},
        {3.3f, 0.0f, 1.8f, 7.0f, 30.0f},
        {5.0f, 0.0f, 5.0f, 7.0f, 230.0f},
        {2.0f, 0.05f, 0.4f, 7.0f, 230.0f},
        {2.0f, 0.07f, 0.4f, 7.0f, 230.0f},
    };

    if (spec.group == kPidGroup) {
        const int profile = index / 5;
        const int field = index % 5;
        return QVariant::fromValue(pidDefaults[profile][field]);
    }
    switch (spec.id) {
    case 0x2000:
    case 0x2001:
        return QVariant::fromValue(qint32(80));
    case 0x2002:
        return QVariant::fromValue(qint32(30));
    case 0x2003:
        return QVariant::fromValue(quint16(1000));
    case 0x2004:
        return QVariant::fromValue(quint16(230));
    case 0x3000:
    case 0x3001:
        return QVariant::fromValue(0.0f);
    case 0x3002:
        return QVariant::fromValue(180.0f);
    case 0x3003:
        return QVariant::fromValue(quint16(100));
    case 0x3004:
        return QVariant::fromValue(quint8(1));
    case 0x3005:
        return QVariant::fromValue(1.0f);
    case 0x4000:
        return QVariant::fromValue(quint16(10));
    default:
        return {};
    }
}

QByteArray imuPayload(quint32 timestamp, qint16 accelerationX,
                      qint16 accelerationY, qint16 accelerationZ,
                      qint16 angularVelocityX, qint16 angularVelocityY,
                      qint16 angularVelocityZ, qint16 roll, qint16 pitch,
                      qint16 yaw) {
    QByteArray payload;
    appendU32(&payload, timestamp);
    appendI16(&payload, accelerationX);
    appendI16(&payload, accelerationY);
    appendI16(&payload, accelerationZ);
    appendI16(&payload, angularVelocityX);
    appendI16(&payload, angularVelocityY);
    appendI16(&payload, angularVelocityZ);
    appendI16(&payload, roll);
    appendI16(&payload, pitch);
    appendI16(&payload, yaw);
    return payload;
}

}  // namespace

FakeDevice::FakeDevice(ProtocolClient *protocol, QObject *parent)
    : QObject(parent), protocol_(protocol) {
    if (protocol_ == nullptr) {
        return;
    }

    const QVector<ParameterSpec> specs = catalog_.group(0x10) +
                                         catalog_.group(0x20) +
                                         catalog_.group(0x30) +
                                         catalog_.group(0x40);
    for (int index = 0; index < specs.size(); ++index) {
        parameters_.insert(specs.at(index).id,
                           defaultValue(specs.at(index), index));
    }

    bytesConnection_ = QObject::connect(
        protocol_, &ProtocolClient::bytesReady, this,
        [this](QByteArray bytes) { handleBytes(std::move(bytes)); });
}

FakeDevice::FakeDevice(ProtocolClient &protocol, QObject *parent)
    : FakeDevice(&protocol, parent) {}

FakeDevice::~FakeDevice() {
    QObject::disconnect(bytesConnection_);
}

void FakeDevice::setOnline(bool online) {
    online_ = online;
}

bool FakeDevice::online() const {
    return online_;
}

QVariant FakeDevice::parameterValue(quint16 id) const {
    return parameters_.value(id);
}

int FakeDevice::acceptedActionCount() const {
    return acceptedActionCount_;
}

int FakeDevice::rejectedActionCount() const {
    return rejectedActionCount_;
}

quint8 FakeDevice::lastErrorCode() const {
    return lastErrorCode_;
}

bool FakeDevice::emergencyLocked() const {
    return emergencyLocked_;
}

void FakeDevice::handleBytes(QByteArray bytes) {
    if (!online_) {
        return;
    }
    const QVector<protocol::Frame> frames = parser_.push(QByteArrayView(bytes));
    for (const protocol::Frame &frame : frames) {
        if ((frame.flags & protocol::Request) == 0) {
            continue;
        }
        QTimer::singleShot(0, this, [this, frame] {
            if (online_) {
                handleFrame(frame);
            }
        });
    }
}

void FakeDevice::handleFrame(protocol::Frame request) {
    const auto command = static_cast<protocol::Command>(request.command);
    const auto requireHandshake = [this, &request]() {
        if (handshakeComplete_) {
            return true;
        }
        sendError(request, 0x09);
        return false;
    };

    switch (command) {
    case protocol::Command::Hello: {
        if (!request.payload.isEmpty()) {
            sendError(request, 0x04);
            return;
        }
        handshakeComplete_ = true;
        testsUnlocked_ = false;
        QByteArray hello;
        hello.append(char(1));
        hello.append(char(0));
        hello.append(char(0));
        hello.append(char(0));
        appendU32(&hello, 0);
        sendResponse(request, hello);
        return;
    }

    case protocol::Command::GetStatus: {
        if (!requireHandshake()) {
            return;
        }
        if (!request.payload.isEmpty()) {
            sendError(request, 0x04);
            return;
        }
        QByteArray status;
        status.append(char(1));
        status.append(static_cast<char>(emergencyLocked_ ? 1 : 0));
        status.append(static_cast<char>(testsUnlocked_ ? 1 : 0));
        status.append(char(0));
        appendU16(&status, lastErrorCode_);
        sendResponse(request, status);
        return;
    }

    case protocol::Command::GetParamGroup: {
        if (!requireHandshake()) {
            return;
        }
        if (request.payload.size() < 1 || request.payload.size() > 2) {
            sendError(request, 0x04);
            return;
        }
        const quint8 group = static_cast<quint8>(request.payload.at(0));
        const QVector<ParameterSpec> specs = catalog_.group(group);
        const int page = request.payload.size() == 2
                             ? static_cast<quint8>(request.payload.at(1))
                             : 0;
        const int first = page * kParameterPageSize;
        if (specs.isEmpty()) {
            sendError(request, 0x05);
            return;
        }
        if (first < 0 || first >= specs.size()) {
            sendError(request, 0x05);
            return;
        }
        const int count = qMin(kParameterPageSize, specs.size() - first);
        QByteArray payload;
        payload.append(static_cast<char>(group));
        payload.append(static_cast<char>(count));
        for (int index = 0; index < count; ++index) {
            const ParameterSpec &spec = specs.at(first + index);
            appendU16(&payload, spec.id);
            payload.append(static_cast<char>(spec.type));
            if (!encodeValue(&payload, spec.type, parameters_.value(spec.id))) {
                sendError(request, 0x04);
                return;
            }
        }
        sendResponse(request, payload);
        return;
    }

    case protocol::Command::SetParamGroup: {
        if (!requireHandshake()) {
            return;
        }
        if (request.payload.size() < 2) {
            sendError(request, 0x04);
            return;
        }
        const quint8 group = static_cast<quint8>(request.payload.at(0));
        const int count = static_cast<quint8>(request.payload.at(1));
        if (count <= 0) {
            sendError(request, 0x04);
            return;
        }
        int offset = 2;
        QVector<StagedValue> staged;
        staged.reserve(count);
        for (int index = 0; index < count; ++index) {
            if (offset + 3 > request.payload.size()) {
                sendError(request, 0x04);
                return;
            }
            const quint16 id = readU16(request.payload, offset);
            offset += 2;
            const auto type = static_cast<ValueType>(
                static_cast<quint8>(request.payload.at(offset++)));
            const ParameterSpec *spec = catalog_.find(id);
            if (spec == nullptr || spec->group != group) {
                sendError(request, 0x05);
                return;
            }
            if (spec->type != type) {
                sendError(request, 0x06);
                return;
            }
            QVariant value;
            if (!decodeValue(request.payload, &offset, type, &value)) {
                sendError(request, 0x04);
                return;
            }
            for (const StagedValue &prior : staged) {
                if (prior.id == id) {
                    sendError(request, 0x05);
                    return;
                }
            }
            QString validationError;
            if (!catalog_.validate(id, value, &validationError)) {
                sendError(request, validationError.contains(QStringLiteral("0x07"))
                                     ? 0x07
                                     : 0x06);
                return;
            }
            staged.push_back({id, type, value});
        }
        if (offset != request.payload.size()) {
            sendError(request, 0x04);
            return;
        }

        QByteArray applied;
        applied.append(static_cast<char>(group));
        applied.append(static_cast<char>(staged.size()));
        for (const StagedValue &value : staged) {
            appendU16(&applied, value.id);
            applied.append(static_cast<char>(value.type));
            if (!encodeValue(&applied, value.type, value.value)) {
                sendError(request, 0x04);
                return;
            }
        }
        for (const StagedValue &value : staged) {
            parameters_.insert(value.id, value.value);
        }
        sendResponse(request, applied);
        return;
    }

    case protocol::Command::SetTelemetry: {
        if (!requireHandshake()) {
            return;
        }
        if (request.payload.size() != 3) {
            sendError(request, 0x04);
            return;
        }
        const quint16 period = readU16(request.payload, 1);
        if (period < 20 || period > 1000) {
            sendError(request, 0x07);
            return;
        }
        telemetryMask_ = static_cast<quint8>(request.payload.at(0));
        telemetryPeriodMs_ = period;
        QByteArray accepted;
        accepted.append(static_cast<char>(telemetryMask_));
        appendU16(&accepted, telemetryPeriodMs_);
        sendResponse(request, accepted);
        sendTelemetryEvents();
        return;
    }

    case protocol::Command::ImuCalibrate:
        if (!requireHandshake()) {
            return;
        }
        if (!request.payload.isEmpty()) {
            sendError(request, 0x04);
            return;
        }
        sendResponse(request, QByteArray(1, char(0)));
        return;

    case protocol::Command::TestUnlock: {
        if (!requireHandshake()) {
            return;
        }
        if (!request.payload.isEmpty()) {
            sendError(request, 0x04);
            return;
        }
        if (emergencyLocked_) {
            sendError(request, 0x0a);
            return;
        }
        testsUnlocked_ = true;
        QByteArray duration;
        appendU16(&duration, 30000);
        sendResponse(request, duration);
        return;
    }

    case protocol::Command::TestAction: {
        if (!requireHandshake()) {
            ++rejectedActionCount_;
            return;
        }
        if (request.payload.isEmpty()) {
            ++rejectedActionCount_;
            sendError(request, 0x04);
            return;
        }
        const quint8 action = static_cast<quint8>(request.payload.at(0));
        quint8 validationError = 0;
        switch (action) {
        case 0x01:
            if (request.payload.size() != 9) {
                validationError = 0x04;
            } else {
                const qint16 vx = readI16(request.payload, 1);
                const qint16 vy = readI16(request.payload, 3);
                const qint16 w = readI16(request.payload, 5);
                const quint16 duration = readU16(request.payload, 7);
                if (vx < -80 || vx > 80 || vy < -80 || vy > 80 || w < -30 ||
                    w > 30 || duration < 50 || duration > 1000) {
                    validationError = 0x07;
                }
            }
            break;
        case 0x10:
        case 0x11: {
            if (request.payload.size() != 8) {
                validationError = 0x04;
                break;
            }
            const bool horizontal = action == 0x10;
            const float target = readF32(request.payload, 1);
            const quint16 speed = readU16(request.payload, 5);
            const quint8 acceleration =
                static_cast<quint8>(request.payload.at(7));
            if (!f32InRange(target, horizontal ? -120.0f : 0.0f,
                           horizontal ? 63.0f : 50.0f) ||
                speed < 100 || speed > 2000 || acceleration < 1 ||
                acceleration > 220) {
                validationError = 0x07;
            }
            break;
        }
        case 0x12:
            if (request.payload.size() != 9) {
                validationError = 0x04;
            } else if (!f32InRange(readF32(request.payload, 1), 135.0f,
                                   295.0f) ||
                       !f32InRange(readF32(request.payload, 5), 1.0f, 20.0f)) {
                validationError = 0x07;
            }
            break;
        case 0x20:
            if (request.payload.size() != 2) {
                validationError = 0x04;
            } else if (static_cast<quint8>(request.payload.at(1)) < 1 ||
                       static_cast<quint8>(request.payload.at(1)) > 3) {
                validationError = 0x07;
            }
            break;
        case 0x21:
            if (request.payload.size() != 2) {
                validationError = 0x04;
            } else if (static_cast<quint8>(request.payload.at(1)) > 1) {
                validationError = 0x07;
            }
            break;
        default:
            validationError = 0x02;
            break;
        }
        if (validationError != 0) {
            ++rejectedActionCount_;
            sendError(request, validationError);
            return;
        }
        if (emergencyLocked_) {
            ++rejectedActionCount_;
            sendError(request, 0x0a);
            return;
        }
        if (!testsUnlocked_) {
            ++rejectedActionCount_;
            sendError(request, 0x09);
            return;
        }
        ++acceptedActionCount_;
        sendResponse(request, {});
        return;
    }

    case protocol::Command::Stop:
        if (!request.payload.isEmpty()) {
            sendError(request, 0x04);
            return;
        }
        testsUnlocked_ = false;
        sendResponse(request, {});
        return;

    case protocol::Command::EmergencyStop:
        if (!request.payload.isEmpty()) {
            sendError(request, 0x04);
            return;
        }
        testsUnlocked_ = false;
        emergencyLocked_ = true;
        sendResponse(request, {});
        sendEvent(static_cast<quint8>(protocol::Command::StatusTelemetry),
                  QByteArray::fromHex("01 01 00 00 00"));
        return;

    case protocol::Command::ClearEmergencyStop:
        if (!request.payload.isEmpty()) {
            sendError(request, 0x04);
            return;
        }
        if (!emergencyLocked_) {
            sendError(request, 0x08);
            return;
        }
        emergencyLocked_ = false;
        sendResponse(request, {});
        return;

    default:
        sendError(request, 0x02);
        return;
    }
}

void FakeDevice::sendResponse(const protocol::Frame &request,
                              const QByteArray &payload) {
    lastErrorCode_ = 0;
    protocol::Frame response = request;
    response.flags = protocol::Response;
    response.payload = payload;
    sendFrame(std::move(response));
}

void FakeDevice::sendError(const protocol::Frame &request, quint8 code) {
    lastErrorCode_ = code;
    protocol::Frame response = request;
    response.flags = protocol::Response | protocol::Error;
    response.payload = QByteArray(1, static_cast<char>(code));
    sendFrame(std::move(response));
}

void FakeDevice::sendEvent(quint8 command, const QByteArray &payload) {
    protocol::Frame event;
    event.flags = protocol::Event;
    event.sequence = 0;
    event.command = command;
    event.payload = payload;
    sendFrame(std::move(event));
}

void FakeDevice::sendFrame(protocol::Frame frame) {
    if (protocol_ == nullptr || !online_) {
        return;
    }
    const QByteArray bytes = encodeFrame(frame);
    if (bytes.isEmpty()) {
        return;
    }
    QTimer::singleShot(0, this, [this, bytes] {
        if (protocol_ != nullptr && online_) {
            protocol_->ingestBytes(QByteArrayView(bytes));
        }
    });
}

void FakeDevice::sendTelemetryEvents() {
    if ((telemetryMask_ & kTelemetryStatus) != 0) {
        QByteArray status;
        status.append(char(1));
        status.append(static_cast<char>(emergencyLocked_ ? 1 : 0));
        status.append(static_cast<char>(testsUnlocked_ ? 1 : 0));
        appendU16(&status, lastErrorCode_);
        sendEvent(static_cast<quint8>(protocol::Command::StatusTelemetry),
                  status);
    }
    if ((telemetryMask_ & kTelemetryImu) != 0) {
        sendEvent(static_cast<quint8>(protocol::Command::ImuTelemetry),
                  imuPayload(telemetryTimestampMs_, 16384, -8192, 8192,
                             16384, -8192, 8192, 16384, -8192, 8192));
        sendEvent(static_cast<quint8>(protocol::Command::ImuTelemetry),
                  imuPayload(telemetryTimestampMs_ + telemetryPeriodMs_,
                             8192, -4096, 4096, 8192, -4096, 4096, 8192,
                             -4096, 4096));
        telemetryTimestampMs_ += static_cast<quint32>(telemetryPeriodMs_ * 2);
    }
    if ((telemetryMask_ & kTelemetryPid) != 0) {
        QByteArray pid;
        appendU32(&pid, telemetryTimestampMs_);
        appendI16(&pid, 9000);
        appendI16(&pid, 8500);
        appendI16(&pid, 1200);
        sendEvent(static_cast<quint8>(protocol::Command::PidTelemetry), pid);
    }
}
