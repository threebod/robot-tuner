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
    bool initializeNavigation(int startZone);
    bool navigateTo(qint32 xMm, qint32 yMm);
    bool navigationInitialized() const;

signals:
    void bytesReady(QByteArray bytes);
    void lineReceived(QString line);
    void commandStateChanged(QString state);
    void commandFailed(QString reason);
    void stopRequested();
    void navigationEstimateReceived(qint32 xMm, qint32 yMm,
                                    double yawDegrees, QString state);
    void navigationValidityChanged(bool initialized);
    void navigationCompleted(qint32 xMm, qint32 yMm);
    void navigationError(QString reason);

private:
    bool validCommand(const QString &command) const;
    void handleLine(const QString &line);
    void failPending(const QString &reason);
    void invalidateNavigation();

    static constexpr int kMaximumReceiveBuffer = 1024;
    QByteArray receiveBuffer_;
    QString pendingCommand_;
    QTimer heartbeatTimer_;
    QTimer armTimer_;
    bool connected_{};
    bool navigationInitialized_{};
};
