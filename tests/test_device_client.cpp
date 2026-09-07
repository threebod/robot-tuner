#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QVariant>

#include <cmath>
#include <cstring>
#include <iostream>

#include "device/DeviceClient.h"
#include "protocol/FrameCodec.h"
#include "protocol/FrameParser.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

quint16 readU16(const QByteArray &bytes, int offset) {
    return static_cast<quint16>(
        static_cast<quint8>(bytes.at(offset)) |
        (static_cast<quint16>(static_cast<quint8>(bytes.at(offset + 1))) << 8));
}

void appendU16(QByteArray *bytes, quint16 value) {
    bytes->append(static_cast<char>(value & 0xff));
    bytes->append(static_cast<char>((value >> 8) & 0xff));
}

void appendI16(QByteArray *bytes, qint16 value) {
    appendU16(bytes, static_cast<quint16>(value));
}

void appendU32(QByteArray *bytes, quint32 value) {
    bytes->append(static_cast<char>(value & 0xff));
    bytes->append(static_cast<char>((value >> 8) & 0xff));
    bytes->append(static_cast<char>((value >> 16) & 0xff));
    bytes->append(static_cast<char>((value >> 24) & 0xff));
}

protocol::Frame capturedFrame(const QByteArray &bytes) {
    FrameParser parser;
    const QVector<protocol::Frame> frames = parser.push(QByteArrayView(bytes));
    return frames.isEmpty() ? protocol::Frame{} : frames.front();
}

QByteArray floatBytes(float value) {
    QByteArray bytes;
    quint32 bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(&bytes, bits);
    return bytes;
}

QByteArray parameterResponsePayload(quint8 group, quint16 id, ValueType type,
                                    const QByteArray &valueBytes) {
    QByteArray payload;
    payload.append(static_cast<char>(group));
    payload.append(static_cast<char>(1));
    appendU16(&payload, id);
    payload.append(static_cast<char>(type));
    payload.append(valueBytes);
    return payload;
}

