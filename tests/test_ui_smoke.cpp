#include <QApplication>
#include <QAbstractSpinBox>
#include <QByteArrayView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

#include <iostream>
#include <utility>

#include "app/MainWindow.h"
#include "device/DeviceClient.h"
#include "device/MecanumJogClient.h"
#include "pages/ActionTestPage.h"
#include "pages/ChassisPage.h"
#include "pages/FieldPositionPage.h"
#include "pages/ImuPage.h"
#include "pages/MechanismPage.h"
#include "pages/MecanumJogPage.h"
#include "pages/OverviewPage.h"
#include "pages/TerminalPage.h"
#include "pages/VisionPage.h"
#include "protocol/FrameCodec.h"
#include "protocol/FrameParser.h"
#include "serial/SerialController.h"
#include "widgets/TelemetryPlot.h"
#include "widgets/SerialDebugPanel.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

void waitFor(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

void acceptNextConfirmation() {
    QTimer::singleShot(0, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box != nullptr) {
            if (auto *yesButton = box->button(QMessageBox::Yes);
                yesButton != nullptr) {
                yesButton->click();
            } else {
                box->done(QMessageBox::Yes);
            }
        }
    });
}

void rejectNextConfirmation() {
    QTimer::singleShot(0, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box != nullptr) {
            if (auto *noButton = box->button(QMessageBox::No);
                noButton != nullptr) {
                noButton->click();
            } else {
                box->done(QMessageBox::No);
            }
        }
    });
}

protocol::Frame capturedFrame(const QByteArray &bytes) {
    FrameParser parser;
    const QVector<protocol::Frame> frames =
        parser.push(QByteArrayView(bytes));
    return frames.isEmpty() ? protocol::Frame{} : frames.front();
}

