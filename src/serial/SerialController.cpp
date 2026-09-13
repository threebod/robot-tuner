#include "serial/SerialController.h"

#include "protocol/ProtocolClient.h"

#include <QIODevice>
#include <QSerialPortInfo>

SerialController::SerialController(ProtocolClient *protocol, QObject *parent)
    : QObject(parent), serialPort_(this), protocol_(protocol) {
    connect(&serialPort_, &QSerialPort::readyRead, this,
            &SerialController::handleReadyRead);
    connect(&serialPort_, &QSerialPort::errorOccurred, this,
            &SerialController::handleSerialError);
    if (protocol_ != nullptr) {
        connect(protocol_, &ProtocolClient::bytesReady, this,
                &SerialController::handleProtocolBytes);
    }
}

SerialController::SerialController(ProtocolClient &protocol, QObject *parent)
    : SerialController(&protocol, parent) {}

QStringList SerialController::availablePorts() const {
    QStringList ports;
    const QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    ports.reserve(infos.size());
    for (const QSerialPortInfo &info : infos) {
        ports.push_back(info.portName());
    }
    return ports;
}

bool SerialController::open(QString portName, qint32 baudRate) {
    portName = portName.trimmed();
    if (serialPort_.isOpen()) {
        if (serialPort_.portName() == portName &&
            serialPort_.baudRate() == baudRate) {
            return true;
        }
        reportError(QStringLiteral("已有串口连接，请先断开"));
        return false;
    }
    if (portName.isEmpty()) {
        reportError(QStringLiteral("未选择串口"));
        return false;
    }
    if (baudRate <= 0) {
        reportError(QStringLiteral("波特率无效"));
        return false;
    }

    openErrorReported_ = false;
    opening_ = true;
    serialPort_.clearError();
    serialPort_.setPortName(portName);

    const bool configured = serialPort_.setBaudRate(baudRate) &&
                            serialPort_.setDataBits(QSerialPort::Data8) &&
                            serialPort_.setParity(QSerialPort::NoParity) &&
                            serialPort_.setStopBits(QSerialPort::OneStop) &&
                            serialPort_.setFlowControl(QSerialPort::NoFlowControl);
    const bool didOpen = configured && serialPort_.open(QIODevice::ReadWrite);
    opening_ = false;

    if (!didOpen) {
        if (!openErrorReported_) {
            const QString detail = serialPort_.errorString();
            reportError(detail.isEmpty()
                            ? QStringLiteral("串口打开失败")
                            : QStringLiteral("串口打开失败：%1").arg(detail));
        }
        if (serialPort_.isOpen()) {
            serialPort_.close();
        }
        return false;
    }

    connected_ = true;
    emit opened();
    return true;
}

void SerialController::close() {
    if (!connected_ && !serialPort_.isOpen()) {
        if (protocol_ != nullptr) {
            protocol_->clearPending();
        }
        return;
    }
    clearProtocolAndEmitClosed();
}

qint64 SerialController::write(QByteArrayView bytes) {
    if (!serialPort_.isOpen()) {
        reportError(QStringLiteral("串口未连接"));
        return -1;
    }
    if (bytes.isEmpty()) {
        return 0;
    }
    return serialPort_.write(bytes.data(), bytes.size());
}

void SerialController::setProtocolEnabled(bool enabled) {
    protocolEnabled_ = enabled;
    if (!protocolEnabled_ && protocol_ != nullptr) {
        protocol_->clearPending();
    }
}

void SerialController::handleReadyRead() {
    const QByteArray bytes = serialPort_.readAll();
    if (bytes.isEmpty()) {
        return;
    }
    emit bytesReceived(bytes);
    if (protocolEnabled_ && protocol_ != nullptr) {
        protocol_->ingestBytes(QByteArrayView(bytes));
    }
}

void SerialController::handleSerialError(
    QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) {
        return;
    }

    const QString detail = serialPort_.errorString();
    const QString message = detail.isEmpty()
                                ? QStringLiteral("串口发生错误")
                                : QStringLiteral("串口错误：%1").arg(detail);
    if (opening_) {
        if (error == QSerialPort::ResourceError) {
            openErrorReported_ = true;
            serialPort_.close();
            reportError(message);
        }
        return;
    }

    if (error == QSerialPort::ResourceError) {
        if (connected_ || serialPort_.isOpen()) {
            clearProtocolAndEmitClosed();
        }
        reportError(message);
        return;
    }

    reportError(message);
}

void SerialController::handleProtocolBytes(QByteArray bytes) {
    if (protocolEnabled_) {
        write(QByteArrayView(bytes));
    }
}

void SerialController::reportError(const QString &message) {
    emit serialError(message);
}

void SerialController::clearProtocolAndEmitClosed() {
    serialPort_.close();
    connected_ = false;
    if (protocol_ != nullptr) {
        protocol_->clearPending();
    }
    emit closed();
}
