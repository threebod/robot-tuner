#include "widgets/SerialDebugPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

SerialDebugPanel::SerialDebugPanel(const QString &objectPrefix, QWidget *parent)
    : QWidget(parent), objectPrefix_(objectPrefix) {
    auto named = [this](const QString &suffix) { return objectPrefix_ + suffix; };
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *toolbar = new QHBoxLayout;
    displayModeCombo_ = new QComboBox(this);
    displayModeCombo_->setObjectName(named(QStringLiteral("DisplayModeCombo")));
    displayModeCombo_->addItems({QStringLiteral("HEX"), QStringLiteral("ASCII")});
    timestampCheckBox_ = new QCheckBox(QStringLiteral("时间戳"), this);
    timestampCheckBox_->setObjectName(named(QStringLiteral("TimestampCheckBox")));
    timestampCheckBox_->setChecked(true);
    clearButton_ = new QPushButton(QStringLiteral("清空"), this);
    clearButton_->setObjectName(named(QStringLiteral("ClearButton")));
    pauseButton_ = new QPushButton(QStringLiteral("暂停显示"), this);
    pauseButton_->setObjectName(named(QStringLiteral("PauseButton")));
    auto *saveButton = new QPushButton(QStringLiteral("保存"), this);
    saveButton->setObjectName(named(QStringLiteral("SaveButton")));
    countsLabel_ = new QLabel(this);
    countsLabel_->setObjectName(named(QStringLiteral("CountsLabel")));
    toolbar->addWidget(new QLabel(QStringLiteral("显示格式"), this));
    toolbar->addWidget(displayModeCombo_);
    toolbar->addWidget(timestampCheckBox_);
    toolbar->addWidget(countsLabel_);
    toolbar->addStretch();
    toolbar->addWidget(saveButton);
    toolbar->addWidget(clearButton_);
    toolbar->addWidget(pauseButton_);
    layout->addLayout(toolbar);

    logTextEdit_ = new QPlainTextEdit(this);
    logTextEdit_->setObjectName(named(QStringLiteral("LogTextEdit")));
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setPlaceholderText(QStringLiteral("当前串口的原始 TX/RX 数据将显示在这里"));
    layout->addWidget(logTextEdit_, 1);

    auto *sendGroup = new QGroupBox(QStringLiteral("原始串口发送"), this);
    auto *sendLayout = new QVBoxLayout(sendGroup);
    auto *sendTabs = new QTabWidget(sendGroup);
    auto *singleTab = new QWidget(sendTabs);
    auto *singleRow = new QHBoxLayout(singleTab);
    inputLineEdit_ = new QLineEdit(singleTab);
    inputLineEdit_->setObjectName(named(QStringLiteral("InputLineEdit")));
    inputLineEdit_->setPlaceholderText(QStringLiteral("输入文本或 HEX 字节"));
    sendButton_ = new QPushButton(QStringLiteral("发送"), singleTab);
    sendButton_->setObjectName(named(QStringLiteral("SendButton")));
    hexSendCheckBox_ = new QCheckBox(QStringLiteral("HEX 发送"), singleTab);
    hexSendCheckBox_->setObjectName(named(QStringLiteral("HexSendCheckBox")));
    hexSendCheckBox_->setChecked(true);
    newlineCheckBox_ = new QCheckBox(QStringLiteral("发送换行 CRLF"), singleTab);
    newlineCheckBox_->setObjectName(named(QStringLiteral("NewlineCheckBox")));
    singleRow->addWidget(inputLineEdit_, 1);
    singleRow->addWidget(hexSendCheckBox_);
    singleRow->addWidget(newlineCheckBox_);
    singleRow->addWidget(sendButton_);
    sendTabs->addTab(singleTab, QStringLiteral("单条发送"));

    auto *presetTab = new QWidget(sendTabs);
    auto *presetLayout = new QVBoxLayout(presetTab);
    auto *presetGrid = new QGridLayout;
    for (int index = 0; index < 10; ++index) {
        auto *enabled = new QCheckBox(QString::number(index + 1), sendGroup);
        enabled->setObjectName(named(QStringLiteral("PresetEnabled%1").arg(index)));
        auto *input = new QLineEdit(sendGroup);
        input->setObjectName(named(QStringLiteral("PresetInput%1").arg(index)));
        auto *button = new QPushButton(QStringLiteral("发送"), sendGroup);
        button->setObjectName(named(QStringLiteral("PresetSend%1").arg(index)));
        const int row = index % 5;
        const int column = (index / 5) * 3;
        presetGrid->addWidget(enabled, row, column);
        presetGrid->addWidget(input, row, column + 1);
        presetGrid->addWidget(button, row, column + 2);
        presetInputs_.push_back(input);
        presetEnabled_.push_back(enabled);
        presetSendButtons_.push_back(button);
        connect(button, &QPushButton::clicked, this,
                [this, input] { sendText(input->text()); });
    }
    presetGrid->setColumnStretch(1, 1);
    presetGrid->setColumnStretch(4, 1);
    presetLayout->addLayout(presetGrid);

    auto *cycleRow = new QHBoxLayout;
    cycleButton_ = new QPushButton(QStringLiteral("开始循环发送"), sendGroup);
    cycleButton_->setObjectName(named(QStringLiteral("CycleButton")));
    cyclePeriodSpinBox_ = new QSpinBox(sendGroup);
    cyclePeriodSpinBox_->setObjectName(named(QStringLiteral("CyclePeriodSpinBox")));
    cyclePeriodSpinBox_->setRange(50, 60000);
    cyclePeriodSpinBox_->setValue(1000);
    cyclePeriodSpinBox_->setSuffix(QStringLiteral(" ms"));
    cycleRow->addWidget(cycleButton_);
    cycleRow->addWidget(new QLabel(QStringLiteral("周期"), sendGroup));
    cycleRow->addWidget(cyclePeriodSpinBox_);
    cycleRow->addStretch();
    presetLayout->addLayout(cycleRow);
    sendTabs->addTab(presetTab, QStringLiteral("多条发送"));
    sendLayout->addWidget(sendTabs);
    auto *bypassNotice = new QLabel(
        QStringLiteral("原始发送绕过请求跟踪，仅用于串口调试。"), sendGroup);
    bypassNotice->setObjectName(named(QStringLiteral("BypassNotice")));
    sendLayout->addWidget(bypassNotice);
    layout->addWidget(sendGroup);

    statusLabel_ = new QLabel(QStringLiteral("未连接"), this);
    statusLabel_->setObjectName(named(QStringLiteral("StatusLabel")));
    layout->addWidget(statusLabel_);

    cycleTimer_ = new QTimer(this);
    connect(sendButton_, &QPushButton::clicked, this, [this] {
        if (sendText(inputLineEdit_->text())) {
            inputLineEdit_->clear();
        }
    });
    connect(inputLineEdit_, &QLineEdit::returnPressed, sendButton_,
            &QPushButton::click);
    connect(clearButton_, &QPushButton::clicked, this, [this] {
        logTextEdit_->clear();
        txCount_ = 0;
        rxCount_ = 0;
        updateCounts();
    });
    connect(pauseButton_, &QPushButton::clicked, this, [this] {
        paused_ = !paused_;
        pauseButton_->setText(paused_ ? QStringLiteral("继续显示")
                                      : QStringLiteral("暂停显示"));
        statusLabel_->setText(paused_ ? QStringLiteral("显示已暂停")
                                      : (connected_ ? QStringLiteral("已连接")
                                                    : QStringLiteral("未连接")));
    });
    connect(saveButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存串口日志"), QStringLiteral("serial.txt"),
            QStringLiteral("文本文件 (*.txt);;所有文件 (*)"));
        if (!path.isEmpty() && !saveLogToFile(path)) {
            statusLabel_->setText(QStringLiteral("保存失败"));
        }
    });
    connect(cyclePeriodSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int intervalMs) { cycleTimer_->setInterval(intervalMs); });
    connect(cycleButton_, &QPushButton::clicked, this, [this] {
        if (cycleTimer_->isActive()) {
            stopCycle();
            return;
        }
        bool hasSelection = false;
        for (int index = 0; index < presetInputs_.size(); ++index) {
            if (!presetEnabled_[index]->isChecked()) {
                continue;
            }
            bool ok = false;
            encodedInput(presetInputs_[index]->text(), &ok);
            if (!ok) {
                statusLabel_->setText(hexSendCheckBox_->isChecked()
                                          ? QStringLiteral("HEX 数据格式无效")
                                          : QStringLiteral("循环条目不能为空"));
                return;
            }
            hasSelection = true;
        }
        if (!hasSelection) {
            statusLabel_->setText(QStringLiteral("请勾选循环发送条目"));
            return;
        }
        cycleTimer_->start(cyclePeriodSpinBox_->value());
        cycleButton_->setText(QStringLiteral("停止循环发送"));
        statusLabel_->setText(QStringLiteral("循环发送中"));
    });
    connect(cycleTimer_, &QTimer::timeout, this, [this] {
        for (int index = 0; index < presetInputs_.size(); ++index) {
            if (presetEnabled_[index]->isChecked()) {
                sendText(presetInputs_[index]->text());
            }
        }
    });
    updateCounts();
    setConnected(false);
}

