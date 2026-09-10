#include "MainWindow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

#include <utility>

#include "pages/ActionTestPage.h"
#include "pages/ChassisPage.h"
#include "pages/FieldPositionPage.h"
#include "pages/ImuPage.h"
#include "pages/MechanismPage.h"
#include "pages/OverviewPage.h"
#include "pages/TerminalPage.h"
#include "pages/VisionPage.h"

namespace {

static const QStringList kPages = {
    "总览", "场地定位", "底盘与 PID", "机械臂与舵机", "HWT101", "动作测试",
    "串口终端", "视觉（预留）"
};

constexpr quint8 kDefaultTelemetryMask = 0x07;
constexpr quint16 kDefaultTelemetryPeriodMs = 100;

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      protocol_(this),
      device_(&protocol_, this),
      serial_(&protocol_, this) {
    heartbeatTimer_ = new QTimer(this);
    heartbeatTimer_->setObjectName(QStringLiteral("heartbeatTimer"));
    heartbeatTimer_->setInterval(250);
    heartbeatTimer_->setTimerType(Qt::CoarseTimer);
    connect(heartbeatTimer_, &QTimer::timeout, this,
            &MainWindow::sendHeartbeat);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *connectionBar = new QHBoxLayout;
    portCombo_ = new QComboBox(centralWidget);
    portCombo_->setObjectName("portCombo");
    baudCombo_ = new QComboBox(centralWidget);
    baudCombo_->setObjectName("baudCombo");
    baudCombo_->addItem(QStringLiteral("115200"), 115200);

    refreshPortsButton_ = new QPushButton("刷新串口", centralWidget);
    refreshPortsButton_->setObjectName("refreshPortsButton");
    connectButton_ = new QPushButton("连接", centralWidget);
    connectButton_->setObjectName("connectButton");
    connectionStatusLabel_ = new QLabel("未连接", centralWidget);
    connectionStatusLabel_->setObjectName("connectionStatusLabel");
    emergencyStopButton_ = new QPushButton("急停", centralWidget);
    emergencyStopButton_->setObjectName("emergencyStopButton");
    emergencyStopButton_->setEnabled(false);
    clearEmergencyStopButton_ =
        new QPushButton(QStringLiteral("解除急停"), centralWidget);
    clearEmergencyStopButton_->setObjectName(
        QStringLiteral("clearEmergencyStopButton"));
    clearEmergencyStopButton_->setEnabled(false);

    connectionBar->addWidget(portCombo_);
    connectionBar->addWidget(baudCombo_);
    connectionBar->addWidget(refreshPortsButton_);
    connectionBar->addWidget(connectionStatusLabel_);
    connectionBar->addStretch();
    connectionBar->addWidget(connectButton_);
    connectionBar->addWidget(emergencyStopButton_);
    connectionBar->addWidget(clearEmergencyStopButton_);
    mainLayout->addLayout(connectionBar);

    auto *contentLayout = new QHBoxLayout;
    auto *navigationList = new QListWidget(centralWidget);
    navigationList->setObjectName("navigationList");
    pageStack_ = new QStackedWidget(centralWidget);
    pageStack_->setObjectName("pageStack");

    for (const auto &pageName : kPages) {
        navigationList->addItem(pageName);

        QWidget *page = nullptr;
        if (pageName == QStringLiteral("总览")) {
            overviewPage_ = new OverviewPage(pageStack_);
            page = overviewPage_;
        } else if (pageName == QStringLiteral("场地定位")) {
            fieldPositionPage_ = new FieldPositionPage(pageStack_);
            page = fieldPositionPage_;
        } else if (pageName == QStringLiteral("底盘与 PID")) {
            chassisPage_ = new ChassisPage(pageStack_);
            page = chassisPage_;
        } else if (pageName == QStringLiteral("HWT101")) {
            imuPage_ = new ImuPage(pageStack_);
            page = imuPage_;
        } else if (pageName == QStringLiteral("机械臂与舵机")) {
            mechanismPage_ = new MechanismPage(pageStack_);
            page = mechanismPage_;
        } else if (pageName == QStringLiteral("动作测试")) {
            actionPage_ = new ActionTestPage(pageStack_);
            page = actionPage_;
        } else if (pageName == QStringLiteral("串口终端")) {
            terminalPage_ = new TerminalPage(pageStack_);
            page = terminalPage_;
        } else if (pageName == QStringLiteral("视觉（预留）")) {
            visionPage_ = new VisionPage(pageStack_);
            page = visionPage_;
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
    connect(emergencyStopButton_, &QPushButton::clicked, this, [this] {
        // Emergency stop is intentionally immediate and never asks for a
        // confirmation.  DeviceClient also locks actions before TX.
        device_.emergencyStop();
    });
    connect(clearEmergencyStopButton_, &QPushButton::clicked, this,
            &MainWindow::requestClearEmergencyStop);
    connect(&serial_, &SerialController::opened, this,
            &MainWindow::handleSerialOpened);
    connect(&serial_, &SerialController::closed, this,
            &MainWindow::handleSerialClosed);
    connect(&serial_, &SerialController::serialError, this,
            &MainWindow::handleSerialError);
    connect(&protocol_, &ProtocolClient::bytesReady, this,
            [this](QByteArray bytes) {
                if (terminalPage_ != nullptr) {
                    terminalPage_->appendTx(bytes);
                }
                if (fieldPositionPage_ != nullptr) {
                    fieldPositionPage_->appendSerialTx(bytes);
                }
            });
    connect(&serial_, &SerialController::bytesReceived, this,
            [this](QByteArray bytes) {
                if (terminalPage_ != nullptr) {
                    terminalPage_->appendRx(bytes);
                }
                if (fieldPositionPage_ != nullptr) {
                    fieldPositionPage_->appendSerialRx(bytes);
                }
            });
    connect(&protocol_, &ProtocolClient::responseReceived, this,
            [this](protocol::Frame frame) {
                if (terminalPage_ != nullptr) {
                    terminalPage_->appendDecodedFrame(frame);
                }
            });
    connect(&protocol_, &ProtocolClient::eventReceived, this,
            [this](protocol::Frame frame) {
                if (terminalPage_ != nullptr) {
                    terminalPage_->appendDecodedFrame(frame);
                }
            });
    const auto sendRaw = [this](QByteArray bytes) {
        if (serial_.write(QByteArrayView(bytes)) < 0) {
            return;
        }
        terminalPage_->appendTx(bytes);
        fieldPositionPage_->appendSerialTx(bytes);
    };
    connect(terminalPage_, &TerminalPage::rawSendRequested, this, sendRaw);
    connect(fieldPositionPage_, &FieldPositionPage::rawSendRequested, this,
            sendRaw);
    connect(&protocol_, &ProtocolClient::requestLatencyChanged, this,
            [this](qint64 latencyMs) {
                if (overviewPage_ != nullptr) {
                    overviewPage_->setLatency(latencyMs);
                }
            });
    connect(baudCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (imuPage_ != nullptr) {
                    imuPage_->setLinkBaudRate(baudCombo_->currentData().toInt());
                }
            });
    connect(&device_, &DeviceClient::handshakeCompleted, this,
            [this](DeviceInfo info) {
                setDeviceControlsEnabled(true);
                connectionStatusLabel_->setText(QStringLiteral("设备已握手"));
                if (overviewPage_ != nullptr) {
                    overviewPage_->setDeviceInfo(info);
                    overviewPage_->setLinkState(QStringLiteral("设备已握手"));
                }
                if (fieldPositionPage_ != nullptr) {
                    fieldPositionPage_->setPoseCapabilityAvailable(
                        (info.capabilities & protocol::Capability::Pose) != 0);
                    fieldPositionPage_->setConnectionState(
                        QStringLiteral("设备已握手"), true);
                }
                device_.getStatus();
                device_.setTelemetry(kDefaultTelemetryMask,
                                     kDefaultTelemetryPeriodMs);
            });
    connect(&device_, &DeviceClient::deviceError, this,
            &MainWindow::handleDeviceError);
    connect(&device_, &DeviceClient::testUnlockStateChanged, this,
            &MainWindow::handleTestUnlockStateChanged);
    connect(&device_, &DeviceClient::emergencyStateChanged, this,
            &MainWindow::handleEmergencyStateChanged);
    connect(&device_, &DeviceClient::imuSampleReceived, this,
            [this](ImuSample sample) {
                if (imuPage_ != nullptr) {
                    imuPage_->setImuSample(sample);
                }
                if (overviewPage_ != nullptr) {
                    overviewPage_->setImuSample(sample);
                }
                if (fieldPositionPage_ != nullptr) {
                    fieldPositionPage_->setImuSample(sample);
                }
            });
    connect(&device_, &DeviceClient::pidSampleReceived, this,
            [this](PidSample sample) {
                if (chassisPage_ != nullptr) {
                    chassisPage_->setPidSample(sample);
                }
                if (imuPage_ != nullptr) {
                    imuPage_->setPidSample(sample);
                }
                if (overviewPage_ != nullptr) {
                    overviewPage_->setPidSample(sample);
                }
            });
    connect(&device_, &DeviceClient::poseSampleReceived, this,
            [this](PoseSample sample) {
                if (fieldPositionPage_ != nullptr) {
                    fieldPositionPage_->setPoseSample(sample,
                                                      QStringLiteral("串口遥测"));
                }
            });
    connect(fieldPositionPage_, &FieldPositionPage::setPoseRequested,
            &device_, &DeviceClient::setPose);
    connect(&device_, &DeviceClient::statusReceived, this,
            [this](DeviceStatus status) {
                if (overviewPage_ != nullptr) {
                    overviewPage_->setStatus(status);
                }
                if (imuPage_ != nullptr) {
                    imuPage_->setStatus(status);
                }
                if (fieldPositionPage_ != nullptr) {
                    fieldPositionPage_->setDeviceStatus(status);
                }
            });
    connect(&device_, &DeviceClient::telemetryConfigured, this,
            [this](quint8 mask, quint16 periodMs) {
                if (imuPage_ != nullptr) {
                    imuPage_->setTelemetryConfiguration(mask, periodMs);
                }
            });
    connect(&device_, &DeviceClient::imuCalibrationStateChanged, this,
            [this](quint8 state) {
                if (imuPage_ != nullptr) {
                    imuPage_->setCalibrationState(state);
                }
            });
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
    connect(&device_, &DeviceClient::parameterGroupReadFailed, chassisPage_,
            &ChassisPage::clearPendingRead);
    connect(imuPage_, &ImuPage::telemetryRateChanged, this,
            &MainWindow::handleTelemetryRateChanged);
    connect(imuPage_, &ImuPage::calibrationRequested, this,
            &MainWindow::handleCalibrationRequested);
    connect(actionPage_, &ActionTestPage::unlockRequested, &device_,
            &DeviceClient::unlockTests);
    connect(actionPage_, &ActionTestPage::chassisRequested, &device_,
            &DeviceClient::testChassis);
    connect(actionPage_, &ActionTestPage::horizontalRequested, &device_,
            &DeviceClient::testHorizontal);
    connect(actionPage_, &ActionTestPage::liftRequested, &device_,
            &DeviceClient::testLift);
    connect(actionPage_, &ActionTestPage::turretRequested, &device_,
            &DeviceClient::testTurret);
    connect(actionPage_, &ActionTestPage::platformPositionRequested, &device_,
            &DeviceClient::setPlatformPosition);
    connect(actionPage_, &ActionTestPage::gripperRequested, &device_,
            &DeviceClient::setGripperOpen);
    connect(actionPage_, &ActionTestPage::servoRequested, &device_,
            &DeviceClient::testServo);
    connect(actionPage_, &ActionTestPage::stopRequested, &device_,
            &DeviceClient::stop);

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
    serialConnected_ = true;
    heartbeatTimer_->stop();
    portCombo_->setEnabled(false);
    baudCombo_->setEnabled(false);
    refreshPortsButton_->setEnabled(false);
    connectButton_->setText(QStringLiteral("断开"));
    connectionStatusLabel_->setText(QStringLiteral("串口已连接，等待设备握手"));
    if (overviewPage_ != nullptr) {
        overviewPage_->setLinkState(QStringLiteral("串口已连接，等待设备握手"));
        overviewPage_->setLatency(-1);
    }
    if (fieldPositionPage_ != nullptr) {
        fieldPositionPage_->setConnectionState(
            QStringLiteral("串口已连接，等待设备握手"), true);
    }
    setDeviceControlsEnabled(false);
    emergencyStopButton_->setEnabled(true);
    clearEmergencyStopButton_->setEnabled(false);
    device_.hello();
}

void MainWindow::handleSerialClosed() {
    serialConnected_ = false;
    heartbeatTimer_->stop();
    portCombo_->setEnabled(true);
    baudCombo_->setEnabled(true);
    refreshPortsButton_->setEnabled(true);
    connectButton_->setText(QStringLiteral("连接"));
    connectionStatusLabel_->setText(QStringLiteral("未连接"));
    if (overviewPage_ != nullptr) {
        overviewPage_->setLinkState(QStringLiteral("未连接"));
        overviewPage_->setLatency(-1);
    }
    if (fieldPositionPage_ != nullptr) {
        fieldPositionPage_->setPoseCapabilityAvailable(false);
        fieldPositionPage_->setConnectionState(QStringLiteral("未连接"), false);
    }
    setDeviceControlsEnabled(false);
    emergencyStopButton_->setEnabled(false);
    clearEmergencyStopButton_->setEnabled(false);
}

void MainWindow::handleSerialError(QString message) {
    connectionStatusLabel_->setText(std::move(message));
    if (overviewPage_ != nullptr) {
        overviewPage_->setLinkState(connectionStatusLabel_->text());
    }
    if (fieldPositionPage_ != nullptr) {
        fieldPositionPage_->setConnectionState(connectionStatusLabel_->text(),
                                               serialConnected_);
    }
}

void MainWindow::handleDeviceError(QString message) {
    if (overviewPage_ != nullptr) {
        overviewPage_->setDeviceError(message);
    }
    if (imuPage_ != nullptr) {
        imuPage_->setDeviceError(message);
    }
    if (actionPage_ != nullptr) {
        actionPage_->setDeviceError(message);
    }
    if (fieldPositionPage_ != nullptr) {
        fieldPositionPage_->setDeviceError(message);
    }
    if (!device_.handshakeComplete()) {
        heartbeatTimer_->stop();
        setDeviceControlsEnabled(false);
        const QString unavailable =
            QStringLiteral("未连接/不可用：%1").arg(message);
        connectionStatusLabel_->setText(unavailable);
        if (overviewPage_ != nullptr) {
            overviewPage_->setLinkState(QStringLiteral("未连接/不可用"));
        }
        if (fieldPositionPage_ != nullptr) {
            fieldPositionPage_->setConnectionState(unavailable,
                                                   serialConnected_);
        }
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
    if (imuPage_ != nullptr) {
        imuPage_->setConnected(enabled);
    }
    if (actionPage_ != nullptr) {
        actionPage_->setEnabled(enabled);
        actionPage_->setConnected(enabled);
        actionPage_->setEmergencyLocked(device_.emergencyLocked());
    }
    if (terminalPage_ != nullptr) {
        terminalPage_->setEnabled(serialConnected_);
        terminalPage_->setConnected(serialConnected_);
    }
    if (pageStack_ != nullptr) {
        for (int index = 1; index < pageStack_->count(); ++index) {
            if (pageStack_->widget(index) == fieldPositionPage_ ||
                pageStack_->widget(index) == actionPage_ ||
                pageStack_->widget(index) == terminalPage_) {
                continue;
            }
            pageStack_->widget(index)->setEnabled(enabled);
        }
    }
    if (emergencyStopButton_ != nullptr) {
        emergencyStopButton_->setEnabled(serialConnected_);
    }
    if (clearEmergencyStopButton_ != nullptr) {
        clearEmergencyStopButton_->setEnabled(
            serialConnected_ && enabled && device_.emergencyLocked());
    }
}

void MainWindow::handleTelemetryRateChanged(quint16 hz) {
    if (hz == 0) {
        return;
    }
    const quint16 periodMs = static_cast<quint16>(qMax<quint32>(
        1, 1000u / static_cast<quint32>(hz)));
    device_.setTelemetry(0x07, periodMs);
}

void MainWindow::handleCalibrationRequested() {
    device_.calibrateImu();
}

void MainWindow::handleTestUnlockStateChanged(bool unlocked,
                                              qint64 remainingMs) {
    if (unlocked && serialConnected_ && device_.handshakeComplete()) {
        if (!heartbeatTimer_->isActive()) {
            heartbeatTimer_->start();
        }
    } else {
        heartbeatTimer_->stop();
    }
    if (actionPage_ != nullptr) {
        actionPage_->setTestActionsEnabled(unlocked, remainingMs);
    }
}

void MainWindow::sendHeartbeat() {
    if (!serialConnected_ || !device_.handshakeComplete() ||
        !device_.testsUnlocked()) {
        heartbeatTimer_->stop();
        return;
    }
    device_.getStatus();
}

void MainWindow::handleEmergencyStateChanged(bool locked) {
    if (actionPage_ != nullptr) {
        actionPage_->setEmergencyLocked(locked);
    }
    if (fieldPositionPage_ != nullptr) {
        fieldPositionPage_->setEmergencyLocked(locked);
    }
    if (clearEmergencyStopButton_ != nullptr) {
        clearEmergencyStopButton_->setEnabled(
            serialConnected_ && device_.handshakeComplete() && locked);
    }
    if (locked) {
        connectionStatusLabel_->setText(QStringLiteral("急停锁定"));
    } else if (device_.handshakeComplete()) {
        connectionStatusLabel_->setText(QStringLiteral("设备已握手"));
    }
}

void MainWindow::requestClearEmergencyStop() {
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("确认解除急停"),
        QStringLiteral("确认设备已静止并解除急停锁定吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        device_.clearEmergencyStop();
    }
}
