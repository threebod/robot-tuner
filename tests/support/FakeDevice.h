#pragma once

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QVariant>

#include "device/ParameterCatalog.h"
#include "protocol/FrameParser.h"
#include "protocol/ProtocolClient.h"

class FakeDevice : public QObject {
public:
    explicit FakeDevice(ProtocolClient *protocol, QObject *parent = nullptr);
    explicit FakeDevice(ProtocolClient &protocol, QObject *parent = nullptr);
    ~FakeDevice() override;

    void setOnline(bool online);
    bool online() const;
    QVariant parameterValue(quint16 id) const;
    int acceptedActionCount() const;
    int rejectedActionCount() const;
    quint8 lastErrorCode() const;
    bool emergencyLocked() const;

private:
    struct StagedValue {
        quint16 id{};
        ValueType type{ValueType::Float32};
        QVariant value;
    };

    void handleBytes(QByteArray bytes);
    void handleFrame(protocol::Frame request);
    void sendResponse(const protocol::Frame &request,
                      const QByteArray &payload);
    void sendError(const protocol::Frame &request, quint8 code);
    void sendEvent(quint8 command, const QByteArray &payload);
    void sendFrame(protocol::Frame frame);
    void sendTelemetryEvents();

    ProtocolClient *protocol_{};
    QMetaObject::Connection bytesConnection_;
    FrameParser parser_;
    ParameterCatalog catalog_;
    QHash<quint16, QVariant> parameters_;
    bool online_{true};
    bool handshakeComplete_{};
    bool testsUnlocked_{};
    bool emergencyLocked_{};
    quint8 telemetryMask_{};
    quint16 telemetryPeriodMs_{};
    quint8 lastErrorCode_{};
    quint32 telemetryTimestampMs_{1000};
    int acceptedActionCount_{};
    int rejectedActionCount_{};
};