void feedResponse(ProtocolClient *protocol, const QByteArray &requestBytes,
                  protocol::Command command, const QByteArray &payload,
                  quint8 flags = protocol::Response) {
    protocol::Frame response = capturedFrame(requestBytes);
    response.flags = flags;
    response.command = static_cast<quint8>(command);
    response.payload = payload;
    protocol->ingestBytes(QByteArrayView(encodeFrame(response)));
}

}  // namespace

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    ProtocolClient protocol;
    DeviceClient device(&protocol);

    QByteArray requestBytes;
    QObject::connect(&protocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { requestBytes = bytes; });

    device.getParameterGroup(0x10);
    FrameParser parser;
    const QVector<protocol::Frame> groupRequests =
        parser.push(QByteArrayView(requestBytes));
    if (!require(groupRequests.size() == 1,
                 "GET_PARAM_GROUP must emit one request frame") ||
        !require(groupRequests.front().command ==
                     static_cast<quint8>(protocol::Command::GetParamGroup),
                 "GET_PARAM_GROUP command is incorrect") ||
        !require(groupRequests.front().payload == QByteArray(1, '\x10'),
                 "GET_PARAM_GROUP payload must contain only the group")) {
        return 1;
    }

    ProtocolClient lockedProtocol;
    DeviceClient lockedDevice(&lockedProtocol);
    int lockedBytes = 0;
    int lockedErrors = 0;
    QObject::connect(&lockedProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &) { ++lockedBytes; });
    QObject::connect(&lockedDevice, &DeviceClient::deviceError,
                     [&](const QString &) { ++lockedErrors; });
    QVector<ParameterValue> lockedValues;
    lockedValues.push_back({0x1000, ValueType::Float32, QVariant(1.5)});
    if (!require(!lockedDevice.setParameterGroup(0x10, lockedValues),
                 "SET_PARAM_GROUP was allowed before HELLO") ||
        !require(!lockedDevice.setTelemetry(1, 100),
                 "SET_TELEMETRY was allowed before HELLO") ||
        !require(!lockedDevice.calibrateImu(),
                 "IMU calibration was allowed before HELLO") ||
        !require(lockedBytes == 0 && lockedErrors == 3,
                 "pre-handshake commands did not fail locally")) {
        return 1;
    }

    requestBytes.clear();
    if (!require(device.hello(), "HELLO request was not sent")) {
        return 1;
    }
    const protocol::Frame helloRequest = capturedFrame(requestBytes);
    QByteArray helloPayload;
    helloPayload.append(char(1));
    helloPayload.append(char(2));
    helloPayload.append(char(3));
    helloPayload.append(char(4));
    appendU32(&helloPayload, 0x11223344);
    feedResponse(&protocol, requestBytes, protocol::Command::Hello,
                 helloPayload);
    if (!require(device.handshakeComplete(),
                 "a successful HELLO did not unlock the device service") ||
        !require(helloRequest.command ==
                     static_cast<quint8>(protocol::Command::Hello),
                 "HELLO command is incorrect")) {
        return 1;
    }

    requestBytes.clear();
    QVector<ParameterValue> values;
    values.push_back({0x1000, ValueType::Float32, QVariant(1.5)});
    values.push_back({0x1001, ValueType::Float32, QVariant(0.25)});
    if (!require(device.setParameterGroup(0x10, values),
                 "valid parameter group was rejected locally")) {
        return 1;
    }
    const QVector<protocol::Frame> setRequests =
        parser.push(QByteArrayView(requestBytes));
    const QByteArray expectedPayload = QByteArray::fromHex(
        "10 02 00 10 07 00 00 C0 3F 01 10 07 00 00 80 3E");
    if (!require(setRequests.size() == 1,
                 "SET_PARAM_GROUP must emit one request frame") ||
        !require(setRequests.front().payload == expectedPayload,
                 "SET_PARAM_GROUP typed payload layout is incorrect")) {
        return 1;
    }

    int deviceErrorCount = 0;
    QString latestError;
    QObject::connect(&device, &DeviceClient::deviceError,
                     [&](const QString &error) {
                         ++deviceErrorCount;
                         latestError = error;
                     });
    requestBytes.clear();
    values[0].value = QVariant(21.0);
    if (!require(!device.setParameterGroup(0x10, values),
                 "an out-of-range parameter group was not rejected") ||
        !require(requestBytes.isEmpty(),
                 "an invalid group must not send a request") ||
        !require(deviceErrorCount == 1 && latestError.contains(QStringLiteral("0x07")),
                 "local range errors must preserve the protocol error code")) {
        return 1;
    }

    int parameterGroupCount = 0;
    QObject::connect(&device, &DeviceClient::parameterGroupReceived,
                     [&](quint8, const QVector<ParameterValue> &) {
                         ++parameterGroupCount;
                     });
    requestBytes.clear();
    device.getParameterGroup(0x10);
    feedResponse(&protocol, requestBytes, protocol::Command::GetParamGroup,
                 parameterResponsePayload(0x10, 0x1000, ValueType::Float32,
                                          floatBytes(1.5f)));
    if (!require(parameterGroupCount == 1,
                 "a valid parameter readback was rejected")) {
        return 1;
    }

    const struct {
        quint16 id;
        ValueType type;
        QByteArray value;
        const char *message;
    } invalidReadbacks[] = {
        {0x7fff, ValueType::Float32, floatBytes(1.5f),
         "unknown readback parameter was accepted"},
        {0x2000, ValueType::Int32, QByteArray::fromHex("01000000"),
         "readback parameter from another group was accepted"},
        {0x1000, ValueType::UInt16, QByteArray::fromHex("0100"),
         "readback type mismatch was accepted"},
        {0x1000, ValueType::Float32, floatBytes(21.0f),
         "out-of-range readback parameter was accepted"},
    };
    for (const auto &invalid : invalidReadbacks) {
        requestBytes.clear();
        device.getParameterGroup(0x10);
        const int groupsBefore = parameterGroupCount;
        const int errorsBefore = deviceErrorCount;
        feedResponse(&protocol, requestBytes, protocol::Command::GetParamGroup,
                     parameterResponsePayload(0x10, invalid.id, invalid.type,
                                              invalid.value));
        if (!require(parameterGroupCount == groupsBefore, invalid.message) ||
            !require(deviceErrorCount == errorsBefore + 1,
                     "invalid readback did not report an error")) {
            return 1;
        }
    }

    requestBytes.clear();
    device.getParameterGroup(0x10);
    const int truncatedErrorsBefore = deviceErrorCount;
    feedResponse(&protocol, requestBytes, protocol::Command::GetParamGroup,
                 parameterResponsePayload(0x10, 0x1000, ValueType::Float32,
                                          QByteArray::fromHex("0000")));
    if (!require(deviceErrorCount == truncatedErrorsBefore + 1,
                 "truncated typed value did not report an error") ||
        !require(latestError.contains(QStringLiteral("0x04")),
                 "truncated typed value did not report LENGTH")) {
        return 1;
    }

    int imuCount = 0;
    ImuSample sample;
    QObject::connect(&device, &DeviceClient::imuSampleReceived,
                     [&](const ImuSample &received) {
                         ++imuCount;
                         sample = received;
                     });
    protocol::Frame imuEvent;
    imuEvent.flags = protocol::Event;
    imuEvent.sequence = 0;
    imuEvent.command = static_cast<quint8>(protocol::Command::ImuTelemetry);
    appendU32(&imuEvent.payload, 1234);
    appendI16(&imuEvent.payload, 16384);
    appendI16(&imuEvent.payload, -16384);
    appendI16(&imuEvent.payload, 8192);
    appendI16(&imuEvent.payload, 16384);
    appendI16(&imuEvent.payload, -16384);
    appendI16(&imuEvent.payload, 8192);
    appendI16(&imuEvent.payload, 16384);
    appendI16(&imuEvent.payload, -16384);
    appendI16(&imuEvent.payload, 8192);
    protocol.ingestBytes(QByteArrayView(encodeFrame(imuEvent)));
    if (!require(imuCount == 1, "an IMU event was not decoded") ||
        !require(sample.timestampMs == 1234, "IMU timestamp was not decoded") ||
        !require(std::abs(sample.accelerationX - 8.0) < 1e-9 &&
                     std::abs(sample.accelerationY + 8.0) < 1e-9 &&
                     std::abs(sample.accelerationZ - 4.0) < 1e-9,
                 "IMU acceleration conversion is incorrect") ||
        !require(std::abs(sample.angularVelocityX - 1000.0) < 1e-9 &&
                     std::abs(sample.angularVelocityY + 1000.0) < 1e-9 &&
                     std::abs(sample.angularVelocityZ - 500.0) < 1e-9,
                 "IMU angular velocity conversion is incorrect") ||
        !require(std::abs(sample.rollDegrees - 90.0) < 1e-9 &&
                     std::abs(sample.pitchDegrees + 90.0) < 1e-9 &&
                     std::abs(sample.yawDegrees - 45.0) < 1e-9,
                 "IMU angle conversion is incorrect")) {
        return 1;
    }

    protocol::Frame shortImu = imuEvent;
    shortImu.payload.remove(0, 4);
    const int shortImuErrorsBefore = deviceErrorCount;
    protocol.ingestBytes(QByteArrayView(encodeFrame(shortImu)));
    if (!require(imuCount == 1,
                 "an 18-byte IMU event was incorrectly accepted") ||
        !require(deviceErrorCount == shortImuErrorsBefore + 1 &&
                     latestError.contains(QStringLiteral("0x04")),
                 "an IMU event without timestamp did not report LENGTH")) {
        return 1;
    }

    protocol::Frame invalidStatus = imuEvent;
    invalidStatus.command =
        static_cast<quint8>(protocol::Command::StatusTelemetry);
    invalidStatus.payload = QByteArray::fromHex("01000000000000");
    const int statusErrorsBefore = deviceErrorCount;
    protocol.ingestBytes(QByteArrayView(encodeFrame(invalidStatus)));
    if (!require(deviceErrorCount == statusErrorsBefore + 1 &&
                     latestError.contains(QStringLiteral("0x04")),
                 "a non-five-byte status event was accepted")) {
        return 1;
    }

    requestBytes.clear();
    device.hello();
    feedResponse(&protocol, requestBytes, protocol::Command::Hello,
                 QByteArray::fromHex("02 02 03 04 44 33 22 11"));
    if (!require(!device.handshakeComplete(),
                 "an incompatible HELLO response kept the service unlocked")) {
        return 1;
    }
    requestBytes.clear();
    device.hello();
    feedResponse(&protocol, requestBytes, protocol::Command::Hello,
                 helloPayload);
    if (!require(device.handshakeComplete(),
                 "a second successful HELLO did not unlock the service")) {
        return 1;
    }
    requestBytes.clear();
    device.hello();
    feedResponse(&protocol, requestBytes, protocol::Command::Hello,
                 QByteArray(1, char(0x07)), protocol::Response | protocol::Error);
    if (!require(!device.handshakeComplete(),
                 "a HELLO error response kept the service unlocked")) {
        return 1;
    }
    requestBytes.clear();
    device.hello();
    protocol.clearPending();
    if (!require(!device.handshakeComplete(),
                 "disconnect did not clear the handshake state")) {
        return 1;
    }

    return 0;
}
