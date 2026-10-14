#pragma once

#include <QObject>
#include <QPointF>
#include <QTimer>
#include <array>

class MecanumJogClient;

class RingCalibrationRunner : public QObject {
    Q_OBJECT
public:
    explicit RingCalibrationRunner(MecanumJogClient *client, QObject *parent = nullptr);
    bool start();
    bool startMaterial(int color);
    bool material() const { return materialColor_ != 0; }
    void stop();
    bool running() const { return running_; }

signals:
    void runningChanged(bool running);
    void progressChanged(QString text);
    void scalesCalculated(int forwardMilli, int rightMilli);
    void completed();
    void failed(QString reason);

private:
    enum class Stage { Idle, Sample, Ready, Move, Settle, Apply };
    bool startCalibration(int color);
    void requestSample();
    void move();
    void handleLine(const QString &line);
    bool calculateScales();
    bool send(const QString &command, bool armed, int timeoutMs);
    void defer(bool apply);
    void finish(bool successful, const QString &reason = {});
    void abort(const QString &reason);

    MecanumJogClient *client_;
    QTimer timeout_, settle_;
    std::array<QPointF, 5> samples_{};
    int sampleIndex_{};
    int materialColor_{};
    quint16 sampleId_{};
    quint64 generation_{};
    int forwardMilli_{}, rightMilli_{};
    Stage stage_{Stage::Idle};
    bool running_{}, seenMove_{};
};
