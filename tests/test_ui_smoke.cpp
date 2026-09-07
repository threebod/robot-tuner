#include <QApplication>
#include <QListWidget>
#include <QPushButton>

#include "app/MainWindow.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow window;
    auto *nav = window.findChild<QListWidget *>("navigationList");
    auto *stop = window.findChild<QPushButton *>("emergencyStopButton");
    if (!nav || nav->count() != 7 || !stop || stop->isEnabled()) return 1;
    return 0;
}
