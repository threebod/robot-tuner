#include "MainWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

namespace {

static const QStringList kPages = {
    "总览", "底盘与 PID", "机械臂与舵机", "HWT101",
    "动作测试", "串口终端", "视觉（预留）"
};

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *connectionBar = new QHBoxLayout;
    auto *connectionStatus = new QLabel("未连接", centralWidget);
    auto *connectButton = new QPushButton("连接", centralWidget);
    auto *emergencyStopButton = new QPushButton("急停", centralWidget);
    emergencyStopButton->setObjectName("emergencyStopButton");
    emergencyStopButton->setEnabled(false);
    connectionBar->addWidget(connectionStatus);
    connectionBar->addStretch();
    connectionBar->addWidget(connectButton);
    connectionBar->addWidget(emergencyStopButton);
    mainLayout->addLayout(connectionBar);

    auto *contentLayout = new QHBoxLayout;
    auto *navigationList = new QListWidget(centralWidget);
    navigationList->setObjectName("navigationList");
    auto *pageStack = new QStackedWidget(centralWidget);
    pageStack->setObjectName("pageStack");

    for (const auto &pageName : kPages) {
        navigationList->addItem(pageName);

        auto *page = new QWidget(pageStack);
        page->setObjectName(pageName);
        auto *pageLayout = new QVBoxLayout(page);
        auto *pageLabel = new QLabel(pageName, page);
        pageLabel->setAlignment(Qt::AlignCenter);
        pageLayout->addWidget(pageLabel);
        pageStack->addWidget(page);
    }

    QObject::connect(navigationList, &QListWidget::currentRowChanged,
                     pageStack, &QStackedWidget::setCurrentIndex);
    navigationList->setCurrentRow(0);

    contentLayout->addWidget(navigationList);
    contentLayout->addWidget(pageStack, 1);
    mainLayout->addLayout(contentLayout, 1);

    setCentralWidget(centralWidget);
    setWindowTitle("机器人调试上位机");
    resize(1000, 700);
}
