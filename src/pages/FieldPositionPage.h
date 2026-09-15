#pragma once

#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include "device/TelemetryTypes.h"

class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
class SerialDebugPanel;

class FieldMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit FieldMapWidget(QWidget *parent = nullptr);

    static QPointF fieldToScreen(QPointF fieldPoint, QRectF screenRect);
    static QPointF headingVector(double yawDegrees);
    QPointF displayedFieldPosition() const;
    QPointF targetFieldPosition() const;
    void setPoseSample(PoseSample sample);
    void setTargetPoint(QPointF point, bool visible);

signals:
    void fieldPointSelected(QPointF fieldPoint);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF fieldRect() const;

    PoseSample pose_;
    QPointF targetPoint_;
    bool targetVisible_{};
};

class FieldPositionPage : public QWidget {
    Q_OBJECT

public:
    explicit FieldPositionPage(QWidget *parent = nullptr);

    void setPoseSample(PoseSample sample, const QString &source);
    void setPoseCapabilityAvailable(bool available);
    void setConnectionState(const QString &state, bool connected);
    void setImuSample(ImuSample sample);
    void setDeviceStatus(DeviceStatus status);
    void setDeviceError(const QString &message);
    void setEmergencyLocked(bool locked);
    void appendSerialTx(const QByteArray &bytes);
    void appendSerialRx(const QByteArray &bytes);
    void setNavigationMode(bool enabled);
    void setNavigationConnected(bool connected);
    void setNavigationInitialized(bool initialized);
    void setNavigationEstimate(qint32 xMm, qint32 yMm, double yawDegrees,
                               const QString &state);
    void setNavigationError(const QString &message);

public slots:
    void selectFieldPoint(QPointF fieldPoint);

signals:
    void setPoseRequested(PoseSample sample);
    void rawSendRequested(QByteArray bytes);
    void navigationInitRequested(int startZone);
    void navigationTargetRequested(qint32 xMm, qint32 yMm, quint16 rpm);

private:
    void applyInputPose();
    void applyPreset(qint32 xMm, qint32 yMm);
    void refreshStatus();
    void refreshNavigationControls();
    QPointF nearestNavigationPoint(QPointF fieldPoint) const;

    FieldMapWidget *map_{};
    QCheckBox *simulationCheckBox_{};
    QGroupBox *poseControlGroup_{};
    QGroupBox *navigationControlGroup_{};
    QDoubleSpinBox *xSpinBox_{};
    QDoubleSpinBox *ySpinBox_{};
    QDoubleSpinBox *yawSpinBox_{};
    QLabel *valueLabel_{};
    QLabel *noticeLabel_{};
    QLabel *sourceLabel_{};
    QLabel *updatedLabel_{};
    QLabel *statusLabel_{};
    QLabel *capabilityLabel_{};
    QLabel *steeringAngleLabel_{};
    QLabel *steeringUpdatedLabel_{};
    QLabel *steeringStatusLabel_{};
    QLabel *connectionStateLabel_{};
    QLabel *emergencyStateLabel_{};
    QLabel *errorCodeLabel_{};
    QLabel *navigationTargetLabel_{};
    QLabel *navigationStateLabel_{};
    QPushButton *navigationMoveButton_{};
    QSpinBox *navigationRpmSpin_{};
    SerialDebugPanel *serialPanel_{};
    QTimer *staleTimer_{};
    QElapsedTimer lastUpdate_;
    QElapsedTimer lastImuUpdate_;
    PoseSample pose_;
    bool outOfBounds_{};
    bool navigationMode_{};
    bool navigationConnected_{};
    bool navigationInitialized_{};
    bool navigationRunning_{};
    bool navigationTargetValid_{};
    QPointF navigationTarget_;
};
