#pragma once

#include <QByteArray>
#include <QWidget>

#include "protocol/ProtocolTypes.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

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

private slots:
    void clearLog();
    void togglePause();
    void sendRaw();
    void updateDisplayMode(int index);

private:
    QString formatBytes(const QByteArray &bytes) const;
    void appendLine(const QString &direction, const QByteArray &bytes);

    QComboBox *displayModeCombo_{};
    QPlainTextEdit *logTextEdit_{};
    QLineEdit *inputLineEdit_{};
    QPushButton *sendButton_{};
    QPushButton *clearButton_{};
    QPushButton *pauseButton_{};
    QLabel *statusLabel_{};
    bool connected_{};
    bool paused_{};
    bool hexMode_{true};
};
