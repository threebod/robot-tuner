#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

#include "app/MainWindow.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow window;
    auto *nav = window.findChild<QListWidget *>("navigationList");
    auto *portCombo = window.findChild<QComboBox *>("portCombo");
    auto *baudCombo = window.findChild<QComboBox *>("baudCombo");
    auto *refreshPortsButton = window.findChild<QPushButton *>("refreshPortsButton");
    auto *connectButton = window.findChild<QPushButton *>("connectButton");
    auto *connectionStatusLabel =
        window.findChild<QLabel *>("connectionStatusLabel");
    auto *stop = window.findChild<QPushButton *>("emergencyStopButton");
    auto *tuningPage = window.findChild<QWidget *>("底盘与 PID");
    auto *actionPage = window.findChild<QWidget *>("动作测试");
    if (!nav || nav->count() != 7 || !portCombo || !baudCombo ||
        !refreshPortsButton || !connectButton || !connectionStatusLabel ||
        !stop || stop->isEnabled() || !tuningPage || tuningPage->isEnabled() ||
        !actionPage || actionPage->isEnabled() || baudCombo->count() != 2 ||
        connectionStatusLabel->text() != QStringLiteral("未连接")) {
        return 1;
    }
    return 0;
}
