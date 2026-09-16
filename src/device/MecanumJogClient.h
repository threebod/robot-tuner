#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QObject>
#include <QString>
#include <QTimer>

#include "device/MechanismActionModel.h"

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
    bool navigateTo(qint32 xMm, qint32 yMm, quint16 rpm = 60);
    bool navigationInitialized() const;
    bool startFullRoute(int startZone, quint16 rpm = 60);
    bool fullRouteRunning() const;
    bool initializeMechanism(const MechanismPoseData &pose);
    bool moveMechanism(const MechanismPoseData &pose);
    bool requestMechanismStatus();
    bool mechanismInitialized() const;

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
    void fullRouteEstimateReceived(qint32 xMm, qint32 yMm,
                                   double yawDegrees, QString state,
                                   QString stage, qint32 targetX,
                                   qint32 targetY);
    void fullRouteStageChanged(int index, QString stage);
    void fullRouteRunningChanged(bool running);
    void fullRouteCompleted(qint32 xMm, qint32 yMm);
    void fullRouteError(QString reason);
    void mechanismEstimateReceived(MechanismPoseData pose, QString state);
    void mechanismValidityChanged(bool initialized);
    void mechanismCompleted(MechanismPoseData pose);
    void mechanismError(QString reason);

private:
    bool validCommand(const QString &command) const;
    void handleLine(const QString &line);
    void failPending(const QString &reason);
    void invalidateNavigation();
    void invalidateMechanism();
    void setFullRouteRunning(bool running);

    static constexpr int kMaximumReceiveBuffer = 1024;
    QByteArray receiveBuffer_;
    QString pendingCommand_;
    QTimer heartbeatTimer_;
    QTimer armTimer_;
    bool connected_{};
    bool navigationInitialized_{};
    bool mechanismInitialized_{};
    bool fullRouteRunning_{};
};
