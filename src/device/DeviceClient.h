#pragma once

#include <QByteArrayView>
#include <QObject>
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
    bool getParameterGroup(quint8 group);
    bool setParameterGroup(quint8 group,
                           const QVector<ParameterValue> &values);
    bool setTelemetry(quint8 mask, quint16 periodMs);
    bool calibrateImu();

    bool handshakeComplete() const;
    const ParameterCatalog &parameterCatalog() const;

signals:
    void handshakeCompleted(DeviceInfo info);
    void parameterGroupReceived(quint8 group, QVector<ParameterValue> values);
    void imuSampleReceived(ImuSample sample);
    void pidSampleReceived(PidSample sample);
    void deviceError(QString message);
    void terminalLog(QString message);

private slots:
    void handleResponse(protocol::Frame frame);
    void handleEvent(protocol::Frame frame);
    void handleRequestFailure(quint8 sequence, QString reason);

private:
    bool send(protocol::Command command, const QByteArray &payload);
    bool reject(const QString &message, quint8 code);
    void reportError(quint8 code, const QString &detail);

    void decodeHello(const QByteArray &payload);
    void decodeParameterGroup(const QByteArray &payload);
    void decodeImu(const QByteArray &payload);
    void decodePid(const QByteArray &payload);
    void decodeStatus(const QByteArray &payload);
    void decodeResponseError(const QByteArray &payload);

    ProtocolClient *protocol_{};
    ParameterCatalog catalog_;
    bool handshakeComplete_{};
    bool helloPending_{};
    quint8 helloSequence_{};
};
