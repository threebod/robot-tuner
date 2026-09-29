#pragma once

#include <QTimer>
#include <QWidget>

#include "device/MechanismActionModel.h"

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;

class MechanismActionPage : public QWidget {
    Q_OBJECT

public:
    explicit MechanismActionPage(QWidget *parent = nullptr);
    bool initialServoSetupRunning() const { return initialServoStage_ != 0; }

public slots:
    void setConnected(bool connected);
    void setMechanismInitialized(bool initialized);
    void setMechanismEstimate(MechanismPoseData pose, QString state);
    void setMechanismCompleted(MechanismPoseData pose);
    void handleDeviceLine(const QString &line);
    void showError(const QString &error);

signals:
    void initializationRequested(MechanismPoseData pose);
    void poseRequested(MechanismPoseData pose);
    void commandRequested(QString command);
    void stopRequested();

private:
    MechanismPoseData initialPose() const;
    MechanismPoseData editedPose() const;
    void updateInitialState();
    void applySequenceToEditors();
    void loadSelectedStep();
    void replaceSelectedStep();
    void requestManualPoseAdjustment(int axis, int value);
    void updatePositionSliders(const MechanismPoseData &pose);
    void refreshTable();
    void updateControls();
    void startReplay(QVector<MechanismStep> steps, const QString &description,
                     int cycleCount = 1);
    void dispatchCurrentStep();
    void finishCurrentStep();
    void cancelReplay();
    bool confirmHighSpeed(const MechanismPoseData &pose);
    QString stepDescription(const MechanismStep &step) const;

    MechanismSequence sequence_;
    QVector<MechanismStep> replaySteps_;
    int replayIndex_{};
    int replayCycle_{1};
    int replayCycleCount_{1};
    QString replayDescription_;
    bool connected_{};
    bool initialized_{};
    bool replaying_{};
    bool awaitingInitialConfirmation_{};
    bool confirmedPoseValid_{};
    bool manualAdjustmentPending_{};
    int initialServoStage_{};
    int initialGripperAngle_{};
    int initialPlatformAngle_{};
    int initialGripperSpeed_{};
    int initialPlatformSpeed_{};
    MechanismPoseData pendingInitialPose_;
    MechanismPoseData confirmedPose_;
    QTimer waitTimer_;

    QLineEdit *nameEdit_{};
    QSpinBox *initialHorizontal_{};
    QSpinBox *initialLift_{};
    QSpinBox *initialTurret_{};
    QComboBox *initialGripper_{};
    QComboBox *initialPlatform_{};
    QSpinBox *gripperOpenAngle_{};
    QSpinBox *gripperCloseAngle_{};
    QSpinBox *gripperSpeed_{};
    QSpinBox *platformSpeed_{};
    QSpinBox *platformAngles_[3]{};
    QPushButton *initializeButton_{};
    QSlider *horizontalPositionSlider_{};
    QSlider *liftPositionSlider_{};
    QSlider *turretPositionSlider_{};
    QLabel *horizontalPositionLabel_{};
    QLabel *liftPositionLabel_{};
    QLabel *turretPositionLabel_{};

    QSpinBox *poseHorizontal_{};
    QSpinBox *poseLift_{};
    QSpinBox *poseTurret_{};
    QSpinBox *horizontalRpm_{};
    QSpinBox *horizontalAccel_{};
    QSpinBox *liftRpm_{};
    QSpinBox *liftAccel_{};
    QSpinBox *turretSpeed_{};
    QSpinBox *stepWait_{};
    QComboBox *gripperStep_{};
    QComboBox *platformStep_{};
    QSpinBox *servoChannel_{};
    QSpinBox *servoAngle_{};
    QSpinBox *waitOnly_{};
    QTableWidget *stepTable_{};
    QComboBox *replaceTypeCombo_{};
    QPushButton *replaceStepButton_{};

    QPushButton *playSelectedButton_{};
    QPushButton *playFromButton_{};
    QPushButton *playAllButton_{};
    QCheckBox *loopEnabled_{};
    QSpinBox *loopCount_{};
    QPushButton *returnInitialButton_{};
    QPushButton *stopButton_{};
    QLabel *stateLabel_{};
};