QVector<ParameterValue> pidPageValues(int firstIndex, int count) {
    QVector<ParameterValue> values;
    values.reserve(count);
    for (int index = 0; index < count; ++index) {
        const int parameterIndex = firstIndex + index;
        const int profile = parameterIndex / 5;
        const int offset = parameterIndex % 5;
        const quint16 id = static_cast<quint16>(
            0x1000 + (parameterIndex / 5) * 0x10 + parameterIndex % 5);
        const float value = offset == 0   ? static_cast<float>(profile + 1)
                            : offset == 1 ? 0.5f + profile * 0.1f
                            : offset == 2 ? static_cast<float>(profile + 2)
                            : offset == 3 ? static_cast<float>(10 + profile)
                                           : static_cast<float>(20 + profile);
        values.push_back({id, ValueType::Float32, QVariant(value)});
    }
    return values;
}

}  // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow window;
    auto *nav = window.findChild<QListWidget *>("navigationList");
    auto *portCombo = window.findChild<QComboBox *>("portCombo");
    auto *baudCombo = window.findChild<QComboBox *>("baudCombo");
    auto *deviceModeCombo = window.findChild<QComboBox *>("deviceModeCombo");
    auto *refreshPortsButton = window.findChild<QPushButton *>("refreshPortsButton");
    auto *connectButton = window.findChild<QPushButton *>("connectButton");
    auto *connectionStatusLabel =
        window.findChild<QLabel *>("connectionStatusLabel");
    auto *stop = window.findChild<QPushButton *>("emergencyStopButton");
    auto *clearStop =
        window.findChild<QPushButton *>("clearEmergencyStopButton");
    auto *tuningPage = window.findChild<QWidget *>("底盘与 PID");
    auto *mechanismPage = window.findChild<QWidget *>("机械臂与舵机");
    auto *actionPage = window.findChild<QWidget *>("动作测试");
    auto *actionUnlock = window.findChild<QPushButton *>("testUnlockButton");
    auto *actionChassis = window.findChild<QPushButton *>("testChassisButton");
    auto *actionChassisVx =
        window.findChild<QSpinBox *>("testChassisVxSpinBox");
    auto *actionHorizontalTarget =
        window.findChild<QDoubleSpinBox *>("testHorizontalTargetSpinBox");
    auto *servo2Slider = window.findChild<QSlider *>("servo2Slider");
    auto *servo2Angle =
        window.findChild<QSpinBox *>("servo2AngleSpinBox");
    auto *servo2Send =
        window.findChild<QPushButton *>("servo2SendButton");
    auto *servo3Slider = window.findChild<QSlider *>("servo3Slider");
    auto *servo3Angle =
        window.findChild<QSpinBox *>("servo3AngleSpinBox");
    auto *servo3Send =
        window.findChild<QPushButton *>("servo3SendButton");
    auto *servo4Slider = window.findChild<QSlider *>("servo4Slider");
    auto *servo4Angle =
        window.findChild<QSpinBox *>("servo4AngleSpinBox");
    auto *servo4Send =
        window.findChild<QPushButton *>("servo4SendButton");
    auto *servoLog = window.findChild<QListWidget *>("servoActionLog");
    auto *terminalPage = window.findChild<TerminalPage *>();
    auto *terminalMode =
        window.findChild<QComboBox *>("terminalDisplayModeCombo");
    auto *terminalLog =
        window.findChild<QPlainTextEdit *>("terminalLogTextEdit");
    auto *fieldSerialLog =
        window.findChild<QPlainTextEdit *>("fieldSerialLogTextEdit");
    auto *fieldSerialMode =
        window.findChild<QComboBox *>("fieldSerialDisplayModeCombo");
    auto *terminalInput =
        window.findChild<QLineEdit *>("terminalInputLineEdit");
    auto *terminalSend =
        window.findChild<QPushButton *>("terminalSendButton");
    auto *terminalClear =
        window.findChild<QPushButton *>("terminalClearButton");
    auto *terminalPause =
        window.findChild<QPushButton *>("terminalPauseButton");
    auto *terminalNotice =
        window.findChild<QLabel *>("terminalBypassNotice");
    auto *visionPage = window.findChild<QWidget *>("视觉（预留）");
    auto *fieldPositionPage = window.findChild<FieldPositionPage *>("场地定位");
    auto *mecanumPage = window.findChild<MecanumJogPage *>("临时调试");
    auto *visionPlaceholder =
        window.findChild<QLabel *>("visionPlaceholderLabel");
    auto *visionUsart1 = window.findChild<QLabel *>("visionUsart1Label");
    auto *pidProfile = window.findChild<QComboBox *>("pidProfileCombo");
    auto *pidKp = window.findChild<QDoubleSpinBox *>("pidKpSpinBox");
    auto *pidWrite = window.findChild<QPushButton *>("pidWriteButton");
    auto *pidAnglePlot =
        window.findChild<TelemetryPlot *>("pidAnglePlot");
    auto *pidOutputPlot =
        window.findChild<TelemetryPlot *>("pidOutputPlot");
    auto *chassisRead = window.findChild<QPushButton *>("chassisReadButton");
    auto *mechanismWrite =
        window.findChild<QPushButton *>("mechanismWriteButton");
    auto *imuRate = window.findChild<QAbstractSpinBox *>("imuRateSpinBox");
    auto *ramOnlyNotice = window.findChild<QLabel *>("ramOnlyNotice");
    auto *imuPage = window.findChild<ImuPage *>();
    auto *overviewPage = window.findChild<QWidget *>("总览");
    auto *overviewLink =
        window.findChild<QLabel *>("overviewLinkStateLabel");
    auto *overviewLatency = window.findChild<QLabel *>("latencyLabel");
    auto *imuTelemetryRate =
        window.findChild<QSpinBox *>("imuTelemetryRateSpinBox");
    auto *protocol = window.findChild<ProtocolClient *>();
    auto *device = window.findChild<DeviceClient *>();
    auto *serialController = window.findChild<SerialController *>();
    auto *heartbeatTimer = window.findChild<QTimer *>("heartbeatTimer");
    if (!require(nav && nav->count() == 9, "navigation pages changed") ||
        !require(fieldPositionPage && fieldPositionPage->isEnabled() &&
                     window.findChild<FieldMapWidget *>("fieldMapWidget"),
                 "field position navigation page is missing or disabled") ||
        !require(portCombo && baudCombo && deviceModeCombo &&
                     deviceModeCombo->count() == 2 &&
                     deviceModeCombo->currentData().toInt() == 1 &&
                     refreshPortsButton && connectButton,
                 "connection controls are missing or text mode is not the default") ||
        !require(mecanumPage && !mecanumPage->isEnabled(),
                 "temporary mecanum page must start disabled") ||
        !require(connectionStatusLabel && stop && !stop->isEnabled(),
                 "connection status controls are invalid") ||
        !require(clearStop && !clearStop->isEnabled(),
                 "clear emergency control must start disabled") ||
        !require(tuningPage && !tuningPage->isEnabled() && mechanismPage &&
                     !mechanismPage->isEnabled(),
                 "parameter pages must be disabled before handshake") ||
        !require(actionPage && !actionPage->isEnabled(),
                 "action page must stay disabled before handshake") ||
        !require(servo2Slider && servo2Angle && servo2Send &&
                     servo3Slider && servo3Angle && servo3Send &&
                     servo4Slider && servo4Angle && servo4Send && servoLog,
                 "servo controls or target log are missing") ||
        !require(servo2Slider->minimum() == 0 &&
                     servo2Slider->maximum() == 270 &&
                     servo2Angle->minimum() == 0 &&
                     servo2Angle->maximum() == 270 &&
                     servo3Slider->minimum() == 0 &&
                     servo3Slider->maximum() == 270 &&
                     servo3Angle->minimum() == 0 &&
                     servo3Angle->maximum() == 270 &&
                     servo4Slider->minimum() == 0 &&
                     servo4Slider->maximum() == 360 &&
                     servo4Angle->minimum() == 0 &&
                     servo4Angle->maximum() == 360,
                 "servo angle ranges are invalid") ||
        !require(!servo2Slider->isEnabled() && !servo2Angle->isEnabled() &&
                     !servo2Send->isEnabled() && !servo3Slider->isEnabled() &&
                     !servo3Angle->isEnabled() && !servo3Send->isEnabled() &&
                     !servo4Slider->isEnabled() && !servo4Angle->isEnabled() &&
                     !servo4Send->isEnabled(),
                 "servo controls must start disabled") ||
        !require(actionUnlock && actionChassis && !actionUnlock->isEnabled() &&
                      !actionChassis->isEnabled() && actionChassisVx &&
                      !actionChassisVx->isEnabled() && actionHorizontalTarget &&
                      !actionHorizontalTarget->isEnabled(),
                  "action controls must start disabled") ||
        !require(terminalPage && terminalMode && terminalLog && fieldSerialMode &&
                      terminalInput &&
                      terminalSend && terminalClear && terminalPause &&
                      terminalNotice &&
                      terminalNotice->text().contains(
                          QStringLiteral("绕过请求跟踪")),
                  "terminal page controls or safety notice are missing") ||
        !require(visionPage && visionPlaceholder && visionUsart1 &&
                      visionPlaceholder->text().contains(QStringLiteral("K230")) &&
                      visionPlaceholder->text().contains(QStringLiteral("MaixCAM")) &&
                      visionUsart1->text().contains(QStringLiteral("USART1")),
                  "vision placeholder or USART1 description is missing") ||
        !require(imuPage && !imuPage->isEnabled() && overviewPage &&
                     overviewLink && overviewLatency &&
                     overviewLink->text() == QStringLiteral("未连接"),
                 "overview or IMU telemetry page is not wired") ||
        !require(imuTelemetryRate && imuTelemetryRate->minimum() == 1 &&
                     imuTelemetryRate->maximum() == 50 &&
                     !imuTelemetryRate->isEnabled(),
                 "IMU telemetry rate control is invalid") ||
        !require(baudCombo->count() == 1 &&
                     baudCombo->currentData().toInt() == 115200 &&
                     connectionStatusLabel->text() == QStringLiteral("未连接"),
                 "connection defaults changed") ||
        !require(terminalMode && terminalMode->currentIndex() == 1,
                 "terminal display does not default to ASCII") ||
        !require(pidProfile && pidProfile->count() == 5,
                 "PID profile selector must expose five profiles") ||
        !require(pidKp && pidKp->minimum() == 0.0 && pidKp->maximum() == 20.0,
                 "PID Kp bounds must come from the catalog") ||
        !require(pidAnglePlot && pidOutputPlot,
                 "PID tuning plots are missing") ||
        !require(pidWrite && !pidWrite->isEnabled() && chassisRead &&
                     !chassisRead->isEnabled() && mechanismWrite &&
                     !mechanismWrite->isEnabled() && imuRate &&
                     !imuRate->isEnabled(),
                 "parameter controls must be disabled before handshake") ||
        !require(ramOnlyNotice &&
                     ramOnlyNotice->text().contains(QStringLiteral("RAM")) &&
                     ramOnlyNotice->text().contains(QStringLiteral("Flash")),
                 "RAM-only notice is missing") ||
        !require(protocol && device && serialController && heartbeatTimer &&
                     heartbeatTimer->interval() <= 500,
                 "window protocol or heartbeat service is missing")) {
        return 1;
    }

    terminalMode->setCurrentIndex(0);
    fieldSerialMode->setCurrentIndex(0);
    emit protocol->bytesReady(QByteArray::fromHex("12 34"));
    emit serialController->bytesReceived(QByteArray::fromHex("ca fe"));
    if (!require(fieldSerialLog &&
                     terminalLog->toPlainText().contains(QStringLiteral("12 34")) &&
                     fieldSerialLog->toPlainText().contains(QStringLiteral("12 34")) &&
                     terminalLog->toPlainText().contains(QStringLiteral("CA FE")) &&
                     fieldSerialLog->toPlainText().contains(QStringLiteral("CA FE")),
                 "main window did not fan out the shared serial stream")) {
        return 1;
    }
    terminalLog->clear();
    fieldSerialLog->clear();

    terminalPage->appendTx(QByteArray::fromHex("aa 55"));
    terminalPage->appendRx(QByteArray("OK"));
    if (!require(terminalLog->toPlainText().contains(QStringLiteral("TX")) &&
                     terminalLog->toPlainText().contains(QStringLiteral("RX")) &&
                     terminalLog->toPlainText().contains(QStringLiteral("AA 55")),
                 "terminal did not render timestamped HEX TX/RX records")) {
        return 1;
    }
    terminalMode->setCurrentIndex(1);
    terminalPage->appendRx(QByteArray("ASCII"));
    if (!require(terminalLog->toPlainText().contains(QStringLiteral("ASCII")),
                 "terminal ASCII display mode is not active")) {
        return 1;
    }
    QByteArray rawBytes;
    QObject::connect(terminalPage, &TerminalPage::rawSendRequested,
                     [&](QByteArray bytes) { rawBytes = std::move(bytes); });
    terminalPage->setEnabled(true);
    terminalPage->setConnected(true);
    terminalMode->setCurrentIndex(0);
    terminalInput->setText(QStringLiteral("DE AD"));
    terminalSend->click();
    if (!require(rawBytes == QByteArray::fromHex("de ad"),
                 "terminal manual HEX send did not emit raw bytes")) {
        return 1;
    }
    terminalClear->click();
    if (!require(terminalLog->toPlainText().isEmpty(),
                 "terminal clear button did not clear the log")) {
        return 1;
    }
    terminalPage->appendRx(QByteArray("before pause"));
    terminalPause->click();
    terminalPage->appendRx(QByteArray("hidden while paused"));
    if (!require(!terminalLog->toPlainText().contains(
                     QStringLiteral("hidden while paused")),
                 "terminal pause button did not suppress display updates")) {
        return 1;
    }
    terminalPause->click();

    deviceModeCombo->setCurrentIndex(0);
    QByteArray helloRequestBytes;
    QVector<QByteArray> telemetryRequests;
    bool heartbeatResponsesEnabled = false;
    QObject::connect(protocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         const protocol::Frame frame = capturedFrame(bytes);
                         if (frame.command == static_cast<quint8>(
                                 protocol::Command::Hello)) {
                             helloRequestBytes = bytes;
                         } else if (frame.command == static_cast<quint8>(
                                        protocol::Command::SetTelemetry)) {
                             telemetryRequests.push_back(bytes);
                             protocol::Frame telemetryResponse = frame;
                             telemetryResponse.flags = protocol::Response;
                             telemetryResponse.payload = frame.payload;
                             protocol->ingestBytes(QByteArrayView(
                                 encodeFrame(telemetryResponse)));
                         } else if (!heartbeatResponsesEnabled &&
                                    frame.command == static_cast<quint8>(
                                        protocol::Command::GetStatus)) {
                             protocol::Frame statusResponse = frame;
                             statusResponse.flags = protocol::Response;
                             statusResponse.payload =
                                 QByteArray::fromHex("01 00 00 00 00 00");
                             protocol->ingestBytes(QByteArrayView(
                                 encodeFrame(statusResponse)));
                         }
                     });
    if (!require(QMetaObject::invokeMethod(&window, "handleSerialOpened",
                                           Qt::DirectConnection),
                 "serial-open handler could not be invoked")) {
        return 1;
    }
    const protocol::Frame helloRequest = capturedFrame(helloRequestBytes);
    protocol::Frame helloResponse = helloRequest;
    helloResponse.flags = protocol::Response;
    helloResponse.payload = QByteArray::fromHex("01 02 03 04 44 33 22 11");
    protocol->ingestBytes(QByteArrayView(encodeFrame(helloResponse)));
    if (!require(!telemetryRequests.isEmpty(),
                 "successful HELLO did not subscribe telemetry automatically")) {
        return 1;
    }
    if (!require(actionPage->isEnabled() && actionUnlock->isEnabled() &&
                      !actionChassis->isEnabled() &&
                      !actionChassisVx->isEnabled() &&
                      !actionHorizontalTarget->isEnabled(),
                 "handshake enabled unsafe action controls before unlock")) {
        return 1;
    }
    QByteArray unlockRequestBytes;
    QObject::connect(protocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         const protocol::Frame frame = capturedFrame(bytes);
                         if (frame.command == static_cast<quint8>(
                                 protocol::Command::TestUnlock)) {
                             unlockRequestBytes = bytes;
                         }
                     });
    actionUnlock->click();
    const protocol::Frame unlockRequest = capturedFrame(unlockRequestBytes);
    protocol::Frame unlockResponse = unlockRequest;
    unlockResponse.flags = protocol::Response;
    unlockResponse.payload = QByteArray::fromHex("30 75");
    protocol->ingestBytes(QByteArrayView(encodeFrame(unlockResponse)));
    if (!require(actionChassis->isEnabled() && actionChassisVx->isEnabled() &&
                      actionHorizontalTarget->isEnabled(),
                 "successful TEST_UNLOCK did not enable action controls")) {
        return 1;
    }

    int heartbeatCount = 0;
    QVector<qint64> heartbeatTimes;
    QElapsedTimer heartbeatClock;
    heartbeatClock.start();
    QObject::connect(protocol, &ProtocolClient::bytesReady,
                     [&](const QByteArray &bytes) {
                         const protocol::Frame request = capturedFrame(bytes);
                         if (request.command != static_cast<quint8>(
                                 protocol::Command::GetStatus)) {
                             return;
                         }
                         ++heartbeatCount;
                         heartbeatTimes.push_back(heartbeatClock.elapsed());
                         protocol::Frame statusResponse = request;
                         statusResponse.flags = protocol::Response;
                          statusResponse.payload =
                              QByteArray::fromHex("01 00 01 00 00 00");
                          protocol->ingestBytes(QByteArrayView(
                              encodeFrame(statusResponse)));
                      });
    heartbeatResponsesEnabled = true;
    QEventLoop heartbeatLoop;
    QTimer::singleShot(620, &heartbeatLoop, &QEventLoop::quit);
    heartbeatLoop.exec();
    if (!require(heartbeatCount >= 2,
                 "unlocked handshaken UI did not send periodic GET_STATUS heartbeats")) {
        return 1;
    }
    for (int index = 1; index < heartbeatTimes.size(); ++index) {
        if (!require(heartbeatTimes[index] - heartbeatTimes[index - 1] <= 500,
                     "heartbeat interval exceeded 500 ms")) {
            return 1;
        }
    }
    if (!require(heartbeatTimer->isActive(),
                 "heartbeat timer was not active while device was unlocked")) {
        return 1;
    }
    device->emergencyStop();
    if (!require(!actionChassis->isEnabled() && !actionChassisVx->isEnabled() &&
                      !actionHorizontalTarget->isEnabled() &&
                      !servo2Slider->isEnabled() && !servo2Angle->isEnabled() &&
                      !servo2Send->isEnabled() && !servo3Slider->isEnabled() &&
                      !servo3Angle->isEnabled() && !servo3Send->isEnabled() &&
                      !servo4Slider->isEnabled() && !servo4Angle->isEnabled() &&
                      !servo4Send->isEnabled() &&
                      !heartbeatTimer->isActive(),
                  "emergency stop did not disable action controls or heartbeat")) {
        return 1;
    }
    const protocol::Frame defaultTelemetry =
        capturedFrame(telemetryRequests.back());
    if (!require(defaultTelemetry.payload == QByteArray::fromHex("07 64 00"),
                 "default telemetry subscription must cover IMU/PID/status at 10 Hz")) {
        return 1;
    }

    protocol::Frame telemetryResponse = defaultTelemetry;
    telemetryResponse.flags = protocol::Response;
    telemetryResponse.payload = QByteArray::fromHex("07 64 00");
    protocol->ingestBytes(QByteArrayView(encodeFrame(telemetryResponse)));
    if (!require(overviewLatency->text().contains(QStringLiteral("ms")),
                 "overview did not display the latest response latency")) {
        return 1;
    }

    protocol->clearPending();
    if (!require(!device->handshakeComplete() && !imuPage->isEnabled() &&
                     !tuningPage->isEnabled() &&
                     connectionStatusLabel->text().contains(
                         QStringLiteral("不可用")),
                 "a post-handshake request failure did not lock controls")) {
        return 1;
    }
    const int heartbeatCountBeforeClose = heartbeatCount;
    if (!require(QMetaObject::invokeMethod(&window, "handleSerialClosed",
                                           Qt::DirectConnection),
                 "serial-close handler could not be invoked")) {
        return 1;
    }
    QEventLoop closedHeartbeatLoop;
    QTimer::singleShot(320, &closedHeartbeatLoop, &QEventLoop::quit);
    closedHeartbeatLoop.exec();
    if (!require(!heartbeatTimer->isActive() &&
                     heartbeatCount == heartbeatCountBeforeClose,
                 "disconnected UI continued sending heartbeats")) {
        return 1;
    }

    ChassisPage chassis;
    auto *pageRead = chassis.findChild<QPushButton *>("chassisReadButton");
    auto *pageWrite = chassis.findChild<QPushButton *>("chassisWriteButton");
    auto *pidSelector = chassis.findChild<QComboBox *>("pidProfileCombo");
    auto *pidControl = chassis.findChild<QDoubleSpinBox *>("pidKpSpinBox");
    auto *pidPageWrite = chassis.findChild<QPushButton *>("pidWriteButton");
    auto *vx = chassis.findChild<QSpinBox *>("chassisVxSpinBox");
    auto *restore = chassis.findChild<QPushButton *>("restoreInitialButton");
    auto *standalonePidAnglePlot =
        chassis.findChild<TelemetryPlot *>("pidAnglePlot");
    auto *standalonePidOutputPlot =
        chassis.findChild<TelemetryPlot *>("pidOutputPlot");
    int readGroup = 0;
    int writeGroup = 0;
    QVector<ParameterValue> writtenValues;
    QObject::connect(&chassis, &ChassisPage::readRequested,
                     [&](quint8 group) { readGroup = group; });
    QObject::connect(&chassis, &ChassisPage::writeRequested,
                     [&](quint8 group, QVector<ParameterValue> values) {
                         writeGroup = group;
                         writtenValues = std::move(values);
                     });
    if (!require(pageRead && pageWrite && pidSelector && pidControl &&
                     pidPageWrite && vx && restore && !pageRead->isEnabled() &&
                     !pageWrite->isEnabled() && standalonePidAnglePlot &&
                     standalonePidOutputPlot,
                 "standalone parameter page is not locked before handshake")) {
        return 1;
    }
    PidSample tuningSample;
    tuningSample.timestampMs = 100;
    tuningSample.targetDegrees = 30.0;
    tuningSample.actualDegrees = 27.5;
    tuningSample.output = 12.0;
    chassis.setPidSample(tuningSample);
    if (!require(standalonePidAnglePlot->sampleCount() == 1 &&
                     standalonePidOutputPlot->sampleCount() == 1,
                 "PID sample was not appended to both tuning plots")) {
        return 1;
    }
    chassis.setConnected(false);
    if (!require(standalonePidAnglePlot->sampleCount() == 0 &&
                     standalonePidOutputPlot->sampleCount() == 0,
                 "disconnect did not clear PID tuning plots")) {
        return 1;
    }
    chassis.setConnected(true);
    if (!require(pageRead->isEnabled() && pageWrite->isEnabled(),
                 "handshake did not enable parameter actions")) {
        return 1;
    }

    ChassisPage pidAggregationPage;
    auto *pidAggregationRead =
        pidAggregationPage.findChild<QPushButton *>("pidReadButton");
    auto *pidAggregationRestore =
        pidAggregationPage.findChild<QPushButton *>("restoreInitialButton");
    auto *pidAggregationControl =
        pidAggregationPage.findChild<QDoubleSpinBox *>("pidKpSpinBox");
    if (!require(pidAggregationRead && pidAggregationRestore &&
                     pidAggregationControl,
                 "PID aggregation page controls are missing")) {
        return 1;
    }
    pidAggregationPage.setConnected(true);
    pidAggregationRead->click();
    pidAggregationPage.setValues(pidPageValues(0, 18));
    if (!require(!pidAggregationRestore->isEnabled() &&
                     pidAggregationControl->value() == 0.0,
                 "PID page zero was captured or refreshed prematurely")) {
        return 1;
    }
    pidAggregationPage.setValues({
        {0x2000, ValueType::Int32, QVariant(qint32(12))},
    });
    if (!require(!pidAggregationRestore->isEnabled() &&
                     pidAggregationControl->value() == 0.0,
                 "an unrelated group refreshed an incomplete PID read")) {
        return 1;
    }
    pidAggregationPage.clearPendingRead(0x10, QStringLiteral("分页错误"));
    if (!require(!pidAggregationRestore->isEnabled(),
                 "failed PID read left the page initial snapshot enabled")) {
        return 1;
    }
    int retryPidReadCount = 0;
    QObject::connect(&pidAggregationPage, &ChassisPage::readRequested,
                     [&](quint8 group) {
                         if (group == 0x10) {
                             ++retryPidReadCount;
                         }
                     });
    pidAggregationRead->click();
    if (!require(retryPidReadCount == 1,
                 "PID read could not be retried after a pagination error")) {
        return 1;
    }
    pidAggregationPage.setValues(pidPageValues(0, 18));
    if (!require(!pidAggregationRestore->isEnabled() &&
                     pidAggregationControl->value() == 0.0,
                 "PID page zero after retry refreshed prematurely")) {
        return 1;
    }
    pidAggregationPage.setValues(pidPageValues(18, 7));
    if (!require(pidAggregationRestore->isEnabled() &&
                     pidAggregationControl->value() == 1.0,
                 "complete PID pages did not refresh the page atomically")) {
        return 1;
    }

    chassis.setValues({
        {0x2000, ValueType::Int32, QVariant(qint32(12))},
        {0x2001, ValueType::Int32, QVariant(qint32(-4))},
        {0x2002, ValueType::Int32, QVariant(qint32(7))},
        {0x2003, ValueType::UInt16, QVariant(quint16(250))},
        {0x2004, ValueType::UInt16, QVariant(quint16(15))},
    });
    if (!require(!restore->isEnabled(),
                 "an unsolicited chassis response created connection initial values")) {
        return 1;
    }
    chassis.setConnected(false);
    chassis.setValues({
        {0x2000, ValueType::Int32, QVariant(qint32(12))},
    });
    if (!require(!restore->isEnabled(),
                 "a disconnected late chassis response created connection initial values")) {
        return 1;
    }
    chassis.setConnected(true);
    chassis.setValues({
        {0x1000, ValueType::Float32, QVariant(1.5)},
        {0x1001, ValueType::Float32, QVariant(0.25)},
        {0x1002, ValueType::Float32, QVariant(0.5)},
        {0x1003, ValueType::Float32, QVariant(4.0)},
        {0x1004, ValueType::Float32, QVariant(20.0)},
    });
    pidSelector->setCurrentIndex(1);
    if (!require(pidControl->value() == 0.0,
                 "switching PID profiles copied the previous profile values")) {
        return 1;
    }
    pidControl->setValue(2.5);
    pidPageWrite->click();
    if (!require(writeGroup == 0x10 && !writtenValues.isEmpty() &&
                     writtenValues.front().id == 0x1010,
                 "PID write did not use the selected profile ID range")) {
        return 1;
    }
    pidSelector->setCurrentIndex(0);
    if (!require(pidControl->value() == 1.5,
                 "switching back to PID profile 0 lost its readback value")) {
        return 1;
    }
    chassis.setConnected(false);
    chassis.setConnected(true);
    pageWrite->click();
    if (!require(writeGroup == 0x20 && writtenValues.size() == 5,
                 "an enabled page did not emit a chassis RAM write")) {
        return 1;
    }
    const QVector<ParameterValue> initialChassisValues = {
        {0x2000, ValueType::Int32, QVariant(qint32(12))},
        {0x2001, ValueType::Int32, QVariant(qint32(-4))},
        {0x2002, ValueType::Int32, QVariant(qint32(7))},
        {0x2003, ValueType::UInt16, QVariant(quint16(250))},
        {0x2004, ValueType::UInt16, QVariant(quint16(15))},
    };
    chassis.setValues(initialChassisValues);
    if (!require(!restore->isEnabled(),
                 "a write response was incorrectly treated as connection initial values")) {
        return 1;
    }
    writeGroup = 0;
    writtenValues.clear();
    pageRead->click();
    if (!require(readGroup == 0x20, "read button emitted the wrong group")) {
        return 1;
    }
    chassis.setValues(initialChassisValues);
    vx->setValue(30);
    if (!require(restore->isEnabled(),
                 "a successful read did not enable the connection snapshot") ) {
        return 1;
    }
    restore->click();
    if (!require(vx->value() == 12,
                 "restoring connection initial values changed the wrong value") ||
        !require(writeGroup == 0 && writtenValues.isEmpty(),
                 "restoring values transmitted without an explicit write")) {
        return 1;
    }
    pageWrite->click();
    if (!require(writeGroup == 0x20 && writtenValues.size() == 5,
                 "RAM write did not emit the complete chassis group")) {
        return 1;
    }

    MechanismPage mechanism;
    auto *horizontalUnit =
        mechanism.findChild<QLabel *>("horizontalPositionSpinBoxUnit");
    auto *mechanismPageWrite =
        mechanism.findChild<QPushButton *>("mechanismWriteButton");
    auto *mechanismRead =
        mechanism.findChild<QPushButton *>("mechanismReadButton");
    auto *mechanismRestore =
        mechanism.findChild<QPushButton *>("mechanismRestoreInitialButton");
    if (!require(horizontalUnit && horizontalUnit->text() == QStringLiteral("mm") &&
                     mechanismPageWrite && !mechanismPageWrite->isEnabled() &&
                     mechanismRead && mechanismRestore,
                 "mechanism units or handshake lock are missing")) {
        return 1;
    }

    ImuPage imu;
    auto *imuRateControl =
        imu.findChild<QSpinBox *>("imuTelemetryRateSpinBox");
    auto *imuError = imu.findChild<QLabel *>("imuTelemetryErrorLabel");
    auto *imuAcceleration =
        imu.findChild<QLabel *>("imuAccelerationXLabel");
    auto *accelerationPlot = imu.findChild<TelemetryPlot *>("accelerationPlot");
    int requestedRate = 0;
    QObject::connect(&imu, &ImuPage::telemetryRateChanged,
                     [&](quint16 rate) { requestedRate = rate; });
    if (!require(imuRateControl && imuError && imuAcceleration &&
                     accelerationPlot && !imuRateControl->isEnabled(),
                 "standalone IMU page controls are invalid") ) {
        return 1;
    }
    imu.setLinkBaudRate(9600);
    if (!require(imuRateControl->maximum() == 50,
                 "IMU rate range was not kept at 50 Hz")) {
        return 1;
    }
    imu.setTelemetryConfiguration(0x07, 25);
    if (!require(imu.findChild<QLabel *>("imuTelemetryRateStatusLabel")
                     ->text()
                     .contains(QStringLiteral("40 Hz")),
                 "IMU page did not display the device's actual rate")) {
        return 1;
    }
    imu.setConnected(true);
    imuRateControl->setValue(10);
    if (!require(requestedRate == 10,
                 "IMU rate control did not emit the requested frequency")) {
        return 1;
    }
    ImuSample imuSample;
    imuSample.timestampMs = 1234;
    imuSample.accelerationX = 1.25;
    imuSample.accelerationY = -2.5;
    imuSample.accelerationZ = 0.5;
    imuSample.angularVelocityX = 4.0;
    imuSample.angularVelocityY = -5.0;
    imuSample.angularVelocityZ = 6.0;
    imuSample.rollDegrees = 7.0;
    imuSample.pitchDegrees = -8.0;
    imuSample.yawDegrees = 9.0;
    imu.setImuSample(imuSample);
    if (!require(imuAcceleration->text().contains(QStringLiteral("1.250")) &&
                     imuAcceleration->text().contains(QStringLiteral("g")),
                 "IMU acceleration value or unit was not displayed") ||
        !require(accelerationPlot->sampleCount() == 1,
                 "IMU acceleration was not appended to the plot")) {
        return 1;
    }
    imu.setCalibrationState(1);
    if (!require(imu.findChild<QLabel *>("imuCalibrationStatusLabel")
                     ->text()
                     .contains(QStringLiteral("完成")),
                 "IMU calibration result was not displayed")) {
        return 1;
    }
    imu.setDeviceError(QStringLiteral("设备错误 0x04"));
    if (!require(imuError->text().contains(QStringLiteral("0x04")),
                 "IMU error message was not displayed")) {
        return 1;
    }

    OverviewPage overview;
    DeviceInfo info;
    info.protocolVersion = 1;
    info.firmwareMajor = 2;
    info.firmwareMinor = 3;
    info.firmwarePatch = 4;
    overview.setDeviceInfo(info);
    overview.setStatus(DeviceStatus{2, 1, 0, 0x000a, 1});
    overview.setImuSample(imuSample);
    if (!require(overview.findChild<QLabel *>("firmwareVersionLabel")
                     ->text() == QStringLiteral("v2.3.4") &&
                     overview.findChild<QLabel *>("emergencyStateLabel")
                         ->text()
                         .contains(QStringLiteral("急停")),
                 "overview status fields were not displayed") ) {
        return 1;
    }
    mechanism.setConnected(true);
    const QVector<ParameterValue> mechanismValues = {
        {0x3000, ValueType::Float32, QVariant(10.0)},
    };
    mechanism.setValues(mechanismValues);
    if (!require(!mechanismRestore->isEnabled(),
                 "an unsolicited mechanism response created connection initial values")) {
        return 1;
    }
    mechanism.setConnected(false);
    mechanism.setValues(mechanismValues);
    if (!require(!mechanismRestore->isEnabled(),
                 "a disconnected late mechanism response created connection initial values")) {
        return 1;
    }
    mechanism.setConnected(true);
    mechanismRead->click();
    mechanism.setValues(mechanismValues);
    if (!require(mechanismRestore->isEnabled(),
                 "a requested mechanism read did not create connection initial values")) {
        return 1;
    }

    ActionTestPage standaloneActionPage;
    standaloneActionPage.setConnected(true);
    standaloneActionPage.setTestActionsEnabled(true, 30000);
    standaloneActionPage.show();
    QApplication::processEvents();
    auto *forwardButton =
        standaloneActionPage.findChild<QPushButton *>("chassisForwardButton");
    auto *backwardButton =
        standaloneActionPage.findChild<QPushButton *>("chassisBackwardButton");
    auto *leftButton =
        standaloneActionPage.findChild<QPushButton *>("chassisLeftButton");
    auto *rightButton =
        standaloneActionPage.findChild<QPushButton *>("chassisRightButton");
    auto *standaloneServo2Slider =
        standaloneActionPage.findChild<QSlider *>("servo2Slider");
    auto *standaloneServo2Angle =
        standaloneActionPage.findChild<QSpinBox *>("servo2AngleSpinBox");
    auto *standaloneServo2Send =
        standaloneActionPage.findChild<QPushButton *>("servo2SendButton");
    auto *standaloneServo3Slider =
        standaloneActionPage.findChild<QSlider *>("servo3Slider");
    auto *standaloneServo3Angle =
        standaloneActionPage.findChild<QSpinBox *>("servo3AngleSpinBox");
    auto *standaloneServo3Send =
        standaloneActionPage.findChild<QPushButton *>("servo3SendButton");
    auto *standaloneServo4Slider =
        standaloneActionPage.findChild<QSlider *>("servo4Slider");
    auto *standaloneServo4Angle =
        standaloneActionPage.findChild<QSpinBox *>("servo4AngleSpinBox");
    auto *standaloneServo4Send =
        standaloneActionPage.findChild<QPushButton *>("servo4SendButton");
    auto *standaloneServoLog =
        standaloneActionPage.findChild<QListWidget *>("servoActionLog");
    qint32 directionVx = 0;
    qint32 directionVy = 0;
    qint32 directionW = 0;
    int directionCalls = 0;
    QObject::connect(&standaloneActionPage, &ActionTestPage::chassisRequested,
                     [&](qint32 vxValue, qint32 vyValue, qint32 wValue,
                         qint32) {
                         ++directionCalls;
                         directionVx = vxValue;
                         directionVy = vyValue;
                         directionW = wValue;
                     });
    if (!require(forwardButton && backwardButton && leftButton && rightButton &&
                     standaloneServo2Slider && standaloneServo2Angle &&
                     standaloneServo2Send && standaloneServo3Slider &&
                     standaloneServo3Angle && standaloneServo3Send &&
                     standaloneServo4Slider && standaloneServo4Angle &&
                     standaloneServo4Send && standaloneServoLog &&
                     forwardButton->isEnabled() && backwardButton->isEnabled() &&
                     leftButton->isEnabled() && rightButton->isEnabled(),
                 "mecanum direction buttons are missing")) {
        return 1;
    }
    standaloneServo2Slider->setValue(120);
    standaloneServo3Slider->setValue(125);
    standaloneServo4Slider->setValue(240);
    if (!require(standaloneServo2Angle->value() == 120 &&
                     standaloneServo3Angle->value() == 125 &&
                     standaloneServo4Angle->value() == 240,
                 "servo sliders did not update their angle fields")) {
        return 1;
    }
    standaloneServo2Angle->setValue(30);
    standaloneServo3Angle->setValue(95);
    standaloneServo4Angle->setValue(300);
    if (!require(standaloneServo2Slider->value() == 30 &&
                     standaloneServo3Slider->value() == 95 &&
                     standaloneServo4Slider->value() == 300,
                 "servo angle fields did not update their sliders")) {
        return 1;
    }
    qint32 sentServoId = -1;
    qint32 sentServoAngle = -1;
    int servoSignalCalls = 0;
    QObject::connect(&standaloneActionPage, &ActionTestPage::servoRequested,
                     [&](qint32 servoId, qint32 angle) {
                         ++servoSignalCalls;
                         sentServoId = servoId;
                         sentServoAngle = angle;
                     });
    standaloneServo3Angle->setValue(125);
    acceptNextConfirmation();
    standaloneServo3Send->click();
    if (!require(sentServoId == 3 && sentServoAngle == 125 &&
                     servoSignalCalls == 1 &&
                     standaloneServoLog->count() == 1 &&
                     standaloneServoLog->item(0)->text() ==
                         QStringLiteral("舵机 3 -> 125°"),
                 "confirmed servo target was not recorded")) {
        return 1;
    }
    rejectNextConfirmation();
    standaloneServo3Send->click();
    if (!require(servoSignalCalls == 1 && standaloneServoLog->count() == 1,
                 "cancelled servo target changed the signal or log")) {
        return 1;
    }
    acceptNextConfirmation();
    forwardButton->click();
    if (!require(directionCalls == 1,
                 "forward direction confirmation did not emit an action") ||
        !require(directionVx == 0 && directionVy == 80 && directionW == 0,
                 "forward direction vector is incorrect")) {
        return 1;
    }
    acceptNextConfirmation();
    backwardButton->click();
    if (!require(directionVx == 0 && directionVy == -80 && directionW == 0,
                 "backward direction vector is incorrect")) {
        return 1;
    }
    acceptNextConfirmation();
    leftButton->click();
    if (!require(directionVx == -80 && directionVy == 0 && directionW == 0,
                 "left direction vector is incorrect")) {
        return 1;
    }
    acceptNextConfirmation();
    rightButton->click();
    if (!require(directionVx == 80 && directionVy == 0 && directionW == 0,
                 "right direction vector is incorrect")) {
        return 1;
    }

    MainWindow textWindow;
    auto *textMode = textWindow.findChild<QComboBox *>("deviceModeCombo");
    auto *textProtocol = textWindow.findChild<ProtocolClient *>();
    auto *textClient = textWindow.findChild<MecanumJogClient *>();
    auto *textPage = textWindow.findChild<MecanumJogPage *>("临时调试");
    auto *textField = textWindow.findChild<FieldPositionPage *>("场地定位");
    auto *textNavControls =
        textWindow.findChild<QGroupBox *>("navigationControlGroup");
    auto *textNavInit1 =
        textWindow.findChild<QPushButton *>("navigationInit1Button");
    auto *textNavMove =
        textWindow.findChild<QPushButton *>("navigationMoveButton");
    auto *textTerminal = textWindow.findChild<TerminalPage *>("串口终端");
    auto *textTerminalLog =
        textWindow.findChild<QPlainTextEdit *>("terminalLogTextEdit");
    auto *textTerminalClear =
        textWindow.findChild<QPushButton *>("terminalClearButton");
    auto *textLegacyPage = textWindow.findChild<QWidget *>("底盘与 PID");
    auto *textEmergency =
        textWindow.findChild<QPushButton *>("emergencyStopButton");
    auto *textClear =
        textWindow.findChild<QPushButton *>("clearEmergencyStopButton");
    QList<QByteArray> binaryWrites;
    QList<QByteArray> textWrites;
    QObject::connect(textProtocol, &ProtocolClient::bytesReady,
                     [&](QByteArray bytes) { binaryWrites.push_back(bytes); });
    QObject::connect(textClient, &MecanumJogClient::bytesReady,
                     [&](QByteArray bytes) { textWrites.push_back(bytes); });
    textMode->setCurrentIndex(1);
    if (!require(QMetaObject::invokeMethod(&textWindow, "handleSerialOpened",
                                           Qt::DirectConnection),
                 "text serial-open handler could not be invoked")) {
        return 1;
    }
    waitFor(300);
    if (!require(binaryWrites.isEmpty() &&
                     textWrites.contains(QByteArray("hb\r\n")),
                 "text mode sent binary HELLO or failed to start heartbeat") ||
        !require(textPage->isEnabled() && textField->isEnabled() &&
                     !textNavControls->isHidden() && !textLegacyPage->isEnabled() &&
                     !textClear->isEnabled(),
                 "text mode page enablement is incorrect")) {
        return 1;
    }
    textWrites.clear();
    textNavInit1->click();
    textClient->ingestBytes(QByteArrayView(
        "NAV INIT x=2250 y=2250 yaw_cdeg=9000\r\n"));
    textField->selectFieldPoint({1160, 2050});
    textNavMove->click();
    textClient->ingestBytes(QByteArrayView(
        "ARMED for one enable or motion command\r\n"));
    if (!require(textWrites ==
                     QList<QByteArray>({QByteArray("nav init 1\r\n"),
                                        QByteArray("arm\r\n"),
                                        QByteArray("nav goto 1200 2080\r\n")}),
                 "map navigation UI is not wired to the text client")) {
        return 1;
    }
    textTerminalClear->click();
    textTerminal->appendTx(QByteArray("hb\r\n"));
    textTerminal->appendRx(QByteArray("route sta"));
    textTerminal->appendRx(QByteArray("tus\r\nready\r\n"));
    if (!require(textTerminalLog->toPlainText() ==
                     QStringLiteral("route status\nready\n"),
                 "text mode terminal is not a plain ASCII receive stream")) {
        return 1;
    }
    textWrites.clear();
    textEmergency->click();
    if (!require(textWrites.contains(QByteArray("!")),
                 "text mode emergency stop did not send a raw exclamation")) {
        return 1;
    }
    if (!require(!textMode->isEnabled(),
                 "device mode remained changeable while connected")) {
        return 1;
    }

    auto *textTerminalInput = textWindow.findChild<QLineEdit *>("terminalInputLineEdit");
    auto *textTerminalSend = textWindow.findChild<QPushButton *>("terminalSendButton");
    auto *terminalHex = textWindow.findChild<QCheckBox *>("terminalHexSendCheckBox");
    auto *terminalNewline = textWindow.findChild<QCheckBox *>("terminalNewlineCheckBox");
    if (!require(!terminalHex->isChecked() && terminalNewline->isChecked(),
                 "text mode terminal retained binary send defaults")) {
        return 1;
    }
    for (const QString &stop : {QStringLiteral("!"), QStringLiteral("stop"),
                               QStringLiteral("X"), QStringLiteral("disable")}) {
        textWrites.clear();
        textClient->sendArmedCommand(QStringLiteral("W"));
        textTerminalInput->setText(stop);
        textTerminalSend->click();
        textClient->ingestBytes(QByteArrayView("ARMED for one enable or motion command\r\n"));
        if (!require(textWrites.size() == 2 &&
                         textWrites.back() == (stop == QStringLiteral("!")
                             ? QByteArray("!") : stop.toUtf8() + QByteArray("\r\n")) &&
                         !textWrites.contains(QByteArray("W\r\n")),
                     "terminal stop bypassed the armed command cancellation")) {
            return 1;
        }
    }
    terminalNewline->setChecked(false);
    textWrites.clear();
    textTerminalInput->setText(QStringLiteral("servo 2 90"));
    textTerminalSend->click();
    if (!require(textWrites == QList<QByteArray>({QByteArray("servo 2 90\r\n")}),
                 "text terminal allowed heartbeat to join an unterminated command")) {
        return 1;
    }
    terminalHex->setChecked(true);
    textWrites.clear();
    textTerminalInput->setText(QStringLiteral("61 72 6d 0d 0a 57 0d 0a"));
    textTerminalSend->click();
    if (!require(textWrites.isEmpty(), "HEX bypass allowed a batch of text commands")) {
        return 1;
    }

    MainWindow cycleWindow;
    cycleWindow.findChild<QComboBox *>("deviceModeCombo")->setCurrentIndex(0);
    QMetaObject::invokeMethod(&cycleWindow, "handleSerialOpened", Qt::DirectConnection);
    auto *cycleTerminal = cycleWindow.findChild<TerminalPage *>();
    auto *cyclePanel = cycleTerminal->findChild<SerialDebugPanel *>();
    cycleWindow.findChild<QCheckBox *>("terminalHexSendCheckBox")->setChecked(false);
    cycleWindow.findChild<QCheckBox *>("terminalPresetEnabled0")->setChecked(true);
    cycleWindow.findChild<QCheckBox *>("terminalPresetEnabled1")->setChecked(true);
    cycleWindow.findChild<QLineEdit *>("terminalPresetInput0")->setText(QStringLiteral("first"));
    cycleWindow.findChild<QLineEdit *>("terminalPresetInput1")->setText(QStringLiteral("second"));
    cycleWindow.findChild<QSpinBox *>("terminalCyclePeriodSpinBox")->setValue(50);
    int cycleWrites = 0;
    QObject::connect(cycleTerminal, &TerminalPage::rawSendRequested,
                     [&](QByteArray) { ++cycleWrites; });
    cycleWindow.findChild<QPushButton *>("terminalCycleButton")->click();
    if (!require(cyclePanel->cyclicSending(), "standard mode cycle did not start")) {
        return 1;
    }
    cycleWindow.findChild<QPushButton *>("emergencyStopButton")->click();
    waitFor(70);
    if (!require(!cyclePanel->cyclicSending() && cycleWrites == 0,
                 "global emergency stop left terminal cyclic sending active")) {
        return 1;
    }
    return 0;
}
