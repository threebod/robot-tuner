#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QObject>
#include <QString>
#include <QTimer>

class MecanumJogClient : public QObject {
    Q_OBJECT

public:
    explicit MecanumJogClient(QObject *parent = nullptr);

    void setConnected(bool connected);
    void ingestBytes(QByteArrayView bytes);
    bool sendCommand(QString command);
    bool sendArmedCommand(QString command);
    bool emergencyStop();

signals:
    void bytesReady(QByteArray bytes);
    void lineReceived(QString line);
    void commandStateChanged(QString state);
    void commandFailed(QString reason);
    void stopRequested();

private:
    bool validCommand(const QString &command) const;
    void handleLine(const QString &line);
    void failPending(const QString &reason);

    static constexpr int kMaximumReceiveBuffer = 1024;
    QByteArray receiveBuffer_;
    QString pendingCommand_;
    QTimer heartbeatTimer_;
    QTimer armTimer_;
    bool connected_{};
};
