#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QVariant>

#include <cmath>
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

    return 0;
}
