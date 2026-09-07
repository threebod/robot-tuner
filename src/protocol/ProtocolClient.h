#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QDeadlineTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include "protocol/FrameParser.h"
#include "protocol/ProtocolTypes.h"

class ProtocolClient : public QObject {
    Q_OBJECT

public:
    explicit ProtocolClient(QObject *parent = nullptr);
    ProtocolClient(int timeoutMs, QObject *parent = nullptr);

    quint8 sendRequest(protocol::Command command, QByteArray payload);
    void ingestBytes(QByteArrayView bytes);
    void clearPending();

signals:
    void bytesReady(QByteArray bytes);
    void responseReceived(protocol::Frame frame);
    void eventReceived(protocol::Frame frame);
    void requestFailed(quint8 sequence, QString reason);
    void connectionCleared();

private slots:
    void checkDeadlines();

private:
    struct PendingRequest {
        protocol::Frame frame;
        int retriesRemaining{1};
        QDeadlineTimer deadline;
    };

    quint8 allocateSequence();
    void startDeadlineTimer();

    static constexpr int kDefaultTimeoutMs = 250;

    const int timeoutMs_;
    quint8 nextSequence_{1};
    QHash<quint8, PendingRequest> pending_;
    FrameParser parser_;
    QTimer deadlineTimer_;
};
