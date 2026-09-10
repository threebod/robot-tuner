#pragma once

#include <QByteArray>
#include <QVector>
#include <QWidget>

#include "protocol/ProtocolTypes.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;

class SerialDebugPanel : public QWidget {
    Q_OBJECT

public:
    explicit SerialDebugPanel(const QString &objectPrefix,
                              QWidget *parent = nullptr);
    void setConnected(bool connected);
    void appendTx(const QByteArray &bytes);
    void appendRx(const QByteArray &bytes);
    void appendDecodedFrame(const protocol::Frame &frame);
    bool saveLogToFile(const QString &path) const;
    bool cyclicSending() const;

signals:
    void rawSendRequested(QByteArray bytes);

private:
    QByteArray encodedInput(const QString &text, bool *ok) const;
    bool sendText(const QString &text);
    void appendLine(const QString &direction, const QByteArray &bytes);
    QString formatBytes(const QByteArray &bytes) const;
    void stopCycle();
    void updateCounts();

    QString objectPrefix_;
    QComboBox *displayModeCombo_{};
    QCheckBox *timestampCheckBox_{};
    QCheckBox *hexSendCheckBox_{};
    QCheckBox *newlineCheckBox_{};
    QPlainTextEdit *logTextEdit_{};
    QLineEdit *inputLineEdit_{};
    QPushButton *sendButton_{};
    QPushButton *clearButton_{};
    QPushButton *pauseButton_{};
    QPushButton *cycleButton_{};
    QSpinBox *cyclePeriodSpinBox_{};
    QLabel *statusLabel_{};
    QLabel *countsLabel_{};
    QVector<QCheckBox *> presetEnabled_;
    QVector<QLineEdit *> presetInputs_;
    QVector<QPushButton *> presetSendButtons_;
    QTimer *cycleTimer_{};
    qint64 txCount_{};
    qint64 rxCount_{};
    bool connected_{};
    bool paused_{};
};
