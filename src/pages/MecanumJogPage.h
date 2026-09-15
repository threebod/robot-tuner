#pragma once

#include <QWidget>

class QLabel;
class QSlider;
class QSpinBox;
class QComboBox;

class MecanumJogPage : public QWidget {
    Q_OBJECT

public:
    explicit MecanumJogPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setCommandState(const QString &state);
    void appendLine(const QString &line);
    void showError(const QString &error);

signals:
    void commandRequested(QString command);
    void armedCommandRequested(QString command);

private:
    QWidget *controls_{};
    QLabel *stateLabel_{};
    QLabel *replyLabel_{};
    QComboBox *servoIdCombo_{};
    QSlider *servoAngleSlider_{};
    QLabel *servoAngleLabel_{};
};
