#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QVariant>

#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>

#include "device/DeviceClient.h"
#include "protocol/FrameCodec.h"
#include "protocol/ProtocolClient.h"
#include "protocol/ProtocolTypes.h"
#include "support/FakeDevice.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool waitFor(const std::function<bool()> &condition, int timeoutMs = 1000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return condition();
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

QByteArray floatBytes(float value) {
    quint32 bits = 0;
    std::memcpy(&bits, &value, sizeof(value));
    QByteArray bytes;
    appendU32(&bytes, bits);
    return bytes;
}

QByteArray parameterSetPayload(quint16 id, ValueType type, float value) {
    QByteArray payload;
    payload.append(char(0x10));
    payload.append(char(1));
    appendU16(&payload, id);
    payload.append(static_cast<char>(type));
    payload.append(floatBytes(value));
    return payload;
}

QByteArray chassisActionPayload() {
    QByteArray payload;
    payload.append(char(0x01));
    appendU16(&payload, 10);
    appendU16(&payload, static_cast<quint16>(-10));
    appendU16(&payload, 5);
    appendU16(&payload, 100);
    return payload;
}

}  // namespace

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    ProtocolClient protocol(100);
    DeviceClient device(&protocol);
    FakeDevice fake(&protocol);

    QVector<DeviceInfo> handshakes;
    QVector<ParameterValue> latestParameters;
    QVector<ImuSample> imuSamples;
    QVector<DeviceStatus> statuses;
    int responseCount = 0;
    int errorResponseCount = 0;
    int requestFailureCount = 0;
    QString latestFailure;

    QObject::connect(&device, &DeviceClient::handshakeCompleted,
                     [&](DeviceInfo info) { handshakes.push_back(info); });
    QObject::connect(
        &device, &DeviceClient::parameterGroupReceived,
        [&](quint8, QVector<ParameterValue> values) {
            latestParameters = std::move(values);
        });
    QObject::connect(&device, &DeviceClient::imuSampleReceived,
                     [&](ImuSample sample) { imuSamples.push_back(sample); });
    QObject::connect(&device, &DeviceClient::statusReceived,
                     [&](DeviceStatus status) { statuses.push_back(status); });
    QObject::connect(
        &protocol, &ProtocolClient::responseReceived,
        [&](protocol::Frame frame) {
            ++responseCount;
            if ((frame.flags & protocol::Error) != 0) {
                ++errorResponseCount;
            }
        });
    QObject::connect(&protocol, &ProtocolClient::requestFailed,
                     [&](quint8, QString reason) {
                         ++requestFailureCount;
                         latestFailure = std::move(reason);
                     });

    if (!require(device.hello(), "HELLO was not sent") ||
        !require(waitFor([&] { return handshakes.size() == 1; }),
                 "HELLO response was not received") ||
        !require(device.handshakeComplete(),
                 "HELLO did not complete the debug handshake") ||
        !require(handshakes.front().protocolVersion == 1,
                 "HELLO returned the wrong protocol version")) {
        return 1;
    }

    latestParameters.clear();
    if (!require(device.getParameterGroup(0x10),
                 "PID parameter-group read was not sent") ||
        !require(waitFor([&] { return latestParameters.size() == 25; }),
                 "PID parameter pages were not aggregated") ||
        !require(latestParameters.front().id == 0x1000 &&
                     latestParameters.back().id == 0x1044,
                 "PID parameter ordering changed during readback")) {
        return 1;
    }

    const int responsesBeforeWrite = responseCount;
    const QVector<ParameterValue> validValues = {
        {0x1000, ValueType::Float32, QVariant(4.25)},
        {0x1001, ValueType::Float32, QVariant(0.75)},
    };
    if (!require(device.setParameterGroup(0x10, validValues),
                 "valid PID write was rejected locally") ||
        !require(waitFor([&] { return responseCount > responsesBeforeWrite; }),
                 "valid PID write ACK was not received") ||
        !require(std::fabs(fake.parameterValue(0x1000).toFloat() - 4.25f) <
                     0.001f,
                 "valid PID write did not change fake-device RAM")) {
        return 1;
    }

    latestParameters.clear();
    if (!require(device.getParameterGroup(0x10),
                 "PID readback request after write was not sent") ||
        !require(waitFor([&] { return latestParameters.size() == 25; }),
                 "PID readback after write was not received")) {
        return 1;
    }
    bool readbackChanged = false;
    for (const ParameterValue &value : latestParameters) {
        if (value.id == 0x1000 &&
            std::fabs(value.value.toFloat() - 4.25f) < 0.001f) {
            readbackChanged = true;
            break;
        }
    }
    if (!require(readbackChanged, "PID readback did not contain the new value")) {
        return 1;
    }

    const float valueBeforeInvalidWrite = fake.parameterValue(0x1000).toFloat();
    const int errorsBeforeInvalidWrite = errorResponseCount;
    protocol.sendRequest(protocol::Command::SetParamGroup,
                         parameterSetPayload(0x1000, ValueType::Float32,
                                              25.0f));
    if (!require(waitFor([&] {
                     return errorResponseCount > errorsBeforeInvalidWrite;
                 }),
                 "out-of-range PID write did not receive an error") ||
        !require(std::fabs(fake.parameterValue(0x1000).toFloat() -
                           valueBeforeInvalidWrite) < 0.001f,
                 "out-of-range PID write changed RAM")) {
        return 1;
    }

    imuSamples.clear();
    if (!require(device.setTelemetry(0x02, 100),
                 "IMU telemetry subscription was not sent") ||
        !require(waitFor([&] { return imuSamples.size() == 2; }),
                 "telemetry subscription did not deliver two IMU events") ||
        !require(std::fabs(imuSamples.front().accelerationX - 8.0) < 0.01 &&
                     std::fabs(imuSamples.front().angularVelocityX - 1000.0) <
                         0.1 &&
                     std::fabs(imuSamples.front().rollDegrees - 90.0) < 0.01,
                 "deterministic IMU conversion is incorrect")) {
        return 1;
    }

    const int actionsBeforeUnlock = fake.acceptedActionCount();
    const int errorsBeforeAction = errorResponseCount;
    protocol.sendRequest(protocol::Command::TestAction,
                         chassisActionPayload());
    if (!require(waitFor([&] { return errorResponseCount > errorsBeforeAction; }),
                 "TEST_ACTION before unlock was not rejected") ||
        !require(fake.acceptedActionCount() == actionsBeforeUnlock,
                 "TEST_ACTION before unlock was accepted")) {
        return 1;
    }

    const int errorsBeforeMalformedAction = errorResponseCount;
    protocol.sendRequest(protocol::Command::TestAction,
                         QByteArray(1, char(0x10)));
    if (!require(waitFor([&] {
                     return errorResponseCount > errorsBeforeMalformedAction;
                 }),
                 "malformed TEST_ACTION did not receive an error") ||
        !require(fake.lastErrorCode() == 0x04,
                 "malformed TEST_ACTION did not return LENGTH")) {
        return 1;
    }

    if (!require(device.unlockTests(), "TEST_UNLOCK was not sent") ||
        !require(waitFor([&] { return device.testsUnlocked(); }),
                 "TEST_UNLOCK did not enable actions") ||
        !require(device.testChassis(10, -10, 5, 100),
                 "unlocked TEST_ACTION was rejected locally") ||
        !require(waitFor([&] {
                     return fake.acceptedActionCount() == actionsBeforeUnlock + 1;
                 }),
                 "unlocked TEST_ACTION was not accepted")) {
        return 1;
    }

    statuses.clear();
    if (!require(device.emergencyStop(), "EMERGENCY_STOP was not sent") ||
        !require(waitFor([&] { return fake.emergencyLocked(); }),
                 "fake device did not enter emergency state") ||
        !require(waitFor([&] {
                     for (const DeviceStatus &status : statuses) {
                         if (status.emergency != 0) {
                             return true;
                         }
                     }
                     return false;
                 }),
                 "emergency status event was not received") ||
        !require(!device.testChassis(10, -10, 5, 100),
                 "TEST_ACTION after emergency stop was not rejected") ||
        !require(fake.acceptedActionCount() == actionsBeforeUnlock + 1,
                 "TEST_ACTION after emergency stop was accepted")) {
        return 1;
    }

    fake.setOnline(false);
    const int failuresBeforeDisconnect = requestFailureCount;
    if (!require(device.getStatus(),
                 "GET_STATUS pending request was not created")) {
        return 1;
    }
    protocol.clearPending();
    if (!require(requestFailureCount > failuresBeforeDisconnect &&
                     latestFailure == QString::fromUtf8("连接已断开"),
                 "disconnect did not fail the pending request") ||
        !require(!device.handshakeComplete() &&
                     !device.testActionsEnabled(),
                 "disconnect did not lock the device service")) {
        return 1;
    }

    return 0;
}
