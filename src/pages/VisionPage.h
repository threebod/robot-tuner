#pragma once

#include <QWidget>
#include <array>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

class VisionPage : public QWidget {
    Q_OBJECT

public:
    explicit VisionPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setOtherMotionRunning(bool running);
    void setVisionRunning(bool running);
    void setVisionState(const QString &state);
    void setVisionSample(int du, int dv, double forwardMm, double rightMm,
                         int quality, int iteration);
    void showVisionError(const QString &error);
    void setRingScaleApplied(int ring, int forwardMilli, int rightMilli);
    int ring2ForwardMilli() const { return forwardScales_[1]; }
    int ring2RightMilli() const { return rightScales_[1]; }

signals:
    void materialPickupRequested(int color);
    void ringAlignmentRequested(int ring);
    void ringScaleRequested(int ring, int forwardMilli, int rightMilli);
    void pauseRequested();
    void statusRequested();
    void jogRequested(int forwardMm, int rightMm, int rpm);

private:
    void rebuildTargets();
    void refreshControls();
    void refreshScaleInputs();
    void requestJog(int forwardSign, int rightSign);

    QComboBox *modeCombo_{};
    QComboBox *targetCombo_{};
    QComboBox *stepCombo_{};
    QSpinBox *rpmSpin_{};
    QDoubleSpinBox *forwardScaleSpin_{};
    QDoubleSpinBox *rightScaleSpin_{};
    QPushButton *applyScaleButton_{};
    QLabel *scaleLabel_{};
    std::array<int, 3> forwardScales_{{640, 640, 640}};
    std::array<int, 3> rightScales_{{673, 673, 673}};
    std::array<bool, 3> scalesApplied_{{false, false, false}};
    QPushButton *startButton_{};
    QPushButton *pauseButton_{};
    QWidget *jogControls_{};
    QLabel *stateLabel_{};
    QLabel *sampleLabel_{};
    bool connected_{};
    bool otherMotionRunning_{};
    bool running_{};
    bool manualEnabled_{};
};
