#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

#include "widgets/SerialDebugPanel.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

void waitMs(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

}  // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    SerialDebugPanel panel(QStringLiteral("debug"));
    auto *log = panel.findChild<QPlainTextEdit *>("debugLogTextEdit");
    auto *displayMode = panel.findChild<QComboBox *>("debugDisplayModeCombo");
    auto *timestamp = panel.findChild<QCheckBox *>("debugTimestampCheckBox");
    auto *hexSend = panel.findChild<QCheckBox *>("debugHexSendCheckBox");
    auto *newline = panel.findChild<QCheckBox *>("debugNewlineCheckBox");
    auto *input = panel.findChild<QLineEdit *>("debugInputLineEdit");
    auto *send = panel.findChild<QPushButton *>("debugSendButton");
    auto *clear = panel.findChild<QPushButton *>("debugClearButton");
    auto *pause = panel.findChild<QPushButton *>("debugPauseButton");
    auto *cycle = panel.findChild<QPushButton *>("debugCycleButton");
    auto *period = panel.findChild<QSpinBox *>("debugCyclePeriodSpinBox");
    auto *status = panel.findChild<QLabel *>("debugStatusLabel");
    auto *counts = panel.findChild<QLabel *>("debugCountsLabel");
    auto *preset0 = panel.findChild<QLineEdit *>("debugPresetInput0");
    auto *preset9 = panel.findChild<QLineEdit *>("debugPresetInput9");
    auto *preset0Enabled = panel.findChild<QCheckBox *>("debugPresetEnabled0");
    auto *preset1Enabled = panel.findChild<QCheckBox *>("debugPresetEnabled1");
    auto *preset0Send = panel.findChild<QPushButton *>("debugPresetSend0");
    if (!require(log && displayMode && timestamp && hexSend && newline &&
                     input && send && clear && pause && cycle && period &&
                     status && counts && preset0 && preset9 && preset0Enabled &&
                     preset1Enabled && preset0Send,
                 "serial debug controls are incomplete") ||
        !require(period->minimum() == 50 && period->maximum() == 60000 &&
                     period->value() == 1000,
                 "cycle period range or default is incorrect") ||
        !require(displayMode->currentIndex() == 1,
                 "serial display does not default to ASCII") ||
        !require(!send->isEnabled() && !cycle->isEnabled(),
                 "serial sending was enabled while disconnected") ||
        !require(input->isEnabled() && preset0->isEnabled(),
                 "serial drafts cannot be edited while disconnected")) {
        return 1;
    }

    panel.appendTx(QByteArray::fromHex("aa55"));
    panel.appendRx(QByteArray("OK"));
    if (!require(log->toPlainText().contains(QStringLiteral("TX")) &&
                     log->toPlainText().contains(QStringLiteral("RX")) &&
                     counts->text().contains(QStringLiteral("S:2")) &&
                     counts->text().contains(QStringLiteral("R:2")),
                 "serial log or byte counters are incorrect")) {
        return 1;
    }
    displayMode->setCurrentIndex(1);
    timestamp->setChecked(false);
    panel.appendRx(QByteArray("ASCII"));
    if (!require(log->toPlainText().contains(QStringLiteral("RX ASCII")),
                 "ASCII display or timestamp toggle is incorrect")) {
        return 1;
    }

    clear->click();
    panel.setTextStreamMode(true);
    if (!require(!hexSend->isChecked() && newline->isChecked() &&
                     !cycle->isEnabled(),
                 "text mode send defaults or cycle restriction are incorrect")) {
        return 1;
    }
    panel.appendTx(QByteArray("hb\r\n"));
    panel.appendRx(QByteArray("Mecan"));
    panel.appendRx(QByteArray("um\r"));
    panel.appendRx(QByteArray("\nready\r\n"));
    if (!require(log->toPlainText() == QStringLiteral("Mecanum\nready\n"),
                 "ASCII text stream did not join fragments or render CRLF") ||
        !require(!log->toPlainText().contains(QStringLiteral("TX")) &&
                     !log->toPlainText().contains(QStringLiteral("RX")) &&
                     !log->toPlainText().contains(QStringLiteral("\\r")) &&
                     !log->toPlainText().contains(QStringLiteral("\\n")),
                 "ASCII text stream contains protocol log decorations")) {
        return 1;
    }
    displayMode->setCurrentIndex(0);
    panel.appendRx(QByteArray("A"));
    if (!require(log->toPlainText().contains(QStringLiteral("RX 41")),
                 "manual HEX display no longer uses record formatting")) {
        return 1;
    }
    displayMode->setCurrentIndex(1);
    panel.setTextStreamMode(false);
    pause->click();
    panel.appendRx(QByteArray("hidden"));
    if (!require(!log->toPlainText().contains(QStringLiteral("hidden")),
                 "pause did not suppress display updates")) {
        return 1;
    }
    pause->click();

    QVector<QByteArray> sent;
    QObject::connect(&panel, &SerialDebugPanel::rawSendRequested,
                     [&](QByteArray bytes) { sent.push_back(bytes); });
    panel.setConnected(true);
    hexSend->setChecked(true);
    input->setText(QStringLiteral("DE AD"));
    send->click();
    if (!require(sent.size() == 1 && sent.back() == QByteArray::fromHex("dead"),
                 "single HEX send is incorrect")) {
        return 1;
    }
    input->setText(QStringLiteral("GG"));
    send->click();
    if (!require(sent.size() == 1 && status->text().contains(QStringLiteral("HEX")),
                 "invalid HEX was sent or not reported")) {
        return 1;
    }
    hexSend->setChecked(false);
    newline->setChecked(true);
    input->setText(QStringLiteral("go"));
    send->click();
    if (!require(sent.back() == QByteArray("go\r\n"),
                 "ASCII CRLF send is incorrect")) {
        return 1;
    }

    newline->setChecked(false);
    preset0->setText(QStringLiteral("first"));
    preset0Send->click();
    if (!require(sent.back() == QByteArray("first"),
                 "preset row send is incorrect")) {
        return 1;
    }
    auto *preset1 = panel.findChild<QLineEdit *>("debugPresetInput1");
    preset1->setText(QStringLiteral("second"));
    preset0Enabled->setChecked(true);
    preset1Enabled->setChecked(true);
    period->setValue(50);
    const int beforeCycle = sent.size();
    cycle->click();
    waitMs(70);
    if (!require(panel.cyclicSending() && sent.size() >= beforeCycle + 2,
                 "enabled presets were not sent cyclically")) {
        return 1;
    }
    panel.setConnected(false);
    const int afterDisconnect = sent.size();
    waitMs(70);
    if (!require(!panel.cyclicSending() && sent.size() == afterDisconnect,
                 "disconnect did not stop cyclic sending")) {
        return 1;
    }

    panel.setConnected(true);
    cycle->click();
    panel.setTextStreamMode(true);
    const int beforeTextCycle = sent.size();
    cycle->click();
    waitMs(70);
    if (!require(!panel.cyclicSending() && !cycle->isEnabled() &&
                     sent.size() == beforeTextCycle,
                 "switching to text mode did not cancel/reject cyclic sending")) {
        return 1;
    }
    preset0->setText(QStringLiteral("status"));
    preset0Send->click();
    if (!require(sent.back() == QByteArray("status\r\n"),
                 "text presets cannot be sent individually with CRLF")) {
        return 1;
    }
    panel.setTextStreamMode(false);
    hexSend->setChecked(false);
    const int beforeStop = sent.size();
    const auto stopDuringCycle = QObject::connect(
        &panel, &SerialDebugPanel::rawSendRequested, &panel,
        [&panel](QByteArray) { panel.stopCycle(); });
    cycle->click();
    waitMs(70);
    QObject::disconnect(stopDuringCycle);
    if (!require(!panel.cyclicSending() && sent.size() == beforeStop + 1,
                 "remaining entries escaped a stop during the cycle callback")) {
        return 1;
    }
    panel.setConnected(false);

    QTemporaryDir directory;
    const QString savePath = directory.filePath(QStringLiteral("serial.txt"));
    if (!require(panel.saveLogToFile(savePath), "serial log save failed")) {
        return 1;
    }
    QFile saved(savePath);
    if (!require(saved.open(QIODevice::ReadOnly) &&
                     QString::fromUtf8(saved.readAll()).contains(
                         QStringLiteral("Mecanum")),
                 "saved serial log content is incorrect")) {
        return 1;
    }
    clear->click();
    if (!require(log->toPlainText().isEmpty() &&
                     counts->text().contains(QStringLiteral("S:0")) &&
                     counts->text().contains(QStringLiteral("R:0")),
                 "clear did not reset log and counters")) {
        return 1;
    }
    return 0;
}
