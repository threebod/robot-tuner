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
    bool startRawPickRoute(int startZone, quint16 rpm = 60);
    bool startMissionRoute(int startZone, quint16 rpm = 60,
                           int forwardMilli = 640, int rightMilli = 673);
    bool continueRawPickRoute();
    bool fullRouteRunning() const;
    bool initializeMechanism(const MechanismPoseData &pose);
    bool moveMechanism(const MechanismPoseData &pose);
    bool requestMechanismStatus();
    bool mechanismInitialized() const;
    bool startVisionMaterialPickup(int color);
    bool startVisionRingAlignment(int ring);
    bool setVisionRingScale(int ring, int forwardMilli, int rightMilli);
    bool pauseVision();
    bool requestVisionStatus();
    bool jogVision(int forwardMm, int rightMm, int rpm);
    bool visionRunning() const;

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
    void rawPickRouteStateChanged(QString state, int color, int slot);
    void rawPickRouteContinued();
    void missionPhaseChanged(QString phase, QString station);
    void qrResultReceived(QString value);
    void mechanismEstimateReceived(MechanismPoseData pose, QString state);
    void mechanismValidityChanged(bool initialized);
    void mechanismCompleted(MechanismPoseData pose);
    void mechanismError(QString reason);
    void visionStateChanged(QString state);
    void visionRingScaleApplied(int ring, int forwardMilli, int rightMilli);
    void visionSampleReceived(int du, int dv, double forwardMm,
                              double rightMm, int quality, int iteration);
    void visionRunningChanged(bool running);
    void visionCompleted(double forwardMm, double rightMm, int iterations);
    void visionError(QString reason);

private:
    bool validCommand(const QString &command) const;
    void handleLine(const QString &line);
    void failPending(const QString &reason);
    void invalidateNavigation();
    void invalidateMechanism();
    void setFullRouteRunning(bool running);
    void setVisionRunning(bool running);
    void failMissionSetup(const QString &reason);
    void dispatchMissionSetup(const QString &command, bool armed);

    static constexpr int kMaximumReceiveBuffer = 1024;
    QByteArray receiveBuffer_;
    QString pendingCommand_;
    QTimer heartbeatTimer_;
    QTimer armTimer_;
    QTimer missionSetupTimer_;
    int missionSetupStage_{};
    int missionStartZone_{};
    quint16 missionRpm_{};
    int missionForwardMilli_{};
    int missionRightMilli_{};
    bool missionSetupDispatch_{};
    bool connected_{};
    bool navigationInitialized_{};
    bool mechanismInitialized_{};
    bool fullRouteRunning_{};
    bool visionRunning_{};
};
