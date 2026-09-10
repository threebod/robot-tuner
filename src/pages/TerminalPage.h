#pragma once

#include <QByteArray>
#include <QWidget>

#include "protocol/ProtocolTypes.h"

class SerialDebugPanel;

class TerminalPage : public QWidget {
    Q_OBJECT

public:
    explicit TerminalPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void appendTx(const QByteArray &bytes);
    void appendRx(const QByteArray &bytes);
    void appendDecodedFrame(const protocol::Frame &frame);

signals:
    void rawSendRequested(QByteArray bytes);

private:
    SerialDebugPanel *panel_{};
};
