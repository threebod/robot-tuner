#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QRegularExpression>
#include <QThread>
#include <QStringList>
#include <iostream>
#include "device/MecanumJogClient.h"
#include "device/RingCalibrationRunner.h"

static bool require(bool condition, const char *message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

static void events(int ms = 1) {
    QElapsedTimer timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    } while (timer.elapsed() < ms);
}

struct Fixture {
    MecanumJogClient client;
    RingCalibrationRunner runner{&client};
    QStringList commands;
    QString error;
    int completed{}, forward{}, right{};
    int color{};
    Fixture() {
        QObject::connect(&client, &MecanumJogClient::bytesReady, [&](QByteArray bytes) {
            if (bytes != "hb\r\n") commands.append(QString::fromUtf8(bytes).trimmed());
        });
        QObject::connect(&runner, &RingCalibrationRunner::failed, [&](QString reason) { error = reason; });
        QObject::connect(&runner, &RingCalibrationRunner::scalesCalculated,
                         [&](int f, int r) { forward = f; right = r; });
        QObject::connect(&runner, &RingCalibrationRunner::completed, [&] { ++completed; });
        client.setConnected(true);
    }
    void receive(const QString &line) { client.ingestBytes((line + "\r\n").toUtf8()); }
    QString last() const { return commands.isEmpty() ? QString() : commands.last(); }
    bool waitForSample() {
        QElapsedTimer timer;
        timer.start();
        while (runner.running() && !last().startsWith((color ? "vision sample material " : "vision sample ring 2 ")) && timer.elapsed() < 600)
            events(2);
        return last().startsWith((color ? "vision sample material " : "vision sample ring 2 "));
    }
    void sample(QPointF point, int idOffset = 0, int quality = 90) {
        const int id = last().section(' ', -1).toInt() + idOffset;
        receive(QStringLiteral("VISION MEASURE id=%1 %6 token=%2 du=%3 dv=%4 quality=%5")
                    .arg(id).arg(id).arg(qRound(point.x())).arg(qRound(point.y())).arg(quality)
                    .arg(color ? QStringLiteral("material=%1").arg(color) : QStringLiteral("ring=2")));
        receive("VISION STATE state=IDLE mode=RING selector=0 target=2 iteration=0");
    }
    bool move(int index) {
        events();
        if (!require(last() == "arm", "movement bypassed one-command arm authorization")) return false;
        receive("ARMED for one enable or motion command");
        static const QStringList expected{"vision jog 20 0 15", "vision jog -20 0 15",
                                           "vision jog 0 20 15", "vision jog 0 -20 15"};
        if (!require(last() == expected[index], "probe did not move and return at 15 RPM")) return false;
        receive("VISION PAUSED");
        events();
        if (!require(last() == expected[index], "stale move completion advanced calibration")) return false;
        receive("VISION STATE state=MOVE mode=MANUAL selector=0 target=0 iteration=0");
        receive("VISION PAUSED");
        events(150);
        if (!require(last() == expected[index], "camera was sampled before settling")) return false;
        return require(waitForSample(), "next sample was not requested after settling");
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    {
        Fixture f;
        if (!require(f.runner.start(), "idle calibration did not start")) return 1;
        const QString initial = f.last();
        if (!require(initial.startsWith("vision sample ring 2 "), "calibration did not start read-only")) return 1;
        f.sample({0, 0}, 10);
        events();
        if (!require(f.last() == initial, "stale sample triggered motion")) return 1;
        if (!require(!f.client.startFullRoute(1), "competing route bypassed calibration reservation")) return 1;
        f.sample({0, 0});
        if (!f.move(0)) return 1;
        f.sample({40, 1}); // 20 mm / 40 px = 0.500 mm/px.
        if (!f.move(1)) return 1;
        f.sample({0, 0});
        if (!f.move(2)) return 1;
        f.sample({1, 20}); // 20 mm / 20 px = 1.000 mm/px.
        if (!f.move(3)) return 1;
        f.sample({0, 0});
        events();
        if (!require(f.forward == 500 && f.right == 1000 && f.last() == "arm",
                     "two-axis calibration calculation or scale authorization was incorrect")) return 1;
        f.receive("ARMED for one enable or motion command");
        if (!require(f.last() == "vision scale ring 2 500 1000", "computed scale was not applied")) return 1;
        f.receive("VISION SCALE ring=2 forward_milli=640 right_milli=673");
        if (!require(f.runner.running() && f.completed == 0, "old scale acknowledgement finished calibration")) return 1;
        f.receive("VISION SCALE ring=2 forward_milli=500 right_milli=1000");
        if (!require(!f.runner.running() && f.completed == 1 && f.error.isEmpty() &&
                     f.client.beginRouteExecution(), "confirmed calibration did not release its reservation")) return 1;
        f.client.endRouteExecution();
    }
    {
        Fixture f;
        f.color = 4;
        if (!require(f.runner.startMaterial(4), "material calibration failed to start")) return 1;
        const QString initial = f.last();
        f.receive("VISION MEASURE id=1 material=2 token=1 du=0 dv=0 quality=0");
        events();
        if (!require(f.last() == initial, "wrong color sample triggered movement")) return 1;
        const std::array<QPointF, 5> points{{{0,0},{40,1},{0,0},{1,20},{0,0}}};
        for (int i = 0; i < 5; ++i) {
            if (!require(f.last().startsWith("vision sample material 4 "), "probe changed material color")) return 1;
            f.sample(points[i], 0, 0);
            if (i < 4 && !f.move(i)) return 1;
        }
        events();
        f.receive("ARMED for one enable or motion command");
        if (!require(f.last() == "vision scale material 500 1000", "material scale not applied independently")) return 1;
        f.receive("VISION SCALE ring=2 forward_milli=500 right_milli=1000");
        f.receive("VISION SCALE material forward_milli=640 right_milli=673");
        if (!require(f.runner.running(), "wrong scale reply completed material calibration")) return 1;
        f.receive("VISION SCALE material forward_milli=500 right_milli=1000");
        if (!require(f.completed == 1 && !f.runner.running(), "material acknowledgement did not complete")) return 1;
    }
    // Replay the second hardware run: one-pixel return error must not inflate the coupling estimate.
    {
        const std::array<QPointF, 5> points{{{-35,22},{-11,21},{-35,22},{-31,45},{-36,23}}};
        Fixture f;
        if (!f.runner.start()) return 1;
        for (int i = 0; i < 5; ++i) {
            f.sample(points[i]);
            if (i < 4 && !f.move(i)) return 1;
        }
        events();
        if (!require(f.runner.running() && f.forward == 833 && f.right == 889 && f.last() == "arm",
                     "hardware samples were rejected due to single-pixel return error")) return 1;
        f.receive("ARMED for one enable or motion command");
        if (!require(f.last() == "vision scale ring 2 833 889", "hardware scales were not applied")) return 1;
        f.receive("VISION SCALE ring=2 forward_milli=833 right_milli=889");
        if (!require(f.completed == 1 && !f.runner.running(), "hardware calibration confirmation failed")) return 1;
    }
    // Independent failures, including the first hardware run's seven-pixel return drift.
    const std::array<std::array<QPointF, 5>, 5> invalid{{
        {{{0,0},{2,0},{0,0},{0,20},{0,0}}},
        {{{0,0},{-40,0},{0,0},{0,20},{0,0}}},
        {{{0,0},{40,0},{5,0},{5,20},{5,0}}},
        {{{0,0},{40,12},{0,0},{0,20},{0,0}}},
        {{{-28,21},{-4,20},{-28,21},{-29,44},{-35,22}}}
    }};
    for (const auto &points : invalid) {
        Fixture f;
        if (!f.runner.start()) return 1;
        for (int i = 0; i < 5; ++i) {
            f.sample(points[i]);
            if (i < 4 && !f.move(i)) return 1;
        }
        events();
        if (!require(!f.runner.running() && !f.error.isEmpty() && f.last() == "stop" && f.forward == 0,
                     "invalid calibration applied a scale or did not stop")) return 1;
    }
    {
        Fixture f;
        f.runner.start();
        f.sample({0,0}, 0, 59);
        if (!require(!f.runner.running() && f.last() == "stop", "poor-quality measurement triggered movement")) return 1;
    }
    {
        Fixture f;
        f.runner.start();
        f.sample({0,0});
        f.runner.stop(); // Cancel before the deferred first movement.
        events();
        if (!require(!f.runner.running() && f.last() == "stop", "cancelled calibration still dispatched movement")) return 1;
        if (!require(f.runner.start(), "stopped calibration could not restart")) return 1;
        f.client.setConnected(false);
        const int count = f.commands.size();
        events(350);
        if (!require(!f.runner.running() && f.commands.size() == count && f.completed == 0,
                     "disconnect left calibration active or dispatched commands")) return 1;
    }
    {
        Fixture f;
        f.runner.start();
        f.receive("ERR: unknown command");
        if (!require(!f.runner.running() && f.last() == "stop", "old firmware did not fail safely")) return 1;
    }
    {
        Fixture f;
        f.runner.start();
        QElapsedTimer timer;
        timer.start();
        while (f.runner.running() && timer.elapsed() < 7000) events(10);
        if (!require(!f.runner.running() && f.last() == "stop", "missing camera result did not time out")) return 1;
    }
    std::cout << "PASS: calibration, authorization, stale replies, settling, scale confirmation and failure cancellation\n";
    return 0;
}
