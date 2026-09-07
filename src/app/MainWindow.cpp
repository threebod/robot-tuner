#include "MainWindow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

#include <utility>

#include "pages/ChassisPage.h"
#include "pages/MechanismPage.h"

namespace {

static const QStringList kPages = {
    "总览", "底盘与 PID", "机械臂与舵机", "HWT101",
    "动作测试", "串口终端", "视觉（预留）"
};

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      protocol_(this),
      device_(&protocol_, this),
      serial_(&protocol_, this) {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *connectionBar = new QHBoxLayout;
    portCombo_ = new QComboBox(centralWidget);
    portCombo_->setObjectName("portCombo");
    baudCombo_ = new QComboBox(centralWidget);
    baudCombo_->setObjectName("baudCombo");
    baudCombo_->addItem(QStringLiteral("9600"), 9600);
    baudCombo_->addItem(QStringLiteral("115200"), 115200);
    baudCombo_->setCurrentIndex(1);

    refreshPortsButton_ = new QPushButton("刷新串口", centralWidget);
    refreshPortsButton_->setObjectName("refreshPortsButton");
    connectButton_ = new QPushButton("连接", centralWidget);
    connectButton_->setObjectName("connectButton");
    connectionStatusLabel_ = new QLabel("未连接", centralWidget);
    connectionStatusLabel_->setObjectName("connectionStatusLabel");
    emergencyStopButton_ = new QPushButton("急停", centralWidget);
    emergencyStopButton_->setObjectName("emergencyStopButton");
    emergencyStopButton_->setEnabled(false);

    connectionBar->addWidget(portCombo_);
    connectionBar->addWidget(baudCombo_);
    connectionBar->addWidget(refreshPortsButton_);
    connectionBar->addWidget(connectionStatusLabel_);
    connectionBar->addStretch();
    connectionBar->addWidget(connectButton_);
    connectionBar->addWidget(emergencyStopButton_);
    mainLayout->addLayout(connectionBar);

    auto *contentLayout = new QHBoxLayout;
    auto *navigationList = new QListWidget(centralWidget);
    navigationList->setObjectName("navigationList");
    pageStack_ = new QStackedWidget(centralWidget);
    pageStack_->setObjectName("pageStack");

    for (const auto &pageName : kPages) {
        navigationList->addItem(pageName);

        QWidget *page = nullptr;
        if (pageName == QStringLiteral("底盘与 PID")) {
            chassisPage_ = new ChassisPage(pageStack_);
            page = chassisPage_;
        } else if (pageName == QStringLiteral("机械臂与舵机")) {
            mechanismPage_ = new MechanismPage(pageStack_);
            page = mechanismPage_;
        } else {
            page = new QWidget(pageStack_);
        }
        page->setObjectName(pageName);
        if (page->layout() == nullptr) {
            auto *pageLayout = new QVBoxLayout(page);
            auto *pageLabel = new QLabel(pageName, page);
            pageLabel->setAlignment(Qt::AlignCenter);
            pageLayout->addWidget(pageLabel);
        }
        pageStack_->addWidget(page);
    }

    QObject::connect(navigationList, &QListWidget::currentRowChanged,
                     pageStack_, &QStackedWidget::setCurrentIndex);
    navigationList->setCurrentRow(0);

    contentLayout->addWidget(navigationList);
    contentLayout->addWidget(pageStack_, 1);
    mainLayout->addLayout(contentLayout, 1);

    setCentralWidget(centralWidget);
    setWindowTitle("机器人调试上位机");
    resize(1000, 700);

    connect(refreshPortsButton_, &QPushButton::clicked, this,
            &MainWindow::refreshPorts);
    connect(connectButton_, &QPushButton::clicked, this,
            &MainWindow::toggleConnection);
    connect(&serial_, &SerialController::opened, this,
            &MainWindow::handleSerialOpened);
    connect(&serial_, &SerialController::closed, this,
            &MainWindow::handleSerialClosed);
    connect(&serial_, &SerialController::serialError, this,
            &MainWindow::handleSerialError);
    connect(&device_, &DeviceClient::handshakeCompleted, this,
            [this](DeviceInfo) {
                setDeviceControlsEnabled(true);
                connectionStatusLabel_->setText(QStringLiteral("设备已握手"));
            });
    connect(&device_, &DeviceClient::deviceError, this,
            &MainWindow::handleDeviceError);
    connect(chassisPage_, &ChassisPage::readRequested, &device_,
            &DeviceClient::getParameterGroup);
    connect(chassisPage_, &ChassisPage::writeRequested, &device_,
            &DeviceClient::setParameterGroup);
    connect(mechanismPage_, &MechanismPage::readRequested, &device_,
            &DeviceClient::getParameterGroup);
    connect(mechanismPage_, &MechanismPage::writeRequested, &device_,
            &DeviceClient::setParameterGroup);
    connect(&device_, &DeviceClient::parameterGroupReceived, this,
            &MainWindow::handleParameterGroup);

    setDeviceControlsEnabled(false);
    refreshPorts();
}

void MainWindow::refreshPorts() {
    const QString previousPort = portCombo_->currentText();
    portCombo_->clear();
    portCombo_->addItems(serial_.availablePorts());
    const int previousIndex = portCombo_->findText(previousPort);
    if (previousIndex >= 0) {
        portCombo_->setCurrentIndex(previousIndex);
    }
}

void MainWindow::toggleConnection() {
    if (!connectButton_->isEnabled() || !portCombo_->isEnabled()) {
        serial_.close();
        return;
    }

    serial_.open(portCombo_->currentText(), baudCombo_->currentData().toInt());
}

void MainWindow::handleSerialOpened() {
    portCombo_->setEnabled(false);
    baudCombo_->setEnabled(false);
    refreshPortsButton_->setEnabled(false);
    connectButton_->setText(QStringLiteral("断开"));
    connectionStatusLabel_->setText(QStringLiteral("串口已连接，等待设备握手"));
    setDeviceControlsEnabled(false);
    device_.hello();
}

void MainWindow::handleSerialClosed() {
    portCombo_->setEnabled(true);
    baudCombo_->setEnabled(true);
    refreshPortsButton_->setEnabled(true);
    connectButton_->setText(QStringLiteral("连接"));
    connectionStatusLabel_->setText(QStringLiteral("未连接"));
    setDeviceControlsEnabled(false);
}

void MainWindow::handleSerialError(QString message) {
    connectionStatusLabel_->setText(std::move(message));
}

void MainWindow::handleDeviceError(QString message) {
    if (!device_.handshakeComplete()) {
        connectionStatusLabel_->setText(std::move(message));
    }
}

void MainWindow::handleParameterGroup(quint8 group,
                                      QVector<ParameterValue> values) {
    Q_UNUSED(group);
    if (chassisPage_ != nullptr) {
        chassisPage_->setValues(values);
    }
    if (mechanismPage_ != nullptr) {
        mechanismPage_->setValues(values);
    }
}

void MainWindow::setDeviceControlsEnabled(bool enabled) {
    if (chassisPage_ != nullptr) {
        chassisPage_->setConnected(enabled);
    }
    if (mechanismPage_ != nullptr) {
        mechanismPage_->setConnected(enabled);
    }
    if (pageStack_ != nullptr) {
        for (int index = 1; index < pageStack_->count(); ++index) {
            pageStack_->widget(index)->setEnabled(enabled);
        }
    }
    if (emergencyStopButton_ != nullptr) {
        emergencyStopButton_->setEnabled(enabled);
    }
}
