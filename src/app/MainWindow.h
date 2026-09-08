#pragma once

#include <QMainWindow>

#include "device/DeviceClient.h"
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

private:
    void setDeviceControlsEnabled(bool enabled);

    ProtocolClient protocol_;
    DeviceClient device_;
    SerialController serial_;

    QComboBox *portCombo_{};
    QComboBox *baudCombo_{};
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
    bool serialConnected_{};
};
