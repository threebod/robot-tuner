#pragma once

#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
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
    void servoRequested(qint32 servoId, qint32 targetAngleDegrees);
    void stopRequested();

private slots:
    void requestChassis();
    void requestHorizontal();
    void requestLift();
    void requestTurret();
    void requestPlatformPosition();
    void requestGripperOpen();
    void requestGripperClose();
    void requestServo(qint32 servoId, QSpinBox *angleSpinBox);
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
    QSlider *servo2Slider_{};
    QSpinBox *servo2AngleSpinBox_{};
    QPushButton *servo2SendButton_{};
    QSlider *servo3Slider_{};
    QSpinBox *servo3AngleSpinBox_{};
    QPushButton *servo3SendButton_{};
    QSlider *servo4Slider_{};
    QSpinBox *servo4AngleSpinBox_{};
    QPushButton *servo4SendButton_{};
    QListWidget *servoActionLog_{};
    QVector<QWidget *> actionWidgets_;
};
