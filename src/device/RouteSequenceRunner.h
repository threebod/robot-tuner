#pragma once

#include <QObject>
#include <QTimer>
#include "device/RouteSequenceModel.h"

class MecanumJogClient;

class RouteSequenceRunner : public QObject {
    Q_OBJECT
public:
    explicit RouteSequenceRunner(MecanumJogClient *client, QObject *parent = nullptr);
    bool start(const RouteSequence &sequence, int from = 0, bool selectedOnly = false);
    bool running() const { return running_; }
    bool awaitingConfirmation() const;
    int currentIndex() const { return index_; }
    void confirm();
    void stop();
    void disconnected();

signals:
    void runningChanged(bool running);
    void progressChanged(int index, QString text);
    void confirmationChanged(bool waiting);
    void failed(QString error);
    void completed();

private:
    enum class Stage { Idle, Move, Turn, Scale, Vision, InitialStatus, Pose, Servo, Wait, Confirm };
    void dispatchStep();
    void dispatchAction();
    void handleLine(const QString &line);
    bool send(const QString &command, bool armed, int timeoutMs);
    void advance();
    void actionDone();
    void finish(bool successful, const QString &error = {});
    void abort(const QString &error);
    void defer(bool action);

    MecanumJogClient *client_;
    RouteSequence sequence_;
    int index_{};
    int end_{};
    int actionIndex_{};
    int cycle_{1};
    quint64 generation_{};
    bool running_{};
    bool seenStart_{};
    bool initialPoseReceived_{};
    bool initialServosReceived_{};
    Stage stage_{Stage::Idle};
    int servoChannel_{};
    int servoAngle_{};
    MechanismPoseData actionPose_;
    MechanismPoseData expectedPose_;
    QTimer timeout_;
    QTimer wait_;
};
