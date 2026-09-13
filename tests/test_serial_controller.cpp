#include <QByteArrayView>
#include <QCoreApplication>

#include <iostream>

#include "protocol/ProtocolClient.h"
#include "serial/SerialController.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    ProtocolClient protocol;
    SerialController serial(&protocol);

    int errorCount = 0;
    QString latestError;
    QObject::connect(&serial, &SerialController::serialError,
                     [&](const QString &message) {
                         ++errorCount;
                         latestError = message;
                     });

    const bool opened = serial.open(QStringLiteral("COM_DOES_NOT_EXIST_6"),
                                    115200);
    if (!require(!opened, "opening a nonexistent port unexpectedly succeeded") ||
        !require(errorCount == 1,
                 "opening a nonexistent port did not report exactly one error") ||
        !require(!latestError.isEmpty(), "serial error did not include a message") ||
        !require(serial.write(QByteArrayView("x", 1)) == -1,
                 "a failed open left the serial transport writable")) {
        return 1;
    }

    int disconnectFailureCount = 0;
    QObject::connect(&protocol, &ProtocolClient::requestFailed,
                     [&](quint8, const QString &) { ++disconnectFailureCount; });
    protocol.sendRequest(protocol::Command::GetStatus, {});
    serial.close();
    if (!require(disconnectFailureCount == 1,
                 "closing the controller did not clear protocol requests")) {
        return 1;
    }

    errorCount = 0;
    serial.setProtocolEnabled(false);
    protocol.sendRequest(protocol::Command::GetStatus, {});
    if (!require(errorCount == 0,
                 "disabled binary protocol still reached the serial port")) {
        return 1;
    }
    serial.close();

    return 0;
}
