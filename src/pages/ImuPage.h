#pragma once

#include <QVector>
#include <QWidget>

#include "device/TelemetryTypes.h"

class QLabel;
class QSpinBox;
class QPushButton;
class TelemetryPlot;

class ImuPage : public QWidget {
    Q_OBJECT

public:
    explicit ImuPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setLinkBaudRate(qint32 baudRate);
    void setTelemetryConfiguration(quint8 acceptedMask, quint16 periodMs);
    void setCalibrationState(quint8 state);
    void setImuSample(const ImuSample &sample);
    void setPidSample(const PidSample &sample);
    void setStatus(const DeviceStatus &status);
    void setDeviceError(const QString &message);

signals:
    void calibrationRequested();
    void telemetryRateChanged(quint16 hz);

private slots:
    void requestCalibration();
    void handleRateChanged(int hz);

private:
    void setValue(QLabel *label, double value, const QString &unit,
                  bool abnormal = false);
    void setAbnormal(const QString &message);
    void updateRateRange();

    bool connected_{};
    qint32 baudRate_{115200};
    int maximumRate_{50};
    QSpinBox *rateSpinBox_{};
    QPushButton *calibrationButton_{};
    QLabel *rateStatusLabel_{};
    QLabel *calibrationStatusLabel_{};
    QLabel *errorLabel_{};
    QLabel *timestampLabel_{};
    QVector<QLabel *> valueLabels_;
    QLabel *pidTargetLabel_{};
    QLabel *pidActualLabel_{};
    QLabel *pidOutputLabel_{};
    TelemetryPlot *accelerationPlot_{};
    TelemetryPlot *angularVelocityPlot_{};
    TelemetryPlot *attitudePlot_{};
};