void SerialDebugPanel::setConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        stopCycle();
    }
    sendButton_->setEnabled(connected_);
    cycleButton_->setEnabled(connected_);
    for (int index = 0; index < presetInputs_.size(); ++index) {
        presetSendButtons_[index]->setEnabled(connected_);
    }
    if (!paused_) {
        statusLabel_->setText(connected_ ? QStringLiteral("已连接")
                                         : QStringLiteral("未连接"));
    }
}

void SerialDebugPanel::appendTx(const QByteArray &bytes) {
    txCount_ += bytes.size();
    updateCounts();
    appendLine(QStringLiteral("TX"), bytes);
}

void SerialDebugPanel::appendRx(const QByteArray &bytes) {
    rxCount_ += bytes.size();
    updateCounts();
    appendLine(QStringLiteral("RX"), bytes);
}

void SerialDebugPanel::appendDecodedFrame(const protocol::Frame &frame) {
    if (paused_) {
        return;
    }
    const QString prefix = timestampCheckBox_->isChecked()
                               ? QDateTime::currentDateTime().toString(
                                     QStringLiteral("HH:mm:ss.zzz "))
                               : QString();
    logTextEdit_->appendPlainText(
        QStringLiteral("%1FRAME seq=%2 cmd=0x%3 flags=0x%4 payload=%5")
            .arg(prefix)
            .arg(frame.sequence)
            .arg(frame.command, 2, 16, QLatin1Char('0'))
            .arg(frame.flags, 2, 16, QLatin1Char('0'))
            .arg(QString::fromLatin1(frame.payload.toHex(' ').toUpper())));
}

