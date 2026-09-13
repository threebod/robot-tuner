#include "device/MecanumJogClient.h"

#include <utility>

namespace {

const QString kArmedReply =
    QStringLiteral("ARMED for one enable or motion command");

}  // namespace

MecanumJogClient::MecanumJogClient(QObject *parent) : QObject(parent) {
    heartbeatTimer_.setInterval(250);
    heartbeatTimer_.setTimerType(Qt::CoarseTimer);
    connect(&heartbeatTimer_, &QTimer::timeout, this,
            [this] { emit bytesReady(QByteArray("hb\r\n")); });

    armTimer_.setSingleShot(true);
    armTimer_.setInterval(2000);
    armTimer_.setTimerType(Qt::PreciseTimer);
    connect(&armTimer_, &QTimer::timeout, this, [this] {
        failPending(QStringLiteral("等待 arm 回复超时"));
    });
}

void MecanumJogClient::setConnected(bool connected) {
    if (connected_ == connected) {
        return;
    }
    connected_ = connected;
    receiveBuffer_.clear();
    if (connected_) {
        heartbeatTimer_.start();
        emit commandStateChanged(QStringLiteral("文本串口已连接"));
        return;
    }

    heartbeatTimer_.stop();
    if (!pendingCommand_.isEmpty()) {
        failPending(QStringLiteral("连接已断开，授权命令已取消"));
    } else {
        armTimer_.stop();
        emit commandStateChanged(QStringLiteral("未连接"));
    }
}

void MecanumJogClient::ingestBytes(QByteArrayView bytes) {
    if (bytes.isEmpty()) {
        return;
    }
    receiveBuffer_.append(bytes.data(), bytes.size());
    if (receiveBuffer_.size() > kMaximumReceiveBuffer) {
        receiveBuffer_.remove(0, receiveBuffer_.size() - kMaximumReceiveBuffer);
    }

    int carriageReturn = receiveBuffer_.indexOf('\r');
    int lineFeed = receiveBuffer_.indexOf('\n');
    int newline = carriageReturn < 0 ? lineFeed
                                     : lineFeed < 0 ? carriageReturn
                                                    : qMin(carriageReturn, lineFeed);
    while (newline >= 0) {
        const QByteArray rawLine = receiveBuffer_.left(newline);
        receiveBuffer_.remove(0, newline + 1);
        while (receiveBuffer_.startsWith('\r') ||
               receiveBuffer_.startsWith('\n')) {
            receiveBuffer_.remove(0, 1);
        }
        const QString line = QString::fromUtf8(rawLine);
        if (!line.isEmpty()) {
            handleLine(line);
        }
        carriageReturn = receiveBuffer_.indexOf('\r');
        lineFeed = receiveBuffer_.indexOf('\n');
        newline = carriageReturn < 0 ? lineFeed
                                     : lineFeed < 0 ? carriageReturn
                                                    : qMin(carriageReturn,
                                                           lineFeed);
    }
}

bool MecanumJogClient::sendCommand(QString command) {
    command = command.trimmed();
    if (!connected_) {
        emit commandFailed(QStringLiteral("串口未连接"));
        return false;
    }
    if (!validCommand(command)) {
        emit commandFailed(QStringLiteral("命令不能为空且不能包含换行"));
        return false;
    }
    if (!pendingCommand_.isEmpty() &&
        (command == QStringLiteral("stop") || command == QStringLiteral("X") ||
         command == QStringLiteral("x") ||
         command == QStringLiteral("disable") ||
         command == QStringLiteral("disable5"))) {
        armTimer_.stop();
        pendingCommand_.clear();
    }
    emit bytesReady(command.toUtf8() + QByteArray("\r\n"));
    emit commandStateChanged(QStringLiteral("已发送：%1").arg(command));
    return true;
}

bool MecanumJogClient::sendArmedCommand(QString command) {
    command = command.trimmed();
    if (!connected_) {
        emit commandFailed(QStringLiteral("串口未连接"));
        return false;
    }
    if (!validCommand(command)) {
        emit commandFailed(QStringLiteral("命令不能为空且不能包含换行"));
        return false;
    }
    if (!pendingCommand_.isEmpty()) {
        emit commandFailed(QStringLiteral("已有命令正在等待授权"));
        return false;
    }

    pendingCommand_ = std::move(command);
    emit bytesReady(QByteArray("arm\r\n"));
    emit commandStateChanged(QStringLiteral("等待单次授权：%1")
                                 .arg(pendingCommand_));
    armTimer_.start();
    return true;
}

bool MecanumJogClient::emergencyStop() {
    if (!connected_) {
        emit commandFailed(QStringLiteral("串口未连接"));
        return false;
    }
    if (!pendingCommand_.isEmpty()) {
        armTimer_.stop();
        pendingCommand_.clear();
    }
    emit bytesReady(QByteArray("!"));
    emit commandStateChanged(QStringLiteral("已发送紧急停止"));
    return true;
}

bool MecanumJogClient::validCommand(const QString &command) const {
    return !command.isEmpty() && !command.contains('\r') &&
           !command.contains('\n');
}

void MecanumJogClient::handleLine(const QString &line) {
    emit lineReceived(line);
    if (pendingCommand_.isEmpty()) {
        return;
    }
    if (line.startsWith(QStringLiteral("ERR"))) {
        failPending(line);
        return;
    }
    if (line != kArmedReply) {
        return;
    }

    armTimer_.stop();
    const QString command = pendingCommand_;
    pendingCommand_.clear();
    emit bytesReady(command.toUtf8() + QByteArray("\r\n"));
    emit commandStateChanged(QStringLiteral("已授权并发送：%1").arg(command));
}

void MecanumJogClient::failPending(const QString &reason) {
    armTimer_.stop();
    pendingCommand_.clear();
    emit commandFailed(reason);
    emit commandStateChanged(QStringLiteral("命令失败：%1").arg(reason));
}
