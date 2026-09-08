#pragma once

#include <QByteArrayView>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVector>

#include "device/ParameterCatalog.h"
#include "device/TelemetryTypes.h"
#include "protocol/ProtocolClient.h"

class DeviceClient : public QObject {
    Q_OBJECT

public:
    explicit DeviceClient(ProtocolClient *protocol, QObject *parent = nullptr);
    explicit DeviceClient(ProtocolClient &protocol, QObject *parent = nullptr);

    bool hello();
    bool getStatus();
    bool getParameterGroup(quint8 group);
    bool setParameterGroup(quint8 group,
                           const QVector<ParameterValue> &values);
    bool setTelemetry(quint8 mask, quint16 periodMs);
    bool calibrateImu();

    // Controlled test actions.  These methods validate the host-side safety
    // envelope before creating a protocol request; the STM32 applies the
    // same limits again at the hardware boundary.
    bool unlockTests();
    bool testChassis(qint32 vx, qint32 vy, qint32 w, qint32 durationMs);
    bool testHorizontal(double target, qint32 speed, qint32 accel);
    bool testLift(double target, qint32 speed, qint32 accel);
    bool testTurret(double angle, double interpolationSpeed);
    bool setPlatformPosition(qint32 position);
    bool setGripperOpen(bool open);
    bool stop();
    bool emergencyStop();
    bool clearEmergencyStop();

    bool handshakeComplete() const;
    bool testsUnlocked() const;
    bool testActionsEnabled() const;
    qint64 unlockRemainingMs() const;
    bool emergencyLocked() const;
    const ParameterCatalog &parameterCatalog() const;

signals:
    void handshakeCompleted(DeviceInfo info);
    void parameterGroupReceived(quint8 group, QVector<ParameterValue> values);
    void parameterGroupReadFailed(quint8 group, QString reason);
    void imuSampleReceived(ImuSample sample);
    void pidSampleReceived(PidSample sample);
    void statusReceived(DeviceStatus status);
    void telemetryConfigured(quint8 acceptedMask, quint16 actualPeriodMs);
    void imuCalibrationStateChanged(quint8 state);
    void testUnlockStateChanged(bool unlocked, qint64 remainingMs);
    void emergencyStateChanged(bool locked);
    void deviceError(QString message);
    void terminalLog(QString message);

private slots:
    void handleResponse(protocol::Frame frame);
    void handleEvent(protocol::Frame frame);
    void handleRequestFailure(quint8 sequence, QString reason);
    void handleConnectionCleared();

private:
    bool send(protocol::Command command, const QByteArray &payload);
    bool sendPidPage(quint8 page);
    bool reject(const QString &message, quint8 code);
    void reportError(quint8 code, const QString &detail);
    void failParameterGroupRead(const QString &reason);

    void decodeHello(const QByteArray &payload);
    void decodeParameterGroup(const QByteArray &payload,
                              protocol::Command command, quint8 sequence);
    void resetParameterGroupRead();
    void decodeImu(const QByteArray &payload);
    void decodePid(const QByteArray &payload);
    void decodeStatus(const QByteArray &payload);
    void decodeStatusResponse(const QByteArray &payload);
    void decodeTelemetryConfiguration(const QByteArray &payload);
    void decodeCalibrationState(const QByteArray &payload);
    void decodeTestUnlock(const QByteArray &payload);
    void decodeEmptyResponse(const QByteArray &payload,
                             protocol::Command command);
    void decodeResponseError(const QByteArray &payload);

    bool requireActionAccess();
    bool sendTestAction(const QByteArray &payload);
    void refreshUnlockState();
    void failClosed();
    void setTestsUnlocked(bool unlocked);
    void setEmergencyLocked(bool locked);

    ProtocolClient *protocol_{};
    ParameterCatalog catalog_;
    bool handshakeComplete_{};
    bool helloPending_{};
    quint8 helloSequence_{};
    bool unlockPending_{};
    quint8 unlockSequence_{};
    bool clearEmergencyStopPending_{};
    quint8 clearEmergencyStopSequence_{};
    quint64 safetyGeneration_{};
    quint64 clearEmergencyStopGeneration_{};
    bool pidGroupReadPending_{};
    quint8 pidGroupPendingPage_{};
    quint8 pidGroupPageSequence_{};
    QVector<ParameterValue> pidGroupValues_;
    bool testsUnlocked_{};
    bool emergencyLocked_{};
    qint64 unlockDurationMs_{};
    QElapsedTimer unlockElapsed_;
    QTimer unlockTimer_;
};
