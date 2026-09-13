#include <QCoreApplication>
#include <QEventLoop>
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
    QObject::connect(&client, &MecanumJogClient::bytesReady,
                     [&](QByteArray bytes) { transmitted.push_back(bytes); });
    QObject::connect(&client, &MecanumJogClient::lineReceived,
                     [&](QString line) { lines.push_back(line); });
    QObject::connect(&client, &MecanumJogClient::commandFailed,
                     [&](QString reason) { failures.push_back(reason); });

    client.setConnected(true);
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

    transmitted.clear();
    client.sendArmedCommand(QStringLiteral("W"));
    client.sendCommand(QStringLiteral("stop"));
    client.ingestBytes(
        QByteArrayView("ARMED for one enable or motion command\r\n"));
    if (!require(transmitted.contains(QByteArray("stop\r\n")) &&
                     !transmitted.contains(QByteArray("W\r\n")),
                 "late ARMED reply restarted an action after stop")) {
        return 1;
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
