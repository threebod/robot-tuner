#pragma once

#include <QWidget>

#include "device/TelemetryTypes.h"

class QLabel;

class OverviewPage : public QWidget {
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = nullptr);

    void setDeviceInfo(const DeviceInfo &info);
    void setLinkState(const QString &state);
    void setLatency(qint64 latencyMs);
    void setImuSample(const ImuSample &sample);
    void setPidSample(const PidSample &sample);
    void setStatus(const DeviceStatus &status);
    void setDeviceError(const QString &message);

private:
    void setStateLabel(QLabel *label, const QString &text, bool abnormal);

    QLabel *protocolVersionLabel_{};
    QLabel *firmwareVersionLabel_{};
    QLabel *linkStateLabel_{};
    QLabel *latencyLabel_{};
    QLabel *modeLabel_{};
    QLabel *debugStateLabel_{};
    QLabel *lockedStateLabel_{};
    QLabel *emergencyStateLabel_{};
    QLabel *rollLabel_{};
    QLabel *pitchLabel_{};
    QLabel *yawLabel_{};
    QLabel *lastErrorCodeLabel_{};
    QLabel *deviceErrorLabel_{};
    QLabel *pidTargetLabel_{};
    QLabel *pidActualLabel_{};
    QLabel *pidOutputLabel_{};
};