bool SerialDebugPanel::saveLogToFile(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(logTextEdit_->toPlainText().toUtf8()) >= 0;
}

bool SerialDebugPanel::cyclicSending() const {
    return cycleTimer_->isActive();
}

QByteArray SerialDebugPanel::encodedInput(const QString &text, bool *ok) const {
    if (hexSendCheckBox_->isChecked()) {
        QString compact = text;
        compact.remove(QRegularExpression(QStringLiteral("\\s+")));
        const QByteArray encoded = compact.toLatin1();
        const QByteArray bytes = QByteArray::fromHex(encoded);
        *ok = !encoded.isEmpty() && encoded.size() % 2 == 0 &&
              bytes.size() * 2 == encoded.size();
        return *ok ? bytes : QByteArray();
    }
    QByteArray bytes = text.toUtf8();
    *ok = !bytes.isEmpty();
    if (*ok && newlineCheckBox_->isChecked()) {
        bytes.append("\r\n");
    }
    return bytes;
}

bool SerialDebugPanel::sendText(const QString &text) {
    if (!connected_) {
        return false;
    }
    bool ok = false;
    const QByteArray bytes = encodedInput(text, &ok);
    if (!ok) {
        statusLabel_->setText(hexSendCheckBox_->isChecked()
                                  ? QStringLiteral("HEX 数据格式无效")
                                  : QStringLiteral("发送内容不能为空"));
        return false;
    }
    emit rawSendRequested(bytes);
    return true;
}

void SerialDebugPanel::appendLine(const QString &direction,
                                  const QByteArray &bytes) {
    if (paused_ || bytes.isEmpty()) {
        return;
    }
    const QString prefix = timestampCheckBox_->isChecked()
                               ? QDateTime::currentDateTime().toString(
                                     QStringLiteral("HH:mm:ss.zzz "))
                               : QString();
    logTextEdit_->appendPlainText(
        QStringLiteral("%1%2 %3").arg(prefix, direction, formatBytes(bytes)));
}

QString SerialDebugPanel::formatBytes(const QByteArray &bytes) const {
    if (displayModeCombo_->currentIndex() == 0) {
        return QString::fromLatin1(bytes.toHex(' ').toUpper());
    }
    QString text = QString::fromUtf8(bytes);
    text.replace(QChar('\r'), QStringLiteral("\\r"));
    text.replace(QChar('\n'), QStringLiteral("\\n"));
    return text;
}

void SerialDebugPanel::stopCycle() {
    cycleTimer_->stop();
    cycleButton_->setText(QStringLiteral("开始循环发送"));
}

void SerialDebugPanel::updateCounts() {
    countsLabel_->setText(QStringLiteral("S:%1  R:%2").arg(txCount_).arg(rxCount_));
}
