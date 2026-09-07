#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <iostream>

#include "protocol/FrameCodec.h"
#include "protocol/FrameParser.h"
#include "protocol/ProtocolClient.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);

    ProtocolClient client(10);
    int bytesReadyCount = 0;
    int responseCount = 0;
    int failureCount = 0;
    QByteArray requestBytes;

    QObject::connect(&client, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         ++bytesReadyCount;
                         requestBytes = bytes;
                     });
    QObject::connect(&client, &ProtocolClient::responseReceived,
                     [&](const protocol::Frame &) { ++responseCount; });
    QObject::connect(&client, &ProtocolClient::requestFailed,
                     [&](quint8, const QString &) { ++failureCount; });

    const quint8 sequence = client.sendRequest(
        protocol::Command::GetStatus, QByteArray("request"));
    if (!require(bytesReadyCount == 1,
                 "sending a request did not emit one encoded frame")) {
        return 1;
    }

    FrameParser parser;
    const QVector<protocol::Frame> requests =
        parser.push(QByteArrayView(requestBytes));
    if (!require(requests.size() == 1 && requests.front().sequence == sequence,
                 "encoded request sequence could not be recovered")) {
        return 1;
    }

    protocol::Frame response = requests.front();
    response.flags = protocol::Response;
    response.payload = QByteArray("response");
    const QByteArray responseBytes = encodeFrame(response);
    client.ingestBytes(QByteArrayView(responseBytes));
    client.ingestBytes(QByteArrayView(responseBytes));

    if (!require(responseCount == 1,
                 "a matching response was not emitted exactly once")) {
        return 1;
    }
    if (!require(bytesReadyCount == 1 && failureCount == 0,
                 "a matched response left a pending request behind")) {
        return 1;
    }

    const int responseCountBeforeWrongCommand = responseCount;
    const quint8 commandMatchSequence = client.sendRequest(
        protocol::Command::GetStatus, {});
    const QVector<protocol::Frame> commandMatchRequests =
        parser.push(QByteArrayView(requestBytes));
    if (!require(commandMatchRequests.size() == 1 &&
                     commandMatchRequests.front().sequence ==
                         commandMatchSequence,
                 "command-match request sequence could not be recovered")) {
        return 1;
    }
    protocol::Frame wrongCommandResponse = commandMatchRequests.front();
    wrongCommandResponse.flags = protocol::Response;
    wrongCommandResponse.command =
        static_cast<quint8>(protocol::Command::Hello);
    wrongCommandResponse.payload = QByteArray("wrong-command");
    client.ingestBytes(QByteArrayView(encodeFrame(wrongCommandResponse)));
    if (!require(responseCount == responseCountBeforeWrongCommand,
                 "a response with a matching sequence but wrong command was accepted")) {
        return 1;
    }
    protocol::Frame matchingCommandResponse = wrongCommandResponse;
    matchingCommandResponse.command =
        static_cast<quint8>(protocol::Command::GetStatus);
    matchingCommandResponse.payload = QByteArray("matching-command");
    client.ingestBytes(QByteArrayView(encodeFrame(matchingCommandResponse)));
    if (!require(responseCount == responseCountBeforeWrongCommand + 1,
                 "a response with a matching command was not accepted")) {
        return 1;
    }

    const quint8 errorSequence = client.sendRequest(
        protocol::Command::GetStatus, QByteArray("error-request"));
    const QVector<protocol::Frame> errorRequests =
        parser.push(QByteArrayView(requestBytes));
    if (!require(errorRequests.size() == 1 &&
                     errorRequests.front().sequence == errorSequence,
                 "error request sequence could not be recovered")) {
        return 1;
    }
    protocol::Frame errorResponse = errorRequests.front();
    errorResponse.flags = protocol::Error;
    errorResponse.payload = QByteArray("error-response");
    client.ingestBytes(QByteArrayView(encodeFrame(errorResponse)));
    if (!require(responseCount == responseCountBeforeWrongCommand + 2 &&
                     failureCount == 0,
                 "an error response did not match the pending request")) {
        return 1;
    }

    ProtocolClient retryClient(10);
    int retryBytesReadyCount = 0;
    int retryFailureCount = 0;
    QByteArray firstRetryBytes;
    QByteArray secondRetryBytes;
    QObject::connect(&retryClient, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         ++retryBytesReadyCount;
                         if (retryBytesReadyCount == 1) {
                             firstRetryBytes = bytes;
                         } else if (retryBytesReadyCount == 2) {
                             secondRetryBytes = bytes;
                         }
                     });
    QObject::connect(&retryClient, &ProtocolClient::requestFailed,
                     [&](quint8, const QString &) { ++retryFailureCount; });

    retryClient.sendRequest(protocol::Command::GetStatus, {});
    QEventLoop retryLoop;
    QTimer::singleShot(35, &retryLoop, &QEventLoop::quit);
    retryLoop.exec();

    if (!require(retryBytesReadyCount == 2,
                 "a timed-out request did not emit exactly one retry")) {
        return 1;
    }
    if (!require(firstRetryBytes == secondRetryBytes,
                 "the retry did not resend the original encoded frame")) {
        return 1;
    }
    if (!require(retryFailureCount == 1,
                 "a request did not fail after its retry was exhausted")) {
        return 1;
    }

    ProtocolClient reentrantClient(10);
    int reentrantBytesReadyCount = 0;
    int reentrantDisconnectFailureCount = 0;
    QObject::connect(&reentrantClient, &ProtocolClient::bytesReady,
                     [&](const QByteArray &) {
                         ++reentrantBytesReadyCount;
                         if (reentrantBytesReadyCount == 3) {
                             reentrantClient.clearPending();
                         }
                     });
    QObject::connect(&reentrantClient, &ProtocolClient::requestFailed,
                     [&](quint8, const QString &) {
                         ++reentrantDisconnectFailureCount;
                     });

    reentrantClient.sendRequest(protocol::Command::GetStatus, {});
    reentrantClient.sendRequest(protocol::Command::GetStatus, {});
    QThread::msleep(25);
    QEventLoop reentrantLoop;
    QTimer::singleShot(10, &reentrantLoop, &QEventLoop::quit);
    reentrantLoop.exec();
    if (!require(reentrantBytesReadyCount == 3,
                 "a stale retry was emitted after clearPending reentrancy")) {
        return 1;
    }
    if (!require(reentrantDisconnectFailureCount == 2,
                 "clearPending did not fail all reentrant requests")) {
        return 1;
    }

    ProtocolClient eventClient(10);
    int eventCount = 0;
    int disconnectFailureCount = 0;
    int connectionClearedCount = 0;
    QString disconnectReason;
    QObject::connect(&eventClient, &ProtocolClient::eventReceived,
                     [&](const protocol::Frame &) { ++eventCount; });
    QObject::connect(&eventClient, &ProtocolClient::requestFailed,
                     [&](quint8, const QString &reason) {
                         ++disconnectFailureCount;
                         disconnectReason = reason;
                     });
    QObject::connect(&eventClient, &ProtocolClient::connectionCleared,
                     [&]() { ++connectionClearedCount; });

    const quint8 pendingSequence = eventClient.sendRequest(
        protocol::Command::GetStatus, {});
    protocol::Frame event;
    event.flags = protocol::Event;
    event.sequence = pendingSequence;
    event.command = static_cast<quint8>(protocol::Command::StatusTelemetry);
    event.payload = QByteArray("event");
    eventClient.ingestBytes(QByteArrayView(encodeFrame(event)));
    if (!require(eventCount == 1,
                 "an event frame was not delivered through eventReceived")) {
        return 1;
    }

    eventClient.clearPending();
    if (!require(disconnectFailureCount == 1 &&
                     disconnectReason == QString::fromUtf8("连接已断开"),
                 "clearing pending requests did not report the disconnect")) {
        return 1;
    }
    if (!require(connectionClearedCount == 1,
                 "clearing pending requests did not emit connectionCleared")) {
        return 1;
    }
    eventClient.clearPending();
    if (!require(connectionClearedCount == 2,
                 "clearing an empty pending table did not emit connectionCleared")) {
        return 1;
    }

    return 0;
}
