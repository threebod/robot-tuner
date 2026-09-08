#include "pages/TerminalPage.h"

#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

TerminalPage::TerminalPage(QWidget *parent) : QWidget(parent) {
    auto *pageLayout = new QVBoxLayout(this);

    auto *toolbar = new QHBoxLayout;
    displayModeCombo_ = new QComboBox(this);
    displayModeCombo_->setObjectName(QStringLiteral("terminalDisplayModeCombo"));
    displayModeCombo_->addItem(QStringLiteral("HEX"));
    displayModeCombo_->addItem(QStringLiteral("ASCII"));
    clearButton_ = new QPushButton(QStringLiteral("清空"), this);
    clearButton_->setObjectName(QStringLiteral("terminalClearButton"));
    pauseButton_ = new QPushButton(QStringLiteral("暂停显示"), this);
    pauseButton_->setObjectName(QStringLiteral("terminalPauseButton"));
    toolbar->addWidget(new QLabel(QStringLiteral("显示格式"), this));
    toolbar->addWidget(displayModeCombo_);
    toolbar->addStretch();
    toolbar->addWidget(clearButton_);
    toolbar->addWidget(pauseButton_);
    pageLayout->addLayout(toolbar);

    logTextEdit_ = new QPlainTextEdit(this);
    logTextEdit_->setObjectName(QStringLiteral("terminalLogTextEdit"));
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setPlaceholderText(
        QStringLiteral("串口 TX/RX 与已解码帧将显示在这里"));
    pageLayout->addWidget(logTextEdit_, 1);

    auto *manualGroup = new QGroupBox(QStringLiteral("原始串口发送"), this);
    auto *manualLayout = new QVBoxLayout(manualGroup);
    auto *manualRow = new QHBoxLayout;
    inputLineEdit_ = new QLineEdit(manualGroup);
    inputLineEdit_->setObjectName(QStringLiteral("terminalInputLineEdit"));
    inputLineEdit_->setPlaceholderText(QStringLiteral("HEX：AA 55；ASCII：直接输入文本"));
    sendButton_ = new QPushButton(QStringLiteral("发送"), manualGroup);
    sendButton_->setObjectName(QStringLiteral("terminalSendButton"));
    manualRow->addWidget(inputLineEdit_, 1);
    manualRow->addWidget(sendButton_);
    manualLayout->addLayout(manualRow);
    auto *bypassNotice = new QLabel(
        QStringLiteral("手动发送会绕过请求跟踪，仅用于原始串口调试；设备仍执行自身安全校验。"),
        manualGroup);
    bypassNotice->setObjectName(QStringLiteral("terminalBypassNotice"));
    bypassNotice->setWordWrap(true);
    manualLayout->addWidget(bypassNotice);
    pageLayout->addWidget(manualGroup);

    statusLabel_ = new QLabel(QStringLiteral("未连接"), this);
    statusLabel_->setObjectName(QStringLiteral("terminalStatusLabel"));
    pageLayout->addWidget(statusLabel_);

    connect(displayModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TerminalPage::updateDisplayMode);
    connect(clearButton_, &QPushButton::clicked, this,
            &TerminalPage::clearLog);
    connect(pauseButton_, &QPushButton::clicked, this,
            &TerminalPage::togglePause);
    connect(sendButton_, &QPushButton::clicked, this, &TerminalPage::sendRaw);
    connect(inputLineEdit_, &QLineEdit::returnPressed, this,
            &TerminalPage::sendRaw);
    setConnected(false);
}

void TerminalPage::setConnected(bool connected) {
    connected_ = connected;
    sendButton_->setEnabled(connected_);
    inputLineEdit_->setEnabled(connected_);
    statusLabel_->setText(connected_ ? QStringLiteral("已连接")
                                     : QStringLiteral("未连接"));
}

void TerminalPage::appendTx(const QByteArray &bytes) {
    appendLine(QStringLiteral("TX"), bytes);
}

void TerminalPage::appendRx(const QByteArray &bytes) {
    appendLine(QStringLiteral("RX"), bytes);
}

void TerminalPage::appendDecodedFrame(const protocol::Frame &frame) {
    if (paused_) {
        return;
    }
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString flags = QStringLiteral("0x%1").arg(
        frame.flags, 2, 16, QLatin1Char('0'));
    const QString command = QStringLiteral("0x%1").arg(
        frame.command, 2, 16, QLatin1Char('0'));
    logTextEdit_->appendPlainText(
        QStringLiteral("%1 FRAME seq=%2 cmd=%3 flags=%4 payload=%5")
            .arg(timestamp)
            .arg(frame.sequence)
            .arg(command)
            .arg(flags)
            .arg(QString::fromLatin1(frame.payload.toHex(' ').toUpper())));
}

void TerminalPage::clearLog() {
    logTextEdit_->clear();
}

void TerminalPage::togglePause() {
    paused_ = !paused_;
    pauseButton_->setText(paused_ ? QStringLiteral("继续显示")
                                  : QStringLiteral("暂停显示"));
    statusLabel_->setText(paused_ ? QStringLiteral("显示已暂停")
                                  : (connected_ ? QStringLiteral("已连接")
                                                 : QStringLiteral("未连接")));
}

void TerminalPage::sendRaw() {
    if (!connected_) {
        return;
    }
    QByteArray bytes;
    if (hexMode_) {
        const QByteArray compact = inputLineEdit_->text()
                                       .toLatin1()
                                       .replace(" ", "")
                                       .replace("\t", "");
        if (compact.isEmpty() || compact.size() % 2 != 0) {
            statusLabel_->setText(QStringLiteral("HEX 数据必须是偶数位"));
            return;
        }
        bytes = QByteArray::fromHex(compact);
        if (bytes.size() * 2 != compact.size()) {
            statusLabel_->setText(QStringLiteral("HEX 数据格式无效"));
            return;
        }
    } else {
        bytes = inputLineEdit_->text().toUtf8();
        if (bytes.isEmpty()) {
            return;
        }
    }
    emit rawSendRequested(bytes);
    appendTx(bytes);
    inputLineEdit_->clear();
}

void TerminalPage::updateDisplayMode(int index) {
    hexMode_ = index == 0;
}

QString TerminalPage::formatBytes(const QByteArray &bytes) const {
    if (hexMode_) {
        return QString::fromLatin1(bytes.toHex(' ').toUpper());
    }
    QString text = QString::fromUtf8(bytes);
    text.replace(QChar('\r'), QStringLiteral("\\r"));
    text.replace(QChar('\n'), QStringLiteral("\\n"));
    return text;
}

void TerminalPage::appendLine(const QString &direction,
                              const QByteArray &bytes) {
    if (paused_ || bytes.isEmpty()) {
        return;
    }
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    logTextEdit_->appendPlainText(
        QStringLiteral("%1 %2 %3").arg(timestamp, direction, formatBytes(bytes)));
}
