#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
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

QByteArray pidPagePayload(int firstIndex, int count) {
    QByteArray payload;
    payload.append(char(0x10));
    payload.append(static_cast<char>(count));
    for (int index = 0; index < count; ++index) {
        const int parameterIndex = firstIndex + index;
        const quint16 id = static_cast<quint16>(
            0x1000 + (parameterIndex / 5) * 0x10 + parameterIndex % 5);
        appendU16(&payload, id);
        payload.append(static_cast<char>(ValueType::Float32));
        const int profile = parameterIndex / 5;
        const int offset = parameterIndex % 5;
        const float value = offset == 0   ? static_cast<float>(profile + 1)
                            : offset == 1 ? 0.5f + profile * 0.1f
                            : offset == 2 ? static_cast<float>(profile + 2)
                            : offset == 3 ? static_cast<float>(10 + profile)
                                           : static_cast<float>(20 + profile);
        payload.append(floatBytes(value));
    }
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

void emitResponse(ProtocolClient *protocol, const QByteArray &requestBytes,
                  protocol::Command command, const QByteArray &payload,
                  quint8 sequence, quint8 flags = protocol::Response) {
    protocol::Frame response = capturedFrame(requestBytes);
    response.flags = flags;
    response.sequence = sequence;
    response.command = static_cast<quint8>(command);
    response.payload = payload;
    protocol->responseReceived(response);
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

    int aggregatedPidCount = 0;
    QVector<ParameterValue> aggregatedPidValues;
    QObject::connect(&device, &DeviceClient::parameterGroupReceived,
                     [&](quint8 group, const QVector<ParameterValue> &values) {
                         if (group == 0x10) {
                             ++aggregatedPidCount;
                             aggregatedPidValues = values;
                         }
                     });
    requestBytes.clear();
    if (!require(device.getParameterGroup(0x10),
                 "PID parameter group request was not sent")) {
        return 1;
    }
    const QByteArray pidPage0Request = requestBytes;
    if (!require(capturedFrame(pidPage0Request).payload ==
                     QByteArray::fromHex("10"),
                 "PID page zero request payload is incorrect")) {
        return 1;
    }
    if (!require(device.getParameterGroup(0x10),
                 "duplicate PID parameter group request was rejected") ||
        !require(requestBytes == pidPage0Request,
                 "duplicate PID parameter group request was sent")) {
        return 1;
    }
    feedResponse(&protocol, pidPage0Request, protocol::Command::GetParamGroup,
                 pidPagePayload(0, 18));
    if (!require(aggregatedPidCount == 0,
                 "PID page zero was emitted before aggregation") ||
        !require(capturedFrame(requestBytes).payload ==
                     QByteArray::fromHex("10 01"),
                 "PID page one request was not sent after page zero")) {
        return 1;
    }
    const QByteArray pidPage1Request = requestBytes;
    feedResponse(&protocol, pidPage1Request, protocol::Command::GetParamGroup,
                 pidPagePayload(18, 7));
    if (!require(aggregatedPidCount == 1 && aggregatedPidValues.size() == 25,
                 "PID pages were not emitted as one complete group") ||
        !require(aggregatedPidValues.front().id == 0x1000 &&
                     aggregatedPidValues.back().id == 0x1044,
                 "PID page aggregation changed parameter order")) {
        return 1;
    }

    ProtocolClient incompletePidProtocol;
    DeviceClient incompletePidDevice(&incompletePidProtocol);
    QByteArray incompletePidRequest;
    int incompletePidCount = 0;
    int incompletePidErrors = 0;
    QObject::connect(&incompletePidProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { incompletePidRequest = bytes; });
    QObject::connect(&incompletePidDevice, &DeviceClient::parameterGroupReceived,
                     [&](quint8 group, const QVector<ParameterValue> &) {
                         if (group == 0x10) {
                             ++incompletePidCount;
                         }
                     });
    QObject::connect(&incompletePidDevice, &DeviceClient::deviceError,
                     [&](const QString &) { ++incompletePidErrors; });
    if (!require(incompletePidDevice.getParameterGroup(0x10),
                 "incomplete PID parameter group request was not sent")) {
        return 1;
    }
    feedResponse(&incompletePidProtocol, incompletePidRequest,
                 protocol::Command::GetParamGroup, pidPagePayload(0, 17));
    if (!require(incompletePidCount == 0,
                 "an incomplete PID page was emitted as a complete group") ||
        !require(incompletePidErrors == 1,
                 "an incomplete PID page did not report an error")) {
        return 1;
    }

    ProtocolClient interleavedProtocol;
    DeviceClient interleavedDevice(&interleavedProtocol);
    QByteArray interleavedRequest;
    int interleavedPidCount = 0;
    int interleavedReadFailures = 0;
    QObject::connect(&interleavedProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { interleavedRequest = bytes; });
    QObject::connect(&interleavedDevice, &DeviceClient::parameterGroupReceived,
                     [&](quint8 group, const QVector<ParameterValue> &) {
                         if (group == 0x10) {
                             ++interleavedPidCount;
                         }
                     });
    QObject::connect(&interleavedDevice, &DeviceClient::parameterGroupReadFailed,
                     [&](quint8 group, const QString &) {
                         if (group == 0x10) {
                             ++interleavedReadFailures;
                         }
                     });
    if (!require(interleavedDevice.getParameterGroup(0x10),
                 "interleaved PID parameter group request was not sent")) {
        return 1;
    }
    const QByteArray interleavedPidPage0 = interleavedRequest;
    if (!require(interleavedDevice.getParameterGroup(0x20),
                 "unrelated parameter group request was not sent")) {
        return 1;
    }
    const QByteArray interleavedOtherRequest = interleavedRequest;
    feedResponse(&interleavedProtocol, interleavedOtherRequest,
                 protocol::Command::GetParamGroup, QByteArray::fromHex("08"),
                 protocol::Response | protocol::Error);
    if (!require(interleavedReadFailures == 0,
                 "an unrelated parameter-group error cleared PID pagination")) {
        return 1;
    }
    feedResponse(&interleavedProtocol, interleavedPidPage0,
                 protocol::Command::GetParamGroup, pidPagePayload(0, 18));
    if (!require(capturedFrame(interleavedRequest).payload ==
                     QByteArray::fromHex("10 01"),
                 "PID page zero did not continue after an unrelated error")) {
        return 1;
    }
    const QByteArray interleavedPidPage1 = interleavedRequest;
    feedResponse(&interleavedProtocol, interleavedPidPage1,
                 protocol::Command::GetParamGroup, pidPagePayload(18, 7));
    if (!require(interleavedPidCount == 1 && interleavedReadFailures == 0,
                 "unrelated parameter-group error corrupted PID aggregation")) {
        return 1;
    }

    ProtocolClient failedOtherProtocol;
    DeviceClient failedOtherDevice(&failedOtherProtocol);
    QByteArray failedOtherRequest;
    int failedOtherReadFailures = 0;
    QObject::connect(&failedOtherProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { failedOtherRequest = bytes; });
    QObject::connect(&failedOtherDevice,
                     &DeviceClient::parameterGroupReadFailed,
                     [&](quint8 group, const QString &) {
                         if (group == 0x10) {
                             ++failedOtherReadFailures;
                         }
                     });
    if (!require(failedOtherDevice.getParameterGroup(0x10),
                 "PID request for unrelated-failure test was not sent")) {
        return 1;
    }
    const QByteArray failedOtherPidPage0 = failedOtherRequest;
    if (!require(failedOtherDevice.getParameterGroup(0x20),
                 "second unrelated parameter group request was not sent")) {
        return 1;
    }
    const quint8 failedOtherSequence =
        capturedFrame(failedOtherRequest).sequence;
    failedOtherProtocol.requestFailed(failedOtherSequence,
                                       QStringLiteral("请求超时"));
    if (!require(failedOtherReadFailures == 0,
                 "an unrelated request failure cleared PID pagination")) {
        return 1;
    }
    feedResponse(&failedOtherProtocol, failedOtherPidPage0,
                 protocol::Command::GetParamGroup, pidPagePayload(0, 18));
    if (!require(capturedFrame(failedOtherRequest).payload ==
                     QByteArray::fromHex("10 01"),
                 "PID page zero did not continue after an unrelated failure")) {
        return 1;
    }

    ProtocolClient stalePidProtocol;
    DeviceClient stalePidDevice(&stalePidProtocol);
    QByteArray stalePidRequest;
    int stalePidCount = 0;
    QVector<ParameterValue> stalePidValues;
    int staleReadFailures = 0;
    QObject::connect(&stalePidProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { stalePidRequest = bytes; });
    QObject::connect(&stalePidDevice, &DeviceClient::parameterGroupReceived,
                     [&](quint8 group, const QVector<ParameterValue> &values) {
                         if (group == 0x10) {
                             ++stalePidCount;
                             stalePidValues = values;
                         }
                     });
    QObject::connect(&stalePidDevice, &DeviceClient::parameterGroupReadFailed,
                     [&](quint8 group, const QString &) {
                         if (group == 0x10) {
                             ++staleReadFailures;
                         }
                     });
    if (!require(stalePidDevice.getParameterGroup(0x10),
                 "stale-response PID request was not sent")) {
        return 1;
    }
    const QByteArray stalePage0Request = stalePidRequest;
    const quint8 stalePage0Sequence = capturedFrame(stalePage0Request).sequence;
    feedResponse(&stalePidProtocol, stalePage0Request,
                 protocol::Command::GetParamGroup, pidPagePayload(0, 18));
    const QByteArray stalePage1Request = stalePidRequest;
    emitResponse(&stalePidProtocol, stalePage0Request,
                 protocol::Command::GetParamGroup, pidPagePayload(0, 18),
                 stalePage0Sequence);
    if (!require(stalePidCount == 0 && staleReadFailures == 0,
                 "a stale PID page response changed current pagination")) {
        return 1;
    }
    if (!require(capturedFrame(stalePidRequest).payload ==
                     QByteArray::fromHex("10 01"),
                 "a stale PID page response replaced the page-one request")) {
        return 1;
    }
    feedResponse(&stalePidProtocol, stalePage1Request,
                 protocol::Command::GetParamGroup, pidPagePayload(18, 7));
    if (!require(stalePidCount == 1 && stalePidValues.size() == 25 &&
                     staleReadFailures == 0,
                 "valid PID page one was lost after a stale response")) {
        return 1;
    }

    ProtocolClient malformedProtocol;
    DeviceClient malformedDevice(&malformedProtocol);
    QByteArray malformedRequest;
    int malformedPidCount = 0;
    int malformedReadFailures = 0;
    QObject::connect(&malformedProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { malformedRequest = bytes; });
    QObject::connect(&malformedDevice,
                     &DeviceClient::parameterGroupReceived,
                     [&](quint8 group, const QVector<ParameterValue> &) {
                         if (group == 0x10) {
                             ++malformedPidCount;
                         }
                     });
    QObject::connect(&malformedDevice, &DeviceClient::parameterGroupReadFailed,
                     [&](quint8 group, const QString &) {
                         if (group == 0x10) {
                             ++malformedReadFailures;
                         }
                     });
    if (!require(malformedDevice.getParameterGroup(0x10),
                 "malformed PID request was not sent")) {
        return 1;
    }
    const QByteArray malformedPage0Request = malformedRequest;
    feedResponse(&malformedProtocol, malformedPage0Request,
                 protocol::Command::GetParamGroup, pidPagePayload(0, 18));
    const QByteArray malformedPage1Request = malformedRequest;
    feedResponse(&malformedProtocol, malformedPage1Request,
                 protocol::Command::GetParamGroup, pidPagePayload(19, 7));
    if (!require(malformedPidCount == 0 && malformedReadFailures == 1,
                 "out-of-order PID page one was accepted") ||
        !require(malformedDevice.getParameterGroup(0x10),
                 "PID pagination could not be retried after page-order error") ||
        !require(capturedFrame(malformedRequest).payload ==
                     QByteArray::fromHex("10"),
                 "retry after page-order error did not request page zero")) {
        return 1;
    }
    const QByteArray retryPage0Request = malformedRequest;
    feedResponse(&malformedProtocol, retryPage0Request,
                 protocol::Command::GetParamGroup, pidPagePayload(0, 18));
    const QByteArray retryPage1Request = malformedRequest;
    feedResponse(&malformedProtocol, retryPage1Request,
                 protocol::Command::GetParamGroup, pidPagePayload(18, 6));
    if (!require(malformedPidCount == 0 && malformedReadFailures == 2,
                 "wrong-count PID page one was accepted") ||
        !require(malformedDevice.getParameterGroup(0x10),
                 "PID pagination could not be retried after page-count error")) {
        return 1;
    }

    ProtocolClient disconnectedProtocol;
    DeviceClient disconnectedDevice(&disconnectedProtocol);
    QByteArray disconnectedRequest;
    int disconnectedReadFailures = 0;
    QObject::connect(&disconnectedProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { disconnectedRequest = bytes; });
    QObject::connect(&disconnectedDevice,
                     &DeviceClient::parameterGroupReadFailed,
                     [&](quint8 group, const QString &) {
                         if (group == 0x10) {
                             ++disconnectedReadFailures;
                         }
                     });
    if (!require(disconnectedDevice.getParameterGroup(0x10),
                 "disconnect PID request was not sent")) {
        return 1;
    }
    disconnectedProtocol.clearPending();
    if (!require(disconnectedReadFailures == 1,
                 "disconnect did not notify the PID page read failure")) {
        return 1;
    }

    ProtocolClient failedPidProtocol;
    DeviceClient failedPidDevice(&failedPidProtocol);
    QByteArray failedPidRequest;
    int failedPidReadFailures = 0;
    QObject::connect(&failedPidProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { failedPidRequest = bytes; });
    QObject::connect(&failedPidDevice, &DeviceClient::parameterGroupReadFailed,
                     [&](quint8 group, const QString &) {
                         if (group == 0x10) {
                             ++failedPidReadFailures;
                         }
                     });
    if (!require(failedPidDevice.getParameterGroup(0x10),
                 "PID request-failure test request was not sent")) {
        return 1;
    }
    failedPidProtocol.requestFailed(capturedFrame(failedPidRequest).sequence,
                                    QStringLiteral("请求超时"));
    if (!require(failedPidReadFailures == 1,
                 "PID request failure did not notify the page")) {
        return 1;
    }
    if (!require(failedPidDevice.getParameterGroup(0x10),
                 "PID pagination could not be retried after request failure") ||
        !require(capturedFrame(failedPidRequest).payload ==
                     QByteArray::fromHex("10"),
                 "PID request-failure retry did not request page zero")) {
        return 1;
    }

    ProtocolClient errorPidProtocol;
    DeviceClient errorPidDevice(&errorPidProtocol);
    QByteArray errorPidRequest;
    int errorPidReadFailures = 0;
    QObject::connect(&errorPidProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { errorPidRequest = bytes; });
    QObject::connect(&errorPidDevice, &DeviceClient::parameterGroupReadFailed,
                     [&](quint8 group, const QString &) {
                         if (group == 0x10) {
                             ++errorPidReadFailures;
                         }
                     });
    if (!require(errorPidDevice.getParameterGroup(0x10),
                 "PID error-response test request was not sent")) {
        return 1;
    }
    feedResponse(&errorPidProtocol, errorPidRequest,
                 protocol::Command::GetParamGroup, QByteArray::fromHex("08"),
                 protocol::Response | protocol::Error);
    if (!require(errorPidReadFailures == 1,
                 "PID error response did not notify the page") ||
        !require(errorPidDevice.getParameterGroup(0x10),
                 "PID pagination could not be retried after error response")) {
        return 1;
    }

    int statusCount = 0;
    DeviceStatus latestStatus;
    QObject::connect(&device, &DeviceClient::statusReceived,
                     [&](const DeviceStatus &status) {
                         ++statusCount;
                         latestStatus = status;
                     });
    requestBytes.clear();
    if (!require(device.getStatus(), "GET_STATUS request was not sent")) {
        return 1;
    }
    const protocol::Frame statusRequest = capturedFrame(requestBytes);
    if (!require(statusRequest.command ==
                     static_cast<quint8>(protocol::Command::GetStatus),
                 "GET_STATUS command is incorrect")) {
        return 1;
    }
    feedResponse(&protocol, requestBytes, protocol::Command::GetStatus,
                 QByteArray::fromHex("02 01 00 01 0a 00"));
    if (!require(statusCount == 1 && latestStatus.mode == 2 &&
                     latestStatus.emergency == 1 &&
                     latestStatus.unlocked == 0 &&
                     latestStatus.activeLink == 1 &&
                     latestStatus.lastError == 0x000a,
                 "GET_STATUS response was not decoded")) {
        return 1;
    }

    int telemetryConfigurationCount = 0;
    quint8 acceptedMask = 0;
    quint16 actualPeriodMs = 0;
    QObject::connect(&device, &DeviceClient::telemetryConfigured,
                     [&](quint8 mask, quint16 periodMs) {
                         ++telemetryConfigurationCount;
                         acceptedMask = mask;
                         actualPeriodMs = periodMs;
                     });
    requestBytes.clear();
    if (!require(device.setTelemetry(0x07, 100),
                 "SET_TELEMETRY request was not sent")) {
        return 1;
    }
    feedResponse(&protocol, requestBytes, protocol::Command::SetTelemetry,
                 QByteArray::fromHex("07 fa 00"));
    if (!require(telemetryConfigurationCount == 1 && acceptedMask == 0x07 &&
                     actualPeriodMs == 250,
                 "SET_TELEMETRY response did not report the actual period")) {
        return 1;
    }

    int calibrationState = -1;
    QObject::connect(&device, &DeviceClient::imuCalibrationStateChanged,
                     [&](quint8 state) { calibrationState = state; });
    requestBytes.clear();
    if (!require(device.calibrateImu(),
                 "IMU_CALIBRATE request was not sent")) {
        return 1;
    }
    feedResponse(&protocol, requestBytes, protocol::Command::ImuCalibrate,
                 QByteArray(1, char(1)));
    if (!require(calibrationState == 1,
                 "IMU calibration state was not decoded")) {
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
    device.getParameterGroup(0x20);
    if (!require(capturedFrame(requestBytes).payload ==
                     QByteArray::fromHex("20"),
                 "single-page parameter group request gained a page field")) {
        return 1;
    }
    feedResponse(&protocol, requestBytes, protocol::Command::GetParamGroup,
                 parameterResponsePayload(0x20, 0x2000, ValueType::Int32,
                                          QByteArray::fromHex("0c000000")));
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
        {0x1000, ValueType::Float32, floatBytes(1.5f),
         "readback parameter from another group was accepted"},
        {0x2000, ValueType::UInt16, QByteArray::fromHex("0100"),
         "readback type mismatch was accepted"},
        {0x2000, ValueType::Int32, QByteArray::fromHex("51000000"),
         "out-of-range readback parameter was accepted"},
    };
    for (const auto &invalid : invalidReadbacks) {
        requestBytes.clear();
        device.getParameterGroup(0x20);
        const int groupsBefore = parameterGroupCount;
        const int errorsBefore = deviceErrorCount;
        feedResponse(&protocol, requestBytes, protocol::Command::GetParamGroup,
                     parameterResponsePayload(0x20, invalid.id, invalid.type,
                                              invalid.value));
        if (!require(parameterGroupCount == groupsBefore, invalid.message) ||
            !require(deviceErrorCount == errorsBefore + 1,
                     "invalid readback did not report an error")) {
            return 1;
        }
    }

    requestBytes.clear();
    device.getParameterGroup(0x20);
    const int truncatedErrorsBefore = deviceErrorCount;
    feedResponse(&protocol, requestBytes, protocol::Command::GetParamGroup,
                 parameterResponsePayload(0x20, 0x2000, ValueType::Int32,
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

    protocol::Frame statusEvent = imuEvent;
    statusEvent.command =
        static_cast<quint8>(protocol::Command::StatusTelemetry);
    statusEvent.payload = QByteArray::fromHex("02 01 01 34 12");
    protocol.ingestBytes(QByteArrayView(encodeFrame(statusEvent)));
    if (!require(statusCount == 2 && latestStatus.mode == 2 &&
                     latestStatus.emergency == 1 &&
                     latestStatus.unlocked == 1 &&
                     latestStatus.lastError == 0x1234,
                 "STATUS_TELEMETRY event was not decoded")) {
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

    ProtocolClient resetProtocol;
    DeviceClient resetDevice(&resetProtocol);
    QByteArray resetRequest;
    QObject::connect(&resetProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { resetRequest = bytes; });
    resetDevice.hello();
    feedResponse(&resetProtocol, resetRequest, protocol::Command::Hello,
                 helloPayload);
    if (!require(resetDevice.handshakeComplete(),
                 "reset-device HELLO did not complete")) {
        return 1;
    }
    resetProtocol.clearPending();
    if (!require(!resetDevice.handshakeComplete(),
                 "clearPending without requests did not clear handshake state")) {
        return 1;
    }

    ProtocolClient staleProtocol;
    DeviceClient staleDevice(&staleProtocol);
    QByteArray firstHelloRequest;
    QByteArray secondHelloRequest;
    int helloRequestCount = 0;
    QObject::connect(&staleProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         ++helloRequestCount;
                         if (helloRequestCount == 1) {
                             firstHelloRequest = bytes;
                         } else if (helloRequestCount == 2) {
                             secondHelloRequest = bytes;
                         }
                     });
    staleDevice.hello();
    staleDevice.hello();
    feedResponse(&staleProtocol, firstHelloRequest, protocol::Command::Hello,
                 helloPayload);
    if (!require(!staleDevice.handshakeComplete(),
                 "an old HELLO response unlocked a newer handshake")) {
        return 1;
    }
    feedResponse(&staleProtocol, secondHelloRequest, protocol::Command::Hello,
                 helloPayload);
    if (!require(staleDevice.handshakeComplete(),
                 "the latest HELLO response was not accepted")) {
        return 1;
    }

    // Action commands are safety-gated locally and use the documented
    // little-endian payload variants.  Emergency stop remains available
    // before a HELLO response so it can be used as a highest-priority stop.
    ProtocolClient actionProtocol(5);
    DeviceClient actionDevice(&actionProtocol);
    QByteArray actionRequestBytes;
    int actionErrors = 0;
    QObject::connect(&actionProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         actionRequestBytes = bytes;
                     });
    QObject::connect(&actionDevice, &DeviceClient::deviceError,
                     [&](const QString &) { ++actionErrors; });
    if (!require(!actionDevice.unlockTests(),
                 "TEST_UNLOCK was allowed before HELLO") ||
        !require(!actionDevice.testChassis(0, 0, 0, 100),
                 "TEST_ACTION was allowed before HELLO") ||
        !require(!actionDevice.testHorizontal(0.0, 100, 1),
                 "horizontal action was allowed before HELLO") ||
        !require(actionErrors == 3,
                 "pre-handshake action requests did not fail locally")) {
        return 1;
    }

    actionRequestBytes.clear();
    if (!require(actionDevice.emergencyStop(),
                 "emergency stop must be available before HELLO")) {
        return 1;
    }
    const protocol::Frame earlyEmergency = capturedFrame(actionRequestBytes);
    if (!require(earlyEmergency.command == static_cast<quint8>(
                     protocol::Command::EmergencyStop) &&
                     earlyEmergency.payload.isEmpty(),
                 "EMERGENCY_STOP payload is incorrect")) {
        return 1;
    }
    actionProtocol.clearPending();

    actionRequestBytes.clear();
    actionDevice.hello();
    const protocol::Frame actionHelloRequest =
        capturedFrame(actionRequestBytes);
    protocol::Frame actionHelloResponse = actionHelloRequest;
    actionHelloResponse.flags = protocol::Response;
    actionHelloResponse.payload = helloPayload;
    actionProtocol.ingestBytes(
        QByteArrayView(encodeFrame(actionHelloResponse)));
    if (!require(actionDevice.handshakeComplete(),
                 "action-device HELLO did not complete") ||
        !require(actionDevice.emergencyLocked(),
                 "emergency stop state was lost across HELLO")) {
        return 1;
    }
    if (!require(!actionDevice.unlockTests(),
                 "TEST_UNLOCK was allowed while emergency was locked") ||
        !require(!actionDevice.testChassis(0, 0, 0, 100),
                 "chassis action was allowed while emergency was locked")) {
        return 1;
    }

    actionRequestBytes.clear();
    if (!require(actionDevice.clearEmergencyStop(),
                 "CLEAR_EMERGENCY_STOP request was not sent")) {
        return 1;
    }
    const protocol::Frame clearRequest = capturedFrame(actionRequestBytes);
    if (!require(clearRequest.command == static_cast<quint8>(
                     protocol::Command::ClearEmergencyStop) &&
                     clearRequest.payload.isEmpty(),
                 "CLEAR_EMERGENCY_STOP payload is incorrect")) {
        return 1;
    }
    protocol::Frame clearResponse = clearRequest;
    clearResponse.flags = protocol::Response;
    clearResponse.payload.clear();
    actionProtocol.ingestBytes(QByteArrayView(encodeFrame(clearResponse)));
    if (!require(!actionDevice.emergencyLocked(),
                 "successful CLEAR_EMERGENCY_STOP did not clear the lock")) {
        return 1;
    }

    if (!require(!actionDevice.testChassis(0, 0, 0, 100),
                 "TEST_ACTION was allowed before TEST_UNLOCK")) {
        return 1;
    }

    actionRequestBytes.clear();
    if (!require(actionDevice.unlockTests(),
                 "TEST_UNLOCK request was not sent after HELLO")) {
        return 1;
    }
    const protocol::Frame unlockRequest = capturedFrame(actionRequestBytes);
    if (!require(unlockRequest.command == static_cast<quint8>(
                     protocol::Command::TestUnlock) &&
                     unlockRequest.payload.isEmpty(),
                 "TEST_UNLOCK payload is incorrect")) {
        return 1;
    }
    protocol::Frame unlockResponse = unlockRequest;
    unlockResponse.flags = protocol::Response;
    unlockResponse.payload = QByteArray::fromHex("30 75");
    actionProtocol.ingestBytes(QByteArrayView(encodeFrame(unlockResponse)));
    if (!require(actionDevice.testsUnlocked() &&
                     actionDevice.testActionsEnabled() &&
                     actionDevice.unlockRemainingMs() > 0,
                 "successful TEST_UNLOCK did not enable actions")) {
        return 1;
    }

    actionRequestBytes.clear();
    if (!require(actionDevice.testChassis(80, -80, 30, 1000),
                 "valid chassis action was rejected")) {
        return 1;
    }
    const protocol::Frame chassisRequest = capturedFrame(actionRequestBytes);
    if (!require(chassisRequest.command == static_cast<quint8>(
                     protocol::Command::TestAction) &&
                     chassisRequest.payload ==
                         QByteArray::fromHex("01 50 00 b0 ff 1e 00 e8 03"),
                 "chassis action payload is incorrect")) {
        return 1;
    }
    protocol::Frame chassisResponse = chassisRequest;
    chassisResponse.flags = protocol::Response;
    chassisResponse.payload.clear();
    actionProtocol.ingestBytes(QByteArrayView(encodeFrame(chassisResponse)));

    const int rangeErrorsBefore = actionErrors;
    actionRequestBytes.clear();
    if (!require(!actionDevice.testChassis(81, 0, 0, 100),
                 "out-of-range chassis vx was accepted") ||
        !require(!actionDevice.testChassis(0, -81, 0, 100),
                 "out-of-range chassis vy was accepted") ||
        !require(!actionDevice.testChassis(0, 0, 31, 100),
                 "out-of-range chassis w was accepted") ||
        !require(!actionDevice.testChassis(0, 0, 0, 49),
                 "short chassis duration was accepted") ||
        !require(!actionDevice.testChassis(0, 0, 0, 1001),
                 "long chassis duration was accepted") ||
        !require(actionRequestBytes.isEmpty() && actionErrors == rangeErrorsBefore + 5,
                 "invalid chassis actions were not rejected without sending")) {
        return 1;
    }

    actionRequestBytes.clear();
    if (!require(actionDevice.testHorizontal(-120.0, 2000, 220),
                 "valid horizontal action was rejected")) {
        return 1;
    }
    const protocol::Frame horizontalRequest = capturedFrame(actionRequestBytes);
    if (!require(horizontalRequest.payload.size() == 8 &&
                     static_cast<quint8>(horizontalRequest.payload.at(0)) ==
                         0x10 &&
                     horizontalRequest.payload.mid(5) ==
                         QByteArray::fromHex("d0 07 dc"),
                 "horizontal action payload is incorrect")) {
        return 1;
    }
    protocol::Frame horizontalResponse = horizontalRequest;
    horizontalResponse.flags = protocol::Response;
    horizontalResponse.payload.clear();
    actionProtocol.ingestBytes(QByteArrayView(encodeFrame(horizontalResponse)));

    actionRequestBytes.clear();
    if (!require(actionDevice.testLift(50.0, 100, 1),
                 "valid lift action was rejected") ||
        !require(actionDevice.testTurret(295.0, 20.0),
                 "valid turret action was rejected") ||
        !require(actionDevice.setPlatformPosition(3),
                 "valid platform action was rejected") ||
        !require(actionDevice.setGripperOpen(true),
                 "valid gripper action was rejected")) {
        return 1;
    }
    if (!require(!actionDevice.testHorizontal(-121.0, 100, 1),
                 "out-of-range horizontal target was accepted") ||
        !require(!actionDevice.testLift(51.0, 100, 1),
                 "out-of-range lift target was accepted") ||
        !require(!actionDevice.testTurret(134.0, 1.0),
                 "out-of-range turret target was accepted") ||
        !require(!actionDevice.setPlatformPosition(4),
                 "invalid platform position was accepted") ||
        !require(actionDevice.testActionsEnabled(),
                 "range rejection unexpectedly disabled test actions")) {
        return 1;
    }
    actionProtocol.clearPending();

    actionRequestBytes.clear();
    if (!require(actionDevice.emergencyStop(),
                 "emergency stop request was rejected after unlock") ||
        !require(actionDevice.emergencyLocked() &&
                     !actionDevice.testActionsEnabled(),
                 "emergency stop did not lock actions immediately") ||
        !require(!actionDevice.testChassis(0, 0, 0, 100),
                 "action was allowed after emergency stop")) {
        return 1;
    }
    actionProtocol.clearPending();

    // A short unlock response lets the test verify expiration without a
    // 30-second test delay.  Request timeout must also report an error and
    // invalidate the safety state.
    actionDevice.hello();
    const protocol::Frame actionSecondHelloFrame =
        capturedFrame(actionRequestBytes);
    protocol::Frame secondHelloResponse = actionSecondHelloFrame;
    secondHelloResponse.flags = protocol::Response;
    secondHelloResponse.payload = helloPayload;
    actionProtocol.ingestBytes(
        QByteArrayView(encodeFrame(secondHelloResponse)));
    actionDevice.clearEmergencyStop();
    const protocol::Frame secondClearRequest =
        capturedFrame(actionRequestBytes);
    protocol::Frame secondClearResponse = secondClearRequest;
    secondClearResponse.flags = protocol::Response;
    secondClearResponse.payload.clear();
    actionProtocol.ingestBytes(
        QByteArrayView(encodeFrame(secondClearResponse)));
    actionDevice.unlockTests();
    const protocol::Frame shortUnlockRequest =
        capturedFrame(actionRequestBytes);
    protocol::Frame shortUnlockResponse = shortUnlockRequest;
    shortUnlockResponse.flags = protocol::Response;
    shortUnlockResponse.payload = QByteArray::fromHex("01 00");
    actionProtocol.ingestBytes(
        QByteArrayView(encodeFrame(shortUnlockResponse)));
    QThread::msleep(3);
    QCoreApplication::processEvents();
    if (!require(!actionDevice.testsUnlocked() &&
                     !actionDevice.testActionsEnabled(),
                 "expired TEST_UNLOCK still enabled actions")) {
        return 1;
    }

    actionDevice.unlockTests();
    const protocol::Frame timeoutUnlockRequest =
        capturedFrame(actionRequestBytes);
    protocol::Frame timeoutUnlockResponse = timeoutUnlockRequest;
    timeoutUnlockResponse.flags = protocol::Response;
    timeoutUnlockResponse.payload = QByteArray::fromHex("30 75");
    actionProtocol.ingestBytes(
        QByteArrayView(encodeFrame(timeoutUnlockResponse)));
    const int timeoutErrorsBefore = actionErrors;
    actionDevice.testChassis(0, 0, 0, 100);
    QElapsedTimer timeoutWait;
    timeoutWait.start();
    while (timeoutWait.elapsed() < 40) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    if (!require(actionErrors > timeoutErrorsBefore &&
                     !actionDevice.handshakeComplete() &&
                     !actionDevice.testActionsEnabled(),
                 "action timeout did not report an error and lock actions")) {
        return 1;
    }

    // A delayed unlock response must not survive a newer HELLO transaction.
    ProtocolClient raceProtocol(100);
    DeviceClient raceDevice(&raceProtocol);
    QVector<QByteArray> raceRequests;
    QObject::connect(&raceProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { raceRequests.push_back(bytes); });
    raceDevice.hello();
    const QByteArray raceHelloRequest = raceRequests.back();
    feedResponse(&raceProtocol, raceHelloRequest, protocol::Command::Hello,
                 helloPayload);
    raceRequests.clear();
    raceDevice.unlockTests();
    const QByteArray staleUnlockRequest = raceRequests.back();
    raceDevice.hello();
    const QByteArray latestHelloRequest = raceRequests.back();
    feedResponse(&raceProtocol, latestHelloRequest, protocol::Command::Hello,
                 helloPayload);
    protocol::Frame staleUnlockResponse = capturedFrame(staleUnlockRequest);
    staleUnlockResponse.flags = protocol::Response;
    staleUnlockResponse.payload = QByteArray::fromHex("30 75");
    raceProtocol.ingestBytes(QByteArrayView(encodeFrame(staleUnlockResponse)));
    if (!require(!raceDevice.testsUnlocked() &&
                     !raceDevice.testActionsEnabled(),
                 "a delayed TEST_UNLOCK ACK re-enabled actions after HELLO")) {
        return 1;
    }

    // The unlock contract is fixed at 30 seconds; other durations are a
    // protocol error and must fail closed.
    raceRequests.clear();
    raceDevice.unlockTests();
    const QByteArray invalidDurationRequest = raceRequests.back();
    protocol::Frame invalidDurationResponse =
        capturedFrame(invalidDurationRequest);
    invalidDurationResponse.flags = protocol::Response;
    invalidDurationResponse.payload = QByteArray::fromHex("31 75");
    raceProtocol.ingestBytes(
        QByteArrayView(encodeFrame(invalidDurationResponse)));
    if (!require(!raceDevice.testsUnlocked() &&
                     !raceDevice.testActionsEnabled(),
                 "TEST_UNLOCK accepted a duration other than 30000 ms")) {
        return 1;
    }

    // Emergency stop invalidates a pending unlock and prevents its delayed
    // ACK from changing the local safety state.
    raceRequests.clear();
    raceDevice.unlockTests();
    const QByteArray emergencyStaleUnlock = raceRequests.front();
    if (!require(raceDevice.emergencyStop(),
                 "emergency stop failed while an unlock was pending")) {
        return 1;
    }
    protocol::Frame emergencyStaleResponse = capturedFrame(emergencyStaleUnlock);
    emergencyStaleResponse.flags = protocol::Response;
    emergencyStaleResponse.payload = QByteArray::fromHex("30 75");
    raceProtocol.ingestBytes(
        QByteArrayView(encodeFrame(emergencyStaleResponse)));
    if (!require(!raceDevice.testsUnlocked() &&
                     !raceDevice.testActionsEnabled() &&
                     raceDevice.emergencyLocked(),
                 "a delayed TEST_UNLOCK ACK re-enabled actions after emergency stop")) {
        return 1;
    }
    raceProtocol.clearPending();

    // Any device error, including BUSY, clears an otherwise valid unlock.
    raceDevice.hello();
    const QByteArray errorHelloRequest = raceRequests.back();
    feedResponse(&raceProtocol, errorHelloRequest, protocol::Command::Hello,
                 helloPayload);
    raceDevice.clearEmergencyStop();
    const QByteArray errorClearRequest = raceRequests.back();
    feedResponse(&raceProtocol, errorClearRequest,
                 protocol::Command::ClearEmergencyStop, {});
    raceRequests.clear();
    raceDevice.unlockTests();
    const QByteArray errorUnlockRequest = raceRequests.back();
    feedResponse(&raceProtocol, errorUnlockRequest,
                 protocol::Command::TestUnlock, QByteArray::fromHex("30 75"));
    if (!require(raceDevice.testsUnlocked(),
                 "valid TEST_UNLOCK did not enable error fail-closed scenario")) {
        return 1;
    }
    raceRequests.clear();
    raceDevice.testChassis(0, 0, 0, 100);
    const QByteArray busyActionRequest = raceRequests.back();
    protocol::Frame busyResponse = capturedFrame(busyActionRequest);
    busyResponse.flags = protocol::Error;
    busyResponse.payload = QByteArray(1, char(0x08));
    raceProtocol.ingestBytes(QByteArrayView(encodeFrame(busyResponse)));
    if (!require(!raceDevice.testsUnlocked() &&
                     !raceDevice.testActionsEnabled(),
                 "BUSY error left test actions unlocked")) {
        return 1;
    }

    raceRequests.clear();
    raceDevice.unlockTests();
    const QByteArray rangeUnlockRequest = raceRequests.back();
    feedResponse(&raceProtocol, rangeUnlockRequest,
                 protocol::Command::TestUnlock, QByteArray::fromHex("30 75"));
    raceRequests.clear();
    raceDevice.testChassis(0, 0, 0, 100);
    const QByteArray rangeActionRequest = raceRequests.back();
    protocol::Frame rangeResponse = capturedFrame(rangeActionRequest);
    rangeResponse.flags = protocol::Error;
    rangeResponse.payload = QByteArray(1, char(0x07));
    raceProtocol.ingestBytes(QByteArrayView(encodeFrame(rangeResponse)));
    if (!require(!raceDevice.testsUnlocked() &&
                     !raceDevice.testActionsEnabled(),
                 "range error left test actions unlocked")) {
        return 1;
    }

    // A malformed success ACK is also an error boundary and must fail closed.
    raceRequests.clear();
    raceDevice.unlockTests();
    const QByteArray malformedUnlockRequest = raceRequests.back();
    feedResponse(&raceProtocol, malformedUnlockRequest,
                 protocol::Command::TestUnlock, QByteArray::fromHex("30 75"));
    raceRequests.clear();
    raceDevice.testChassis(0, 0, 0, 100);
    const QByteArray malformedActionRequest = raceRequests.back();
    feedResponse(&raceProtocol, malformedActionRequest,
                 protocol::Command::TestAction, QByteArray(1, char(0x01)));
    if (!require(!raceDevice.testsUnlocked() &&
                     !raceDevice.testActionsEnabled(),
                 "malformed action ACK left test actions unlocked")) {
        return 1;
    }

    // Emergency stop must cancel pending action retries before transmitting
    // STOP/EMERGENCY_STOP itself.
    ProtocolClient emergencyProtocol(5);
    DeviceClient emergencyDevice(&emergencyProtocol);
    QVector<QByteArray> emergencyRequests;
    int testActionFrameCount = 0;
    QObject::connect(&emergencyProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         emergencyRequests.push_back(bytes);
                         if (capturedFrame(bytes).command == static_cast<quint8>(
                                 protocol::Command::TestAction)) {
                             ++testActionFrameCount;
                         }
                     });
    emergencyDevice.hello();
    const QByteArray emergencyHello = emergencyRequests.front();
    feedResponse(&emergencyProtocol, emergencyHello, protocol::Command::Hello,
                 helloPayload);
    emergencyRequests.clear();
    emergencyDevice.unlockTests();
    const QByteArray emergencyUnlock = emergencyRequests.back();
    feedResponse(&emergencyProtocol, emergencyUnlock,
                 protocol::Command::TestUnlock, QByteArray::fromHex("30 75"));
    emergencyRequests.clear();
    emergencyDevice.testChassis(0, 0, 0, 100);
    if (!require(testActionFrameCount == 1,
                 "test action request was not sent before emergency stop")) {
        return 1;
    }
    emergencyDevice.emergencyStop();
    QElapsedTimer emergencyWait;
    emergencyWait.start();
    while (emergencyWait.elapsed() < 35) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    if (!require(testActionFrameCount == 1,
                 "pending test action was retried after emergency stop")) {
        return 1;
    }

    // STOP is safe to issue after the handshake and must cancel any action
    // retry that was already queued.
    ProtocolClient stopProtocol(5);
    DeviceClient stopDevice(&stopProtocol);
    QVector<QByteArray> stopRequests;
    int stopActionFrameCount = 0;
    QObject::connect(&stopProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         stopRequests.push_back(bytes);
                         if (capturedFrame(bytes).command == static_cast<quint8>(
                                 protocol::Command::TestAction)) {
                             ++stopActionFrameCount;
                         }
                     });
    stopDevice.hello();
    const QByteArray stopHello = stopRequests.front();
    feedResponse(&stopProtocol, stopHello, protocol::Command::Hello,
                 helloPayload);
    stopRequests.clear();
    stopDevice.unlockTests();
    const QByteArray stopUnlock = stopRequests.back();
    feedResponse(&stopProtocol, stopUnlock, protocol::Command::TestUnlock,
                 QByteArray::fromHex("30 75"));
    stopRequests.clear();
    stopDevice.testChassis(0, 0, 0, 100);
    if (!require(stopActionFrameCount == 1,
                 "test action request was not sent before STOP")) {
        return 1;
    }
    if (!require(stopDevice.stop(), "STOP request was rejected after handshake")) {
        return 1;
    }
    QElapsedTimer stopWait;
    stopWait.start();
    while (stopWait.elapsed() < 35) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    if (!require(stopActionFrameCount == 1,
                 "pending test action was retried after STOP")) {
        return 1;
    }

    // failClosed must cancel every safety-sensitive request, not just the
    // unlock request.  A malformed GET_STATUS response triggers that path.
    ProtocolClient failClosedProtocol(5);
    DeviceClient failClosedDevice(&failClosedProtocol);
    QVector<QByteArray> failClosedRequests;
    int failClosedActionFrames = 0;
    int failClosedUnlockFrames = 0;
    int failClosedClearFrames = 0;
    QObject::connect(&failClosedProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         failClosedRequests.push_back(bytes);
                         switch (static_cast<protocol::Command>(
                             capturedFrame(bytes).command)) {
                         case protocol::Command::TestAction:
                             ++failClosedActionFrames;
                             break;
                         case protocol::Command::TestUnlock:
                             ++failClosedUnlockFrames;
                             break;
                         case protocol::Command::ClearEmergencyStop:
                             ++failClosedClearFrames;
                             break;
                         default:
                             break;
                         }
                     });
    failClosedDevice.hello();
    const QByteArray failClosedHello = failClosedRequests.front();
    feedResponse(&failClosedProtocol, failClosedHello,
                 protocol::Command::Hello, helloPayload);
    failClosedProtocol.sendRequest(protocol::Command::TestAction,
                                   QByteArray("move"));
    failClosedProtocol.sendRequest(protocol::Command::TestUnlock, {});
    failClosedProtocol.sendRequest(protocol::Command::ClearEmergencyStop, {});
    if (!require(failClosedDevice.getStatus(),
                 "GET_STATUS request was not sent for failClosed test")) {
        return 1;
    }
    const QByteArray malformedStatusRequest = failClosedRequests.back();
    feedResponse(&failClosedProtocol, malformedStatusRequest,
                 protocol::Command::GetStatus, QByteArray(1, char(0x00)));
    QElapsedTimer failClosedWait;
    failClosedWait.start();
    while (failClosedWait.elapsed() < 35) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    if (!require(failClosedActionFrames == 1 && failClosedUnlockFrames == 1 &&
                     failClosedClearFrames == 1,
                 "failClosed left a safety request eligible for retry")) {
        return 1;
    }

    // Both status paths must fail closed on malformed payloads, while a valid
    // emergency status must lock the actions immediately.
    ProtocolClient statusProtocol(100);
    DeviceClient statusDevice(&statusProtocol);
    QVector<QByteArray> statusRequests;
    QObject::connect(&statusProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) { statusRequests.push_back(bytes); });
    statusDevice.hello();
    const QByteArray statusHello = statusRequests.front();
    feedResponse(&statusProtocol, statusHello, protocol::Command::Hello,
                 helloPayload);
    statusRequests.clear();
    statusDevice.unlockTests();
    const QByteArray statusUnlock = statusRequests.back();
    feedResponse(&statusProtocol, statusUnlock, protocol::Command::TestUnlock,
                 QByteArray::fromHex("30 75"));
    protocol::Frame malformedStatusEvent;
    malformedStatusEvent.flags = protocol::Event;
    malformedStatusEvent.sequence = 0;
    malformedStatusEvent.command = static_cast<quint8>(
        protocol::Command::StatusTelemetry);
    malformedStatusEvent.payload = QByteArray(1, char(0x00));
    statusProtocol.ingestBytes(
        QByteArrayView(encodeFrame(malformedStatusEvent)));
    if (!require(!statusDevice.testsUnlocked() &&
                     !statusDevice.testActionsEnabled(),
                 "malformed status event left actions unlocked")) {
        return 1;
    }
    statusRequests.clear();
    statusDevice.unlockTests();
    const QByteArray statusUnlockAgain = statusRequests.back();
    feedResponse(&statusProtocol, statusUnlockAgain,
                 protocol::Command::TestUnlock, QByteArray::fromHex("30 75"));
    statusRequests.clear();
    if (!require(statusDevice.getStatus(),
                 "GET_STATUS request was not sent for malformed response test")) {
        return 1;
    }
    const QByteArray malformedStatusResponse = statusRequests.back();
    feedResponse(&statusProtocol, malformedStatusResponse,
                 protocol::Command::GetStatus, QByteArray(1, char(0x00)));
    if (!require(!statusDevice.testsUnlocked() &&
                     !statusDevice.testActionsEnabled(),
                 "malformed status response left actions unlocked")) {
        return 1;
    }
    statusRequests.clear();
    statusDevice.unlockTests();
    const QByteArray emergencyStatusUnlock = statusRequests.back();
    feedResponse(&statusProtocol, emergencyStatusUnlock,
                 protocol::Command::TestUnlock, QByteArray::fromHex("30 75"));
    protocol::Frame emergencyStatusEvent;
    emergencyStatusEvent.flags = protocol::Event;
    emergencyStatusEvent.command = static_cast<quint8>(
        protocol::Command::StatusTelemetry);
    emergencyStatusEvent.payload = QByteArray::fromHex("02 01 01 00 00");
    statusProtocol.ingestBytes(
        QByteArrayView(encodeFrame(emergencyStatusEvent)));
    if (!require(statusDevice.emergencyLocked() &&
                     !statusDevice.testsUnlocked() &&
                     !statusDevice.testActionsEnabled(),
                 "emergency status event did not lock actions")) {
        return 1;
    }

    // A clear ACK from an obsolete safety generation must not release an
    // emergency lock after a newer emergency stop.
    ProtocolClient staleClearProtocol(100);
    DeviceClient staleClearDevice(&staleClearProtocol);
    QVector<QByteArray> staleClearRequests;
    QObject::connect(&staleClearProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         staleClearRequests.push_back(bytes);
                     });
    staleClearDevice.hello();
    const QByteArray staleClearHello = staleClearRequests.front();
    feedResponse(&staleClearProtocol, staleClearHello,
                 protocol::Command::Hello, helloPayload);
    staleClearDevice.emergencyStop();
    const QByteArray firstEmergencyStop = staleClearRequests.back();
    feedResponse(&staleClearProtocol, firstEmergencyStop,
                 protocol::Command::EmergencyStop, {});
    staleClearRequests.clear();
    if (!require(staleClearDevice.clearEmergencyStop(),
                 "CLEAR_EMERGENCY_STOP request was not sent")) {
        return 1;
    }
    const QByteArray staleClearRequest = staleClearRequests.back();
    staleClearDevice.emergencyStop();
    protocol::Frame staleClearResponse = capturedFrame(staleClearRequest);
    staleClearResponse.flags = protocol::Response;
    staleClearResponse.payload.clear();
    staleClearProtocol.ingestBytes(
        QByteArrayView(encodeFrame(staleClearResponse)));
    if (!require(staleClearDevice.emergencyLocked(),
                 "late CLEAR_EMERGENCY_STOP ACK cleared a newer emergency lock")) {
        return 1;
    }
    staleClearProtocol.clearPending();

    // HELLO must also invalidate a clear request that was sent on the old
    // connection generation.
    ProtocolClient helloClearProtocol(100);
    DeviceClient helloClearDevice(&helloClearProtocol);
    QVector<QByteArray> helloClearRequests;
    QObject::connect(&helloClearProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         helloClearRequests.push_back(bytes);
                     });
    helloClearDevice.hello();
    const QByteArray helloClearHello = helloClearRequests.front();
    feedResponse(&helloClearProtocol, helloClearHello,
                 protocol::Command::Hello, helloPayload);
    helloClearDevice.emergencyStop();
    const QByteArray helloEmergencyStop = helloClearRequests.back();
    feedResponse(&helloClearProtocol, helloEmergencyStop,
                 protocol::Command::EmergencyStop, {});
    helloClearRequests.clear();
    helloClearDevice.clearEmergencyStop();
    const QByteArray oldClearRequest = helloClearRequests.back();
    helloClearDevice.hello();
    const QByteArray newHelloRequest = helloClearRequests.back();
    feedResponse(&helloClearProtocol, newHelloRequest,
                 protocol::Command::Hello, helloPayload);
    protocol::Frame oldClearResponse = capturedFrame(oldClearRequest);
    oldClearResponse.flags = protocol::Response;
    oldClearResponse.payload.clear();
    helloClearProtocol.ingestBytes(
        QByteArrayView(encodeFrame(oldClearResponse)));
    if (!require(helloClearDevice.emergencyLocked(),
                 "late CLEAR_EMERGENCY_STOP ACK cleared the HELLO lock")) {
        return 1;
    }
    helloClearProtocol.clearPending();

    // A failClosed transition must invalidate a clear ACK as well.
    ProtocolClient failClearProtocol(100);
    DeviceClient failClearDevice(&failClearProtocol);
    QVector<QByteArray> failClearRequests;
    QObject::connect(&failClearProtocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         failClearRequests.push_back(bytes);
                     });
    failClearDevice.hello();
    const QByteArray failClearHello = failClearRequests.front();
    feedResponse(&failClearProtocol, failClearHello,
                 protocol::Command::Hello, helloPayload);
    failClearDevice.emergencyStop();
    const QByteArray failEmergencyStop = failClearRequests.back();
    feedResponse(&failClearProtocol, failEmergencyStop,
                 protocol::Command::EmergencyStop, {});
    failClearRequests.clear();
    failClearDevice.clearEmergencyStop();
    const QByteArray failClearRequest = failClearRequests.back();
    if (!require(failClearDevice.getStatus(),
                 "GET_STATUS request was not sent for clear failClosed test")) {
        return 1;
    }
    const QByteArray failStatusRequest = failClearRequests.back();
    feedResponse(&failClearProtocol, failStatusRequest,
                 protocol::Command::GetStatus, QByteArray(1, char(0x00)));
    protocol::Frame failClearResponse = capturedFrame(failClearRequest);
    failClearResponse.flags = protocol::Response;
    failClearResponse.payload.clear();
    failClearProtocol.ingestBytes(
        QByteArrayView(encodeFrame(failClearResponse)));
    if (!require(failClearDevice.emergencyLocked(),
                 "late CLEAR_EMERGENCY_STOP ACK cleared the failClosed lock")) {
        return 1;
    }
    failClearProtocol.clearPending();

    return 0;
}
