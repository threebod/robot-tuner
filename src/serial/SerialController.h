#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QObject>
#include <QStringList>

#include <QSerialPort>

class ProtocolClient;

class SerialController : public QObject {
    Q_OBJECT

public:
    explicit SerialController(ProtocolClient *protocol = nullptr,
                               QObject *parent = nullptr);
    explicit SerialController(ProtocolClient &protocol, QObject *parent = nullptr);

    QStringList availablePorts() const;
    bool open(QString portName, qint32 baudRate);
    void close();
    qint64 write(QByteArrayView bytes);
    void setProtocolEnabled(bool enabled);

signals:
    void opened();
    void closed();
    void bytesReceived(QByteArray bytes);
    void serialError(QString message);

private slots:
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);

private:
    void handleProtocolBytes(QByteArray bytes);
    void reportError(const QString &message);
    void clearProtocolAndEmitClosed();

    QSerialPort serialPort_;
    ProtocolClient *protocol_{};
    bool connected_{};
    bool opening_{};
    bool openErrorReported_{};
    bool protocolEnabled_{true};
};
