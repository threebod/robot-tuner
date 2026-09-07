#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QEventLoop>
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

    ProtocolClient eventClient(10);
    int eventCount = 0;
    int disconnectFailureCount = 0;
    QString disconnectReason;
    QObject::connect(&eventClient, &ProtocolClient::eventReceived,
                     [&](const protocol::Frame &) { ++eventCount; });
    QObject::connect(&eventClient, &ProtocolClient::requestFailed,
                     [&](quint8, const QString &reason) {
                         ++disconnectFailureCount;
                         disconnectReason = reason;
                     });

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

    return 0;
}
