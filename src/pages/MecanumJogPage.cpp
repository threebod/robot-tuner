#include "pages/MecanumJogPage.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QPushButton *button(const QString &text, const QString &name, QWidget *parent) {
    auto *result = new QPushButton(text, parent);
    result->setObjectName(name);
    return result;
}

QSpinBox *spin(const QString &name, int minimum, int maximum, int value,
               QWidget *parent) {
    auto *result = new QSpinBox(parent);
    result->setObjectName(name);
    result->setRange(minimum, maximum);
    result->setValue(value);
    return result;
}

QComboBox *combo(const QString &name, const QStringList &items,
                 QWidget *parent) {
    auto *result = new QComboBox(parent);
    result->setObjectName(name);
    result->addItems(items);
    return result;
}

}  // namespace

MecanumJogPage::MecanumJogPage(QWidget *parent) : QWidget(parent) {
    auto *pageLayout = new QVBoxLayout(this);
    auto *notice = new QLabel(
        QStringLiteral("临时实车调试：动作会自动执行 arm → 等待确认 → 命令。"
                       "首次运行必须架空底盘并准备物理急停。"),
        this);
    notice->setWordWrap(true);
    pageLayout->addWidget(notice);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    controls_ = new QWidget(scroll);
    auto *grid = new QGridLayout(controls_);

    auto bind = [this](QPushButton *control, const QString &command,
                       bool armed) {
        connect(control, &QPushButton::clicked, this,
                [this, command, armed] {
                    if (armed) {
                        emit armedCommandRequested(command);
                    } else {
                        emit commandRequested(command);
                    }
                });
    };

    auto *chassis = new QGroupBox(QStringLiteral("底盘"), controls_);
    auto *chassisLayout = new QGridLayout(chassis);
    auto *forward = button(QStringLiteral("前进 W"),
                           QStringLiteral("mecanumForwardButton"), chassis);
    auto *back = button(QStringLiteral("后退 S"),
                        QStringLiteral("mecanumBackButton"), chassis);
    auto *left = button(QStringLiteral("左移 A"),
                        QStringLiteral("mecanumLeftButton"), chassis);
    auto *right = button(QStringLiteral("右移 D"),
                         QStringLiteral("mecanumRightButton"), chassis);
    auto *stop = button(QStringLiteral("停止"),
                        QStringLiteral("mecanumStopButton"), chassis);
    bind(forward, QStringLiteral("W"), true);
    bind(back, QStringLiteral("S"), true);
    bind(left, QStringLiteral("A"), true);
    bind(right, QStringLiteral("D"), true);
    bind(stop, QStringLiteral("stop"), false);
    chassisLayout->addWidget(forward, 0, 1);
    chassisLayout->addWidget(left, 1, 0);
    chassisLayout->addWidget(back, 1, 1);
    chassisLayout->addWidget(right, 1, 2);
    chassisLayout->addWidget(stop, 1, 3);

    auto *lineDirection = combo(QStringLiteral("mecanumLineDirectionCombo"),
                                {QStringLiteral("W"), QStringLiteral("S")},
                                chassis);
    auto *lineDistance = spin(QStringLiteral("mecanumLineDistanceSpin"),
                              100, 500, 100, chassis);
    lineDistance->setSingleStep(100);
    lineDistance->setSuffix(QStringLiteral(" mm"));
    auto *lineRpm = spin(QStringLiteral("mecanumLineRpmSpin"), 10, 120, 30,
                         chassis);
    lineRpm->setSingleStep(10);
    lineRpm->setSuffix(QStringLiteral(" RPM"));
    auto *lineSend = button(QStringLiteral("发送 line"),
                            QStringLiteral("mecanumLineSendButton"), chassis);
    connect(lineSend, &QPushButton::clicked, this,
            [this, lineDirection, lineDistance, lineRpm] {
                emit armedCommandRequested(
                    QStringLiteral("line %1 %2 %3")
                        .arg(lineDirection->currentText())
                        .arg(lineDistance->value())
                        .arg(lineRpm->value()));
            });
    chassisLayout->addWidget(new QLabel(QStringLiteral("定距"), chassis), 2, 0);
    chassisLayout->addWidget(lineDirection, 2, 1);
    chassisLayout->addWidget(lineDistance, 2, 2);
    chassisLayout->addWidget(lineRpm, 2, 3);
    chassisLayout->addWidget(lineSend, 2, 4);

    auto *straightDirection = combo(
        QStringLiteral("mecanumStraightDirectionCombo"),
        {QStringLiteral("W"), QStringLiteral("S")}, chassis);
    auto *straightDuration = spin(
        QStringLiteral("mecanumStraightDurationSpin"), 1000, 5000, 2000,
        chassis);
    straightDuration->setSuffix(QStringLiteral(" ms"));
    auto *straightRpm = spin(QStringLiteral("mecanumStraightRpmSpin"),
                             10, 120, 30, chassis);
    straightRpm->setSingleStep(10);
    straightRpm->setSuffix(QStringLiteral(" RPM"));
    auto *straightSend = button(
        QStringLiteral("发送 straight"),
        QStringLiteral("mecanumStraightSendButton"), chassis);
    connect(straightSend, &QPushButton::clicked, this,
            [this, straightDirection, straightDuration, straightRpm] {
                emit armedCommandRequested(
                    QStringLiteral("straight %1 %2 %3")
                        .arg(straightDirection->currentText())
                        .arg(straightDuration->value())
                        .arg(straightRpm->value()));
            });
    chassisLayout->addWidget(new QLabel(QStringLiteral("航向保持"), chassis),
                             3, 0);
    chassisLayout->addWidget(straightDirection, 3, 1);
    chassisLayout->addWidget(straightDuration, 3, 2);
    chassisLayout->addWidget(straightRpm, 3, 3);
    chassisLayout->addWidget(straightSend, 3, 4);
    grid->addWidget(chassis, 0, 0, 1, 2);

    auto *diagnostics = new QGroupBox(QStringLiteral("单轮与 CAN"), controls_);
    auto *diagnosticsLayout = new QFormLayout(diagnostics);
    auto makeRow = [diagnostics]() {
        auto *row = new QWidget(diagnostics);
        row->setLayout(new QHBoxLayout);
        row->layout()->setContentsMargins(0, 0, 0, 0);
        return row;
    };
    auto *auxMotorRow = makeRow();
    auto *auxMotorId = combo(QStringLiteral("mecanumAuxMotorIdCombo"),
                             {QStringLiteral("5"), QStringLiteral("6")},
                             auxMotorRow);
    auto *auxMotorDirection = combo(
        QStringLiteral("mecanumAuxMotorDirectionCombo"),
        {QStringLiteral("0"), QStringLiteral("1")}, auxMotorRow);
    auto *auxMotorSend = button(QStringLiteral("点动"),
                                QStringLiteral("mecanumAuxMotorSendButton"),
                                auxMotorRow);
    auxMotorRow->layout()->addWidget(auxMotorId);
    auxMotorRow->layout()->addWidget(auxMotorDirection);
    auxMotorRow->layout()->addWidget(auxMotorSend);
    connect(auxMotorSend, &QPushButton::clicked, this,
            [this, auxMotorId, auxMotorDirection] {
                emit armedCommandRequested(QStringLiteral("motor %1 %2")
                                                .arg(auxMotorId->currentText())
                                                .arg(auxMotorDirection->currentText()));
            });
    diagnosticsLayout->addRow(QStringLiteral("辅助电机"), auxMotorRow);

    auto *wheelRow = makeRow();
    auto *wheelId = spin(QStringLiteral("mecanumWheelIdSpin"), 1, 4, 1, wheelRow);
    auto *wheelDirection = combo(QStringLiteral("mecanumWheelDirectionCombo"),
                                 {QStringLiteral("0"), QStringLiteral("1")},
                                 wheelRow);
    auto *wheelSend = button(QStringLiteral("单轮点动"),
                             QStringLiteral("mecanumWheelSendButton"), wheelRow);
    wheelRow->layout()->addWidget(wheelId);
    wheelRow->layout()->addWidget(wheelDirection);
    wheelRow->layout()->addWidget(wheelSend);
    connect(wheelSend, &QPushButton::clicked, this,
            [this, wheelId, wheelDirection] {
                emit armedCommandRequested(QStringLiteral("wheel %1 %2")
                                               .arg(wheelId->value())
                                               .arg(wheelDirection->currentText()));
            });
    diagnosticsLayout->addRow(QStringLiteral("单轮"), wheelRow);

    auto *canRow = makeRow();
    auto *canId = spin(QStringLiteral("mecanumCanIdSpin"), 1, 6, 1, canRow);
    auto *canButton = button(QStringLiteral("查询 CAN"),
                             QStringLiteral("mecanumCanCheckButton"), canRow);
    canRow->layout()->addWidget(canId);
    canRow->layout()->addWidget(canButton);
    connect(canButton, &QPushButton::clicked, this, [this, canId] {
        emit commandRequested(QStringLiteral("cancheck %1").arg(canId->value()));
    });
    diagnosticsLayout->addRow(QStringLiteral("驱动器状态"), canRow);

    auto *invertRow = makeRow();
    auto *invertId = spin(QStringLiteral("mecanumInvertIdSpin"), 1, 4, 1,
                          invertRow);
    auto *invertValue = combo(QStringLiteral("mecanumInvertValueCombo"),
                              {QStringLiteral("0"), QStringLiteral("1")},
                              invertRow);
    auto *invertSend = button(QStringLiteral("设置反向"),
                              QStringLiteral("mecanumInvertSendButton"), invertRow);
    invertRow->layout()->addWidget(invertId);
    invertRow->layout()->addWidget(invertValue);
    invertRow->layout()->addWidget(invertSend);
    connect(invertSend, &QPushButton::clicked, this,
            [this, invertId, invertValue] {
                emit commandRequested(QStringLiteral("invert %1 %2")
                                          .arg(invertId->value())
                                          .arg(invertValue->currentText()));
            });
    diagnosticsLayout->addRow(QStringLiteral("方向映射"), invertRow);

    auto *trimRow = makeRow();
    auto *trimId = spin(QStringLiteral("mecanumTrimIdSpin"), 1, 4, 1, trimRow);
    auto *trimValue = spin(QStringLiteral("mecanumTrimValueSpin"), 900, 1100,
                           1000, trimRow);
    auto *trimSend = button(QStringLiteral("设置比例"),
                            QStringLiteral("mecanumTrimSendButton"), trimRow);
    trimRow->layout()->addWidget(trimId);
    trimRow->layout()->addWidget(trimValue);
    trimRow->layout()->addWidget(trimSend);
    connect(trimSend, &QPushButton::clicked, this,
            [this, trimId, trimValue] {
                emit commandRequested(QStringLiteral("trim %1 %2")
                                          .arg(trimId->value())
                                          .arg(trimValue->value()));
            });
    diagnosticsLayout->addRow(QStringLiteral("轮速比例"), trimRow);
    grid->addWidget(diagnostics, 1, 0);

    auto *servoImu = new QGroupBox(QStringLiteral("舵机与 IMU"), controls_);
    auto *servoImuLayout = new QFormLayout(servoImu);
    auto *servoRow = makeRow();
    servoIdCombo_ = combo(QStringLiteral("mecanumServoIdCombo"),
                          {QStringLiteral("2"), QStringLiteral("3"),
                           QStringLiteral("4")}, servoRow);
    servoAngleSlider_ = new QSlider(Qt::Horizontal, servoRow);
    servoAngleSlider_->setObjectName(QStringLiteral("mecanumServoAngleSlider"));
    servoAngleSlider_->setRange(0, 270);
    servoAngleLabel_ = new QLabel(QStringLiteral("0°"), servoRow);
    servoAngleLabel_->setObjectName(QStringLiteral("mecanumServoAngleValueLabel"));
    servoRow->layout()->addWidget(servoIdCombo_);
    servoRow->layout()->addWidget(servoAngleSlider_);
    servoRow->layout()->addWidget(servoAngleLabel_);
    connect(servoIdCombo_, &QComboBox::currentTextChanged, this,
            [this](const QString &id) {
                servoAngleSlider_->setMaximum(
                    id == QStringLiteral("4") ? 360 : 270);
                servoAngleLabel_->setText(
                    QStringLiteral("%1°").arg(servoAngleSlider_->value()));
            });
    connect(servoAngleSlider_, &QSlider::valueChanged, this, [this](int angle) {
        servoAngleLabel_->setText(QStringLiteral("%1°").arg(angle));
    });
    connect(servoAngleSlider_, &QSlider::sliderReleased, this, [this] {
        emit commandRequested(QStringLiteral("servo %1 %2")
                                  .arg(servoIdCombo_->currentText())
                                  .arg(servoAngleSlider_->value()));
    });
    servoImuLayout->addRow(QStringLiteral("舵机"), servoRow);

    auto *imuRow = makeRow();
    auto *imuBaud = combo(QStringLiteral("mecanumImuBaudCombo"),
                          {QStringLiteral("115200"), QStringLiteral("9600")},
                          imuRow);
    auto *imuSend = button(QStringLiteral("配置接收"),
                           QStringLiteral("mecanumImuSendButton"), imuRow);
    imuRow->layout()->addWidget(imuBaud);
    imuRow->layout()->addWidget(imuSend);
    connect(imuSend, &QPushButton::clicked, this, [this, imuBaud] {
        emit commandRequested(QStringLiteral("imu %1").arg(imuBaud->currentText()));
    });
    servoImuLayout->addRow(QStringLiteral("IMU 波特率"), imuRow);

    auto *yawRow = makeRow();
    auto *yawDir = combo(QStringLiteral("mecanumYawDirCombo"),
                         {QStringLiteral("0"), QStringLiteral("1")}, yawRow);
    auto *yawSend = button(QStringLiteral("设置纠偏方向"),
                           QStringLiteral("mecanumYawDirSendButton"), yawRow);
    auto *status = button(QStringLiteral("读取 status"),
                          QStringLiteral("mecanumStatusButton"), yawRow);
    yawRow->layout()->addWidget(yawDir);
    yawRow->layout()->addWidget(yawSend);
    yawRow->layout()->addWidget(status);
    connect(yawSend, &QPushButton::clicked, this, [this, yawDir] {
        emit commandRequested(QStringLiteral("yawdir %1").arg(yawDir->currentText()));
    });
    bind(status, QStringLiteral("status"), false);
    servoImuLayout->addRow(QStringLiteral("航向"), yawRow);
    grid->addWidget(servoImu, 1, 1);

    auto *route = new QGroupBox(QStringLiteral("路线"), controls_);
    auto *routeLayout = new QHBoxLayout(route);
    auto *routeMode = combo(QStringLiteral("mecanumRouteModeCombo"),
                            {QStringLiteral("start"), QStringLiteral("step"),
                             QStringLiteral("auto")},
                            route);
    auto *routeZone = combo(QStringLiteral("mecanumRouteZoneCombo"),
                            {QStringLiteral("1"), QStringLiteral("2")}, route);
    auto *routeRpm = spin(QStringLiteral("mecanumRouteRpmSpin"), 10, 120, 60,
                          route);
    routeRpm->setSingleStep(10);
    routeRpm->setSuffix(QStringLiteral(" RPM"));
    auto *routeStart = button(QStringLiteral("启动路线"),
                              QStringLiteral("mecanumRouteStartButton"), route);
    auto *routeNext = button(QStringLiteral("继续"),
                             QStringLiteral("mecanumRouteNextButton"), route);
    auto *routeStatus = button(QStringLiteral("路线状态"),
                               QStringLiteral("mecanumRouteStatusButton"), route);
    auto *routeLateralScale = new QDoubleSpinBox(route);
    routeLateralScale->setObjectName(
        QStringLiteral("mecanumRouteLateralScaleSpin"));
    routeLateralScale->setRange(50.0, 150.0);
    routeLateralScale->setDecimals(2);
    routeLateralScale->setSingleStep(0.1);
    routeLateralScale->setValue(87.62);
    routeLateralScale->setSuffix(QStringLiteral(" %"));
    auto *routeLateralScaleSend = button(
        QStringLiteral("设置横移比例"),
        QStringLiteral("mecanumRouteLateralScaleButton"), route);
    auto *help = button(QStringLiteral("帮助"),
                        QStringLiteral("mecanumHelpButton"), route);
    auto *turnDirection = combo(QStringLiteral("mecanumTurnDirectionCombo"),
                                {QStringLiteral("L"), QStringLiteral("R")},
                                route);
    auto *turnAngle = spin(QStringLiteral("mecanumTurnAngleSpin"), 1, 180, 90,
                           route);
    turnAngle->setSuffix(QStringLiteral("°"));
    auto *turnSend = button(QStringLiteral("原地旋转"),
                            QStringLiteral("mecanumTurnSendButton"), route);
    connect(routeStart, &QPushButton::clicked, this,
            [this, routeMode, routeZone, routeRpm] {
                emit armedCommandRequested(QStringLiteral("route %1 %2 %3")
                                               .arg(routeMode->currentText())
                                               .arg(routeZone->currentText())
                                               .arg(routeRpm->value()));
            });
    bind(routeNext, QStringLiteral("route next"), false);
    bind(routeStatus, QStringLiteral("route status"), false);
    connect(routeLateralScaleSend, &QPushButton::clicked, this,
            [this, routeLateralScale] {
                const int basisPoints = static_cast<int>(
                    routeLateralScale->value() * 100.0 + 0.5);
                emit commandRequested(
                    QStringLiteral("route scale %1").arg(basisPoints));
            });
    bind(help, QStringLiteral("help"), false);
    connect(turnSend, &QPushButton::clicked, this,
            [this, turnDirection, turnAngle] {
                emit armedCommandRequested(QStringLiteral("turn %1 %2")
                                               .arg(turnDirection->currentText())
                                               .arg(turnAngle->value()));
            });
    routeLayout->addWidget(new QLabel(QStringLiteral("模式"), route));
    routeLayout->addWidget(routeMode);
    routeLayout->addWidget(new QLabel(QStringLiteral("启停区"), route));
    routeLayout->addWidget(routeZone);
    routeLayout->addWidget(routeRpm);
    routeLayout->addWidget(routeStart);
    routeLayout->addWidget(routeNext);
    routeLayout->addWidget(routeStatus);
    routeLayout->addWidget(new QLabel(QStringLiteral("横移比例"), route));
    routeLayout->addWidget(routeLateralScale);
    routeLayout->addWidget(routeLateralScaleSend);
    routeLayout->addWidget(turnDirection);
    routeLayout->addWidget(turnAngle);
    routeLayout->addWidget(turnSend);
    routeLayout->addWidget(help);
    grid->addWidget(route, 2, 0, 1, 2);

    scroll->setWidget(controls_);
    pageLayout->addWidget(scroll, 1);
    stateLabel_ = new QLabel(QStringLiteral("未连接"), this);
    stateLabel_->setObjectName(QStringLiteral("mecanumCommandStateLabel"));
    replyLabel_ = new QLabel(QStringLiteral("尚无设备回复"), this);
    replyLabel_->setObjectName(QStringLiteral("mecanumLatestReplyLabel"));
    replyLabel_->setWordWrap(true);
    pageLayout->addWidget(stateLabel_);
    pageLayout->addWidget(replyLabel_);
    setConnected(false);
}

void MecanumJogPage::setConnected(bool connected) {
    controls_->setEnabled(connected);
    if (!connected) {
        stateLabel_->setText(QStringLiteral("未连接"));
    }
}

void MecanumJogPage::setCommandState(const QString &state) {
    stateLabel_->setText(state);
}

void MecanumJogPage::appendLine(const QString &line) {
    replyLabel_->setText(QStringLiteral("最近回复：%1").arg(line));
}

void MecanumJogPage::showError(const QString &error) {
    stateLabel_->setText(QStringLiteral("错误：%1").arg(error));
}
