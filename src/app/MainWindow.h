#pragma once

#include <QMainWindow>
#include <QTimer>

#include "device/DeviceClient.h"
#include "device/MecanumJogClient.h"
#include "protocol/ProtocolClient.h"
#include "serial/SerialController.h"

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class ChassisPage;
class MechanismPage;
class OverviewPage;
class ImuPage;
class ActionTestPage;
class TerminalPage;
class VisionPage;
class FieldPositionPage;
class MecanumJogPage;
class MechanismActionPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshPorts();
    void toggleConnection();
    void handleSerialOpened();
    void handleSerialClosed();
    void handleSerialError(QString message);
    void handleDeviceError(QString message);
    void handleParameterGroup(quint8 group, QVector<ParameterValue> values);
    void handleTelemetryRateChanged(quint16 hz);
    void handleCalibrationRequested();
    void handleTestUnlockStateChanged(bool unlocked, qint64 remainingMs);
    void handleEmergencyStateChanged(bool locked);
    void requestClearEmergencyStop();
    void sendHeartbeat();

private:
    void setDeviceControlsEnabled(bool enabled);
    bool mecanumMode() const;
    void sendRawBytes(QByteArray bytes, bool showInLog = true);
    void stopAutomaticSending();

    ProtocolClient protocol_;
    DeviceClient device_;
    MecanumJogClient mecanum_;
    SerialController serial_;
    QTimer *heartbeatTimer_{};

    QComboBox *portCombo_{};
    QComboBox *baudCombo_{};
    QComboBox *deviceModeCombo_{};
    QPushButton *refreshPortsButton_{};
    QPushButton *connectButton_{};
    QLabel *connectionStatusLabel_{};
    QPushButton *emergencyStopButton_{};
    QPushButton *clearEmergencyStopButton_{};
    QStackedWidget *pageStack_{};
    OverviewPage *overviewPage_{};
    ChassisPage *chassisPage_{};
    ImuPage *imuPage_{};
    MechanismPage *mechanismPage_{};
    ActionTestPage *actionPage_{};
    TerminalPage *terminalPage_{};
    VisionPage *visionPage_{};
    FieldPositionPage *fieldPositionPage_{};
    MecanumJogPage *mecanumPage_{};
    MechanismActionPage *mechanismActionPage_{};
    bool serialConnected_{};
};
