#include <QCoreApplication>
#include <QEventLoop>
#include <QPointF>
#include <QTimer>

#include <iostream>

#include "device/MecanumJogClient.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

void waitFor(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

}  // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    MecanumJogClient client;
    QList<QByteArray> transmitted;
    QStringList lines;
    QStringList failures;
    QList<QString> navigationStates;
    QList<QPointF> navigationPositions;
    QList<bool> navigationValidity;
    QList<QPointF> navigationCompletions;
    QObject::connect(&client, &MecanumJogClient::bytesReady,
                     [&](QByteArray bytes) { transmitted.push_back(bytes); });
    QObject::connect(&client, &MecanumJogClient::lineReceived,
                     [&](QString line) { lines.push_back(line); });
    QObject::connect(&client, &MecanumJogClient::commandFailed,
                     [&](QString reason) { failures.push_back(reason); });
    QObject::connect(
        &client, &MecanumJogClient::navigationEstimateReceived,
        [&](qint32 xMm, qint32 yMm, double yawDegrees, QString state) {
            navigationPositions.push_back(QPointF(xMm, yMm));
            navigationStates.push_back(
                QStringLiteral("%1:%2").arg(state).arg(yawDegrees));
        });
    QObject::connect(&client, &MecanumJogClient::navigationValidityChanged,
                     [&](bool valid) { navigationValidity.push_back(valid); });
    QObject::connect(&client, &MecanumJogClient::navigationCompleted,
                     [&](qint32 xMm, qint32 yMm) {
                         navigationCompletions.push_back(QPointF(xMm, yMm));
                     });

    client.setConnected(true);
    transmitted.clear();
    if (!require(client.initializeNavigation(1) &&
                     transmitted == QList<QByteArray>({QByteArray("nav init 1\r\n")}),
                 "navigation initialization command is incorrect")) {
        return 1;
    }
    client.ingestBytes(QByteArrayView(
        "NAV INIT x=2250 y=2250 yaw_cdeg=9000\r\n"
        "NAV POS x=2100 y=2200 yaw_cdeg=8950 state=RUN target_x=1200 target_y=2080\r\n"
        "NAV DONE x=1200 y=2080 yaw_cdeg=9000\r\n"));
    if (!require(navigationValidity == QList<bool>({true}) &&
                     navigationPositions ==
                         QList<QPointF>({QPointF(2250, 2250), QPointF(2100, 2200),
                                        QPointF(1200, 2080)}) &&
                     navigationStates.at(0).startsWith(QStringLiteral("IDLE:90")) &&
                     navigationStates.at(1).startsWith(QStringLiteral("RUN:89.5")) &&
                     navigationCompletions == QList<QPointF>({QPointF(1200, 2080)}),
                 "structured navigation replies were not decoded")) {
        return 1;
    }
    transmitted.clear();
    if (!require(client.navigateTo(400, 1200) &&
                     transmitted == QList<QByteArray>({QByteArray("arm\r\n")}),
                 "navigation target did not start the arm handshake")) {
        return 1;
    }
    client.ingestBytes(QByteArrayView(
        "ARMED for one enable or motion command\r\n"));
    if (!require(transmitted == QList<QByteArray>({QByteArray("arm\r\n"),
                                                   QByteArray("nav goto 400 1200\r\n")}),
                 "navigation target command is incorrect")) {
        return 1;
    }
    client.ingestBytes(QByteArrayView("NAV INVALID reason=stopped\r\n"));
    if (!require(navigationValidity.back() == false,
                 "navigation invalidation was not reported")) {
        return 1;
    }
    lines.clear();
    transmitted.clear();
    if (!require(client.emergencyStop() &&
                     transmitted == QList<QByteArray>({QByteArray("!")}),
                 "emergency stop was not emitted as one raw byte")) {
        return 1;
    }
    transmitted.clear();
    client.ingestBytes(QByteArrayView("OK first\r"));
    client.ingestBytes(QByteArrayView("\nERR second\r\n"));
    if (!require(lines == QStringList({QStringLiteral("OK first"),
                                       QStringLiteral("ERR second")}),
                 "fragmented and consecutive lines were not decoded")) {
        return 1;
    }
    client.ingestBytes(QByteArrayView("CR only\r"));
    if (!require(lines.back() == QStringLiteral("CR only"),
                 "CR-only line ending was not decoded")) {
        return 1;
    }

    transmitted.clear();
    if (!require(client.sendArmedCommand(QStringLiteral("W")) &&
                     transmitted == QList<QByteArray>({QByteArray("arm\r\n")}),
                 "armed command did not begin with arm")) {
        return 1;
    }
    if (!require(!client.sendArmedCommand(QStringLiteral("S")),
                 "parallel armed command was accepted")) {
        return 1;
    }
    client.ingestBytes(QByteArrayView("status noise\r\nARMED for one enable "));
    client.ingestBytes(QByteArrayView("or motion command\r\n"));
    if (!require(transmitted == QList<QByteArray>({QByteArray("arm\r\n"),
                                                   QByteArray("W\r\n")}),
                 "target command was not gated by the exact ARMED reply")) {
        return 1;
    }

    failures.clear();
    client.ingestBytes(QByteArrayView(
        "ERR: fresh IMU yaw required; check baud/wiring/status\r\n"));
    if (!require(failures.size() == 1,
                 "post-dispatch firmware error was ignored")) {
        return 1;
    }
    client.sendCommand(QStringLiteral("servo 2 90"));
    client.ingestBytes(QByteArrayView("ERR: test busy; use stop or ! first\r\n"));
    if (!require(failures.size() == 2,
                 "unarmed command error was ignored")) {
        return 1;
    }
    QString latestState;
    QObject::connect(&client, &MecanumJogClient::commandStateChanged,
                     [&](QString state) { latestState = state; });
    client.ingestBytes(QByteArrayView("STOPPED and disarmed\r\n"));
    if (!require(latestState.contains(QStringLiteral("设备已停止")),
                 "device stop did not update command state")) {
        return 1;
    }

    transmitted.clear();
    if (!require(!client.sendCommand(QStringLiteral("arm\r\nW")) &&
                     !client.sendCommand(QString(24, QLatin1Char('a'))) &&
                     !client.sendCommand(QStringLiteral("中文")) &&
                     transmitted.isEmpty(),
                 "invalid or batched text commands reached the UART")) {
        return 1;
    }
    client.sendArmedCommand(QStringLiteral("W"));
    if (!require(!client.sendCommand(QStringLiteral("arm")) &&
                     transmitted.size() == 1,
                 "terminal command interfered with pending authorization")) {
        return 1;
    }
    client.sendCommand(QStringLiteral("stop"));
    client.ingestBytes(
        QByteArrayView("ARMED for one enable or motion command\r\n"));
    if (!require(transmitted.contains(QByteArray("stop\r\n")) &&
                     !transmitted.contains(QByteArray("W\r\n")),
                 "late ARMED reply restarted an action after stop")) {
        return 1;
    }

    for (const QString &stop : {QStringLiteral("!\r\n"), QStringLiteral("X"),
                               QStringLiteral("disable"), QStringLiteral("disable5")}) {
        transmitted.clear();
        client.sendArmedCommand(QStringLiteral("W"));
        client.sendCommand(stop);
        client.ingestBytes(QByteArrayView("ARMED for one enable or motion command\r\n"));
        if (!require(transmitted.size() == 2 &&
                         !transmitted.contains(QByteArray("W\r\n")),
                     "terminal stop did not cancel pending authorization")) {
            return 1;
        }
    }

    transmitted.clear();
    failures.clear();
    client.sendArmedCommand(QStringLiteral("line W 100 30"));
    client.ingestBytes(QByteArrayView("ERR: send 'arm' first\r\n"));
    if (!require(transmitted.size() == 1 && !failures.isEmpty() &&
                     failures.back().contains(QStringLiteral("ERR")),
                 "ERR reply did not cancel the armed command")) {
        return 1;
    }

    transmitted.clear();
    failures.clear();
    client.sendArmedCommand(QStringLiteral("D"));
    client.setConnected(false);
    if (!require(!failures.isEmpty() && transmitted.size() == 1,
                 "disconnect did not cancel the pending armed command")) {
        return 1;
    }

    transmitted.clear();
    client.setConnected(true);
    waitFor(300);
    if (!require(transmitted.contains(QByteArray("hb\r\n")),
                 "connected text client did not emit a heartbeat")) {
        return 1;
    }
    client.setConnected(false);
    transmitted.clear();
    waitFor(300);
    if (!require(transmitted.isEmpty(),
                 "disconnected text client continued heartbeats")) {
        return 1;
    }

    client.setConnected(true);
    transmitted.clear();
    failures.clear();
    client.sendArmedCommand(QStringLiteral("A"));
    waitFor(2100);
    if (!require(transmitted.contains(QByteArray("arm\r\n")) &&
                     !transmitted.contains(QByteArray("A\r\n")) &&
                     !failures.isEmpty() &&
                     failures.back().contains(QStringLiteral("超时")),
                 "arm timeout did not reject the target command")) {
        return 1;
    }

    return 0;
}
