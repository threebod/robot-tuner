#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

#include "device/MecanumJogClient.h"
#include "device/RouteSequenceRunner.h"
#include "pages/RouteEditorPage.h"

#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; return 1; } } while (0)

static void pump(int milliseconds = 1) {
    QEventLoop loop; QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit); loop.exec();
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (argc == 3) {
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    }
    QTemporaryDir temp;
    CHECK(temp.isValid());
    RouteSequence route;
    RouteStep move; move.distanceMm = 980; move.direction = "A";
    RouteStep turn; turn.type = RouteStepType::Turn; turn.direction = "L";
    RouteStep vision; vision.type = RouteStepType::Vision; vision.target = 4;
    RouteStep action; action.type = RouteStepType::Action; action.action.name = QStringLiteral("取料准备");
    action.action.initial.pose.turretDdeg = 650;
    MechanismStep pose; pose.type = MechanismStepType::Pose;
    pose.pose.horizontalDmm = -500; pose.pose.turretDdeg = 2700;
    MechanismStep servo; servo.type = MechanismStepType::Servo; servo.channel = 3; servo.value = 146;
    MechanismStep pause; pause.type = MechanismStepType::Wait; pause.waitMs = 0;
    MechanismStep turret; turret.type = MechanismStepType::Servo; turret.channel = 4; turret.value = 180;
    action.action.steps = {pose, turret, servo, pause};
    RouteStep confirm; confirm.type = RouteStepType::Confirm;
    RouteStep wait; wait.type = RouteStepType::Wait; wait.waitMs = 0;
    route.steps = {move, turn, vision, action, confirm, wait};
    QString error;
    const QString path = temp.filePath("route.json");
    CHECK(saveRouteSequence(path, route, &error));
    RouteSequence loaded;
    CHECK(loadRouteSequence(path, &loaded, &error));
    CHECK(loaded.steps.size() == 6 && loaded.steps[0].distanceMm == 980);
    CHECK(loaded.steps[3].action.steps[0].pose.horizontalDmm == -500);
    CHECK(loaded.steps[3].action.initial.pose.turretDdeg == 650);
    QFile file(path); CHECK(file.open(QIODevice::ReadOnly));
    const auto validJson = QJsonDocument::fromJson(file.readAll()).object(); file.close();
    for (int failure = 0; failure < 5; ++failure) {
        auto bad = validJson;
        auto steps = bad["steps"].toArray(); auto first = steps[0].toObject();
        if (failure == 0) bad["version"] = 2;
        if (failure == 1) first["type"] = "unsupported";
        if (failure == 2) first["distanceMm"] = 1.5;
        if (failure == 3) first["direction"] = "L";
        if (failure == 4) first["rpm"] = 251;
        steps[0] = first; bad["steps"] = steps;
        CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write(QJsonDocument(bad).toJson()); file.close();
        loaded.name = "preserved";
        CHECK(!loadRouteSequence(path, &loaded, &error) && loaded.name == "preserved");
    }
    CHECK(saveRouteSequence(path, route, &error));
    auto invalid = route; invalid.steps[3].action.steps[0].pose.liftDmm = 1501;
    CHECK(!validateRouteSequence(invalid, &error));
    invalid = route; invalid.steps[2].ring = true; invalid.steps[2].target = 4;
    CHECK(!validateRouteSequence(invalid, &error));

    MecanumJogClient client;
    RouteSequenceRunner runner(&client);
    QList<QByteArray> sent;
    QObject::connect(&client, &MecanumJogClient::bytesReady, [&](QByteArray bytes) { if (bytes != "hb\r\n") sent.append(bytes); });
    auto feed = [&](const QString &line) { const auto bytes = (line + "\r\n").toUtf8(); client.ingestBytes(bytes); };
    auto arm = [&] { feed("ARMED for one enable or motion command"); };
    int completions = 0, failures = 0;
    QObject::connect(&runner, &RouteSequenceRunner::completed, [&] { ++completions; });
    QObject::connect(&runner, &RouteSequenceRunner::failed, [&](QString) { ++failures; });
    CHECK(!runner.start(route) && !runner.running());
    client.setConnected(true);
    CHECK(runner.start(route)); CHECK(sent.last() == "arm\r\n");
    CHECK(!client.sendArmedCommand("turn R 90") && runner.running());
    arm(); CHECK(sent.last() == "move A 980 100\r\n");
    feed("MOVE DONE dir=A distance_mm=980"); pump(); CHECK(runner.currentIndex() == 0);
    feed("MOVE RUN dir=A distance_mm=980 rpm=100");
    feed("MOVE DONE dir=D distance_mm=980"); pump(); CHECK(runner.currentIndex() == 0);
    feed("MOVE DONE dir=A distance_mm=980"); feed("MOVE DONE dir=A distance_mm=980"); pump();
    CHECK(runner.currentIndex() == 1); arm(); CHECK(sent.last() == "turn L 90\r\n");
    feed("TURN running"); feed("TURN DONE (2deg entry / 3.5deg hysteresis)"); pump();
    arm(); CHECK(sent.last() == "vision scale material 640 673\r\n");
    feed("VISION SCALE material forward_milli=641 right_milli=673"); pump(); CHECK(runner.currentIndex() == 2);
    feed("VISION SCALE material forward_milli=640 right_milli=673"); arm();
    CHECK(sent.last() == "vision align material 4\r\n");
    feed("VISION STATE state=REQUEST mode=MATERIAL selector=4 target=0 iteration=0");
    feed("VISION DONE forward_mm=45 right_mm=-30 iterations=2"); pump();
    CHECK(sent.last() == "mech status\r\n");
    feed("MECH POS h=0 l=0 t=650");
    CHECK(sent.last() == "mech status\r\n");
    feed("MECH SERVOS gripper_mdeg=90000 platform_mdeg=26000 turret_mdeg=65000 moving=0"); pump(); arm();
    CHECK(sent.last() == "mech pose -500 0 2700 30 50 30 50 1200\r\n");
    feed("MECH RUN h=-500 l=0 t=2700"); feed("MECH DONE h=0 l=0 t=2700"); pump();
    CHECK(runner.currentIndex() == 3);
    feed("MECH DONE h=-500 l=0 t=2700"); pump();
    arm(); CHECK(sent.last() == "mech pose -500 0 1800 30 50 30 50 1200\r\n");
    feed("MECH RUN h=-500 l=0 t=1800"); feed("MECH DONE h=-500 l=0 t=1800"); pump();
    CHECK(sent.last() == "servo 3 146 1200\r\n");
    feed("DONE servo=2 angle=146"); pump(); CHECK(sent.last() == "servo 3 146 1200\r\n");
    feed("MOVING servo=3 target=146 speed_dps10=1200"); feed("DONE servo=3 angle=146"); pump(10);
    CHECK(runner.awaitingConfirmation() && runner.currentIndex() == 4);
    runner.confirm(); pump(10); CHECK(!runner.running() && completions == 1);
    CHECK(!sent.contains("vision jog -20 20 15\r\n"));

    // Ring scale is confirmed before alignment, and its result starts a new relative move.
    RouteSequence aligned; vision.ring = true; vision.target = 2; move.direction = "W"; move.distanceMm = 500;
    aligned.steps = {vision, move};
    CHECK(runner.start(aligned)); arm();
    feed("VISION SCALE ring=2 forward_milli=640 right_milli=673"); arm();
    CHECK(sent.last() == "vision align ring 2\r\n");
    feed("VISION STATE state=REQUEST mode=RING selector=0 target=2 iteration=0");
    feed("VISION DONE forward_mm=-10 right_mm=20 iterations=1"); pump(); arm();
    CHECK(sent.last() == "move W 500 100\r\n");
    feed("MOVE RUN dir=W distance_mm=500 rpm=100"); feed("MOVE DONE dir=W distance_mm=500"); pump();
    CHECK(!runner.running());

    // All cancellation paths discard deferred work and pending arm authorization.
    CHECK(runner.start(route)); runner.stop(); arm(); pump(); CHECK(!runner.running() && sent.last() == "stop\r\n");
    CHECK(runner.start(route)); arm(); feed("ERR MOVE: unsupported"); pump(); CHECK(!runner.running());
    CHECK(runner.start(route)); client.setConnected(false); pump(); CHECK(!runner.running());
    client.setConnected(true);
    CHECK(runner.start(route, 3, true)); feed("MECH POS h=0 l=0 t=650");
    feed("MECH SERVOS gripper_mdeg=35000 platform_mdeg=26000 turret_mdeg=65000 moving=0"); pump();
    CHECK(!runner.running() && sent.last() == "stop\r\n");
    CHECK(runner.start(route, 3, true)); feed("MECH POS h=0 l=0 t=650");
    feed("MECH SERVOS gripper_mdeg=90000 platform_mdeg=26000 turret_mdeg=270000 moving=0"); pump();
    CHECK(!runner.running() && sent.last() == "stop\r\n");
    CHECK(runner.start(route, 3, true)); feed("MECH POS h=1 l=0 t=650"); pump(); CHECK(!runner.running());
    CHECK(failures >= 6);
    CHECK(runner.start(route, 4, true)); client.setConnected(false); pump();
    CHECK(!runner.running()); client.setConnected(true);
    CHECK(runner.start(route)); pump(2100); CHECK(!runner.running() && sent.last() == "stop\r\n");

    RouteSequence timerRoute; wait.waitMs = 20; timerRoute.steps = {wait, move};
    CHECK(runner.start(timerRoute)); runner.stop(); const auto count = sent.size(); pump(30);
    CHECK(!runner.running() && sent.size() == count);
    RouteSequence actionLoop; action.action.loopEnabled = true; action.action.loopCount = 2;
    action.action.steps = {pause}; actionLoop.steps = {action};
    CHECK(runner.start(actionLoop)); feed("MECH POS h=0 l=0 t=650");
    feed("MECH SERVOS gripper_mdeg=90000 platform_mdeg=26000 turret_mdeg=65000 moving=0"); pump(20); CHECK(!runner.running());

    RouteEditorPage page(&runner); page.resize(1100, 850); page.show();
    auto *visionMode = page.findChild<QComboBox *>("routeVisionMode");
    auto *visionTarget = page.findChild<QSpinBox *>("routeVisionTarget");
    auto *forwardScale = page.findChild<QSpinBox *>("routeForwardScale");
    auto *rightScale = page.findChild<QSpinBox *>("routeRightScale");
    CHECK(visionMode && visionTarget && forwardScale && rightScale);
    visionMode->setCurrentIndex(1); visionTarget->setValue(2);
    CHECK(forwardScale->value() == 1111 && rightScale->value() == 1143);
    visionMode->setCurrentIndex(0);
    CHECK(forwardScale->value() == 640 && rightScale->value() == 673);
    CHECK(page.loadFile(path, &error)); CHECK(page.sequence().steps.size() == 6);
    CHECK(page.findChild<QPushButton *>("routeAdd")->isEnabled());
    CHECK(!page.findChild<QPushButton *>("routeRunAll")->isEnabled());
    auto *table = page.findChild<QTableWidget *>("routeSteps");
    table->selectRow(0); page.findChild<QSpinBox *>("routeDistance")->setValue(1000);
    page.findChild<QPushButton *>("routeReplace")->click(); CHECK(page.sequence().steps[0].distanceMm == 1000);
    page.findChild<QPushButton *>("routeCopy")->click(); CHECK(page.sequence().steps.size() == 7);
    page.findChild<QPushButton *>("routeDown")->click(); CHECK(page.sequence().steps[1].type == RouteStepType::Turn);
    page.findChild<QPushButton *>("routeUp")->click(); page.findChild<QPushButton *>("routeDelete")->click();
    CHECK(page.sequence().steps.size() == 6);
    CHECK(page.saveFile(temp.filePath("edited.json"), &error));
    CHECK(!page.loadFile(temp.filePath("missing.json"), &error) && page.sequence().steps.size() == 6);
    page.setConnected(true); CHECK(page.findChild<QPushButton *>("routeRunAll")->isEnabled());
    CHECK(runner.start(route, 4, true)); CHECK(!page.findChild<QPushButton *>("routeAdd")->isEnabled());
    CHECK(page.findChild<QPushButton *>("routeConfirm")->isEnabled());
    page.findChild<QPushButton *>("routeConfirm")->click(); pump(); CHECK(!runner.running());
    CHECK(page.findChild<QPushButton *>("routeAdd")->isEnabled());
    page.setSequence(route); table->selectRow(2); pump();
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--screenshot") CHECK(page.grab().save(QString::fromLocal8Bit(argv[2])));
    std::cout << "PASS: route JSON, matched execution feedback, alignment baseline, embedded actions, cancellation and editing\n";
    return 0;
}
