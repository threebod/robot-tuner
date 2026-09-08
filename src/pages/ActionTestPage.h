#pragma once

#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

class ActionTestPage : public QWidget {
    Q_OBJECT

public:
    explicit ActionTestPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setTestActionsEnabled(bool enabled, qint64 remainingMs = 0);
    void setEmergencyLocked(bool locked);
    void setDeviceError(const QString &message);

signals:
    void unlockRequested();
    void chassisRequested(qint32 vx, qint32 vy, qint32 w, qint32 durationMs);
    void horizontalRequested(double target, qint32 speed, qint32 accel);
    void liftRequested(double target, qint32 speed, qint32 accel);
    void turretRequested(double angle, double interpolationSpeed);
    void platformPositionRequested(qint32 position);
    void gripperRequested(bool open);
    void stopRequested();

private slots:
    void requestChassis();
    void requestHorizontal();
    void requestLift();
    void requestTurret();
    void requestPlatformPosition();
    void requestGripperOpen();
    void requestGripperClose();
    void requestUnlock();
    void requestStop();

private:
    bool confirmAction(const QString &description) const;
    void requestDirectional(qint32 vx, qint32 vy, qint32 w,
                            const QString &description);
    void setActionWidgetsEnabled(bool enabled);
    void setStatus(const QString &message);

    bool connected_{};
    bool testActionsEnabled_{};
    bool emergencyLocked_{};
    QLabel *unlockStatusLabel_{};
    QLabel *remainingLabel_{};
    QLabel *statusLabel_{};
    QPushButton *unlockButton_{};
    QPushButton *stopButton_{};
    QSpinBox *chassisVxSpinBox_{};
    QSpinBox *chassisVySpinBox_{};
    QSpinBox *chassisWSpinBox_{};
    QSpinBox *chassisDurationSpinBox_{};
    QDoubleSpinBox *horizontalTargetSpinBox_{};
    QSpinBox *horizontalSpeedSpinBox_{};
    QSpinBox *horizontalAccelSpinBox_{};
    QDoubleSpinBox *liftTargetSpinBox_{};
    QSpinBox *liftSpeedSpinBox_{};
    QSpinBox *liftAccelSpinBox_{};
    QDoubleSpinBox *turretAngleSpinBox_{};
    QDoubleSpinBox *turretSpeedSpinBox_{};
    QComboBox *platformPositionCombo_{};
    QVector<QWidget *> actionWidgets_;
};
