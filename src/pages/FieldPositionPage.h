#pragma once

#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include "device/TelemetryTypes.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QTimer;

class FieldMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit FieldMapWidget(QWidget *parent = nullptr);

    static QPointF fieldToScreen(QPointF fieldPoint, QRectF screenRect);
    static QPointF headingVector(double yawDegrees);
    QPointF displayedFieldPosition() const;
    void setPoseSample(PoseSample sample);

signals:
    void fieldPointSelected(QPointF fieldPoint);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF fieldRect() const;

    PoseSample pose_;
};

class FieldPositionPage : public QWidget {
    Q_OBJECT

public:
    explicit FieldPositionPage(QWidget *parent = nullptr);

    void setPoseSample(PoseSample sample, const QString &source);
    void setPoseCapabilityAvailable(bool available);

public slots:
    void selectFieldPoint(QPointF fieldPoint);

signals:
    void setPoseRequested(PoseSample sample);

private:
    void applyInputPose();
    void applyPreset(qint32 xMm, qint32 yMm);
    void refreshStatus();

    FieldMapWidget *map_{};
    QCheckBox *simulationCheckBox_{};
    QDoubleSpinBox *xSpinBox_{};
    QDoubleSpinBox *ySpinBox_{};
    QDoubleSpinBox *yawSpinBox_{};
    QLabel *valueLabel_{};
    QLabel *sourceLabel_{};
    QLabel *updatedLabel_{};
    QLabel *statusLabel_{};
    QLabel *capabilityLabel_{};
    QTimer *staleTimer_{};
    QElapsedTimer lastUpdate_;
    PoseSample pose_;
    bool outOfBounds_{};
};
