#include "pages/ActionTestPage.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QPushButton *makeButton(const QString &text, const QString &objectName,
                        QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

QSpinBox *makeIntSpin(const QString &objectName, int minimum, int maximum,
                      int value, QWidget *parent) {
    auto *spin = new QSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    return spin;
}

QDoubleSpinBox *makeDoubleSpin(const QString &objectName, double minimum,
                               double maximum, double value, QWidget *parent) {
    auto *spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setDecimals(3);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    return spin;
}

}  // namespace

ActionTestPage::ActionTestPage(QWidget *parent) : QWidget(parent) {
    auto *pageLayout = new QVBoxLayout(this);

    auto *safetyGroup = new QGroupBox(QStringLiteral("调试动作安全状态"), this);
    auto *safetyLayout = new QHBoxLayout(safetyGroup);
    unlockButton_ = makeButton(QStringLiteral("解锁动作测试"),
                                QStringLiteral("testUnlockButton"),
                                safetyGroup);
    unlockStatusLabel_ = new QLabel(QStringLiteral("未解锁"), safetyGroup);
    unlockStatusLabel_->setObjectName(QStringLiteral("testUnlockStatusLabel"));
    remainingLabel_ = new QLabel(QStringLiteral("剩余：—"), safetyGroup);
    remainingLabel_->setObjectName(QStringLiteral("unlockRemainingLabel"));
    stopButton_ = makeButton(QStringLiteral("立即停止"),
                             QStringLiteral("actionStopButton"), safetyGroup);
    safetyLayout->addWidget(unlockButton_);
    safetyLayout->addWidget(unlockStatusLabel_);
    safetyLayout->addWidget(remainingLabel_);
    safetyLayout->addStretch();
    safetyLayout->addWidget(stopButton_);
    pageLayout->addWidget(safetyGroup);

    auto *chassisGroup = new QGroupBox(QStringLiteral("底盘点动（设备端再次限幅）"),
                                       this);
    auto *chassisLayout = new QVBoxLayout(chassisGroup);
    auto *chassisFields = new QFormLayout;
    chassisVxSpinBox_ = makeIntSpin(QStringLiteral("testChassisVxSpinBox"),
                                     -80, 80, 0, chassisGroup);
    chassisVySpinBox_ = makeIntSpin(QStringLiteral("testChassisVySpinBox"),
                                     -80, 80, 0, chassisGroup);
    chassisWSpinBox_ = makeIntSpin(QStringLiteral("testChassisWSpinBox"),
                                    -30, 30, 0, chassisGroup);
    chassisDurationSpinBox_ = makeIntSpin(
        QStringLiteral("testChassisDurationSpinBox"), 50, 1000, 100,
        chassisGroup);
    chassisDurationSpinBox_->setSuffix(QStringLiteral(" ms"));
    actionWidgets_ += {chassisVxSpinBox_, chassisVySpinBox_, chassisWSpinBox_,
                       chassisDurationSpinBox_};
    chassisFields->addRow(QStringLiteral("vx"), chassisVxSpinBox_);
    chassisFields->addRow(QStringLiteral("vy"), chassisVySpinBox_);
    chassisFields->addRow(QStringLiteral("w"), chassisWSpinBox_);
    chassisFields->addRow(QStringLiteral("持续时间"), chassisDurationSpinBox_);
    chassisLayout->addLayout(chassisFields);
    auto *directionButtons = new QHBoxLayout;
    const auto addDirection = [&](const QString &text, const QString &name,
                                  qint32 vx, qint32 vy, qint32 w) {
        auto *button = makeButton(text, name, chassisGroup);
        directionButtons->addWidget(button);
        connect(button, &QPushButton::clicked, this,
                [this, vx, vy, w, text] {
                    requestDirectional(vx, vy, w, text);
                });
        actionWidgets_.push_back(button);
    };
    addDirection(QStringLiteral("前进"), QStringLiteral("chassisForwardButton"),
                 80, 0, 0);
    addDirection(QStringLiteral("后退"), QStringLiteral("chassisBackwardButton"),
                 -80, 0, 0);
    addDirection(QStringLiteral("左移"), QStringLiteral("chassisLeftButton"),
                 0, 80, 0);
    addDirection(QStringLiteral("右移"), QStringLiteral("chassisRightButton"),
                 0, -80, 0);
    addDirection(QStringLiteral("左转"), QStringLiteral("chassisTurnLeftButton"),
                 0, 0, 30);
    addDirection(QStringLiteral("右转"), QStringLiteral("chassisTurnRightButton"),
                 0, 0, -30);
    auto *customButton = makeButton(QStringLiteral("发送自定义底盘动作"),
                                     QStringLiteral("testChassisButton"),
                                     chassisGroup);
    connect(customButton, &QPushButton::clicked, this,
            &ActionTestPage::requestChassis);
    actionWidgets_.push_back(customButton);
    directionButtons->addWidget(customButton);
    chassisLayout->addLayout(directionButtons);
    pageLayout->addWidget(chassisGroup);

    auto *mechanismGroup = new QGroupBox(QStringLiteral("机构单步动作"), this);
    auto *mechanismLayout = new QVBoxLayout(mechanismGroup);
    auto *horizontalFields = new QFormLayout;
    horizontalTargetSpinBox_ = makeDoubleSpin(
        QStringLiteral("testHorizontalTargetSpinBox"), -120.0, 63.0, 0.0,
        mechanismGroup);
    horizontalSpeedSpinBox_ = makeIntSpin(
        QStringLiteral("testHorizontalSpeedSpinBox"), 100, 2000, 100,
        mechanismGroup);
    horizontalAccelSpinBox_ = makeIntSpin(
        QStringLiteral("testHorizontalAccelSpinBox"), 1, 220, 1,
        mechanismGroup);
    actionWidgets_ += {horizontalTargetSpinBox_, horizontalSpeedSpinBox_,
                       horizontalAccelSpinBox_};
    horizontalFields->addRow(QStringLiteral("水平位置"), horizontalTargetSpinBox_);
    horizontalFields->addRow(QStringLiteral("水平速度"), horizontalSpeedSpinBox_);
    horizontalFields->addRow(QStringLiteral("水平加速度"),
                             horizontalAccelSpinBox_);
    mechanismLayout->addLayout(horizontalFields);
    auto *horizontalButton = makeButton(
        QStringLiteral("执行水平机构"), QStringLiteral("testHorizontalButton"),
        mechanismGroup);
    connect(horizontalButton, &QPushButton::clicked, this,
            &ActionTestPage::requestHorizontal);
    actionWidgets_.push_back(horizontalButton);
    mechanismLayout->addWidget(horizontalButton);

    auto *liftFields = new QFormLayout;
    liftTargetSpinBox_ = makeDoubleSpin(QStringLiteral("testLiftTargetSpinBox"),
                                        0.0, 50.0, 0.0, mechanismGroup);
    liftSpeedSpinBox_ = makeIntSpin(QStringLiteral("testLiftSpeedSpinBox"),
                                    100, 2000, 100, mechanismGroup);
    liftAccelSpinBox_ = makeIntSpin(QStringLiteral("testLiftAccelSpinBox"), 1,
                                    220, 1, mechanismGroup);
    actionWidgets_ += {liftTargetSpinBox_, liftSpeedSpinBox_, liftAccelSpinBox_};
    liftFields->addRow(QStringLiteral("升降位置"), liftTargetSpinBox_);
    liftFields->addRow(QStringLiteral("升降速度"), liftSpeedSpinBox_);
    liftFields->addRow(QStringLiteral("升降加速度"), liftAccelSpinBox_);
    mechanismLayout->addLayout(liftFields);
    auto *liftButton = makeButton(QStringLiteral("执行升降机构"),
                                   QStringLiteral("testLiftButton"),
                                   mechanismGroup);
    connect(liftButton, &QPushButton::clicked, this,
            &ActionTestPage::requestLift);
    actionWidgets_.push_back(liftButton);
    mechanismLayout->addWidget(liftButton);

    auto *turretFields = new QFormLayout;
    turretAngleSpinBox_ = makeDoubleSpin(QStringLiteral("testTurretAngleSpinBox"),
                                         135.0, 295.0, 135.0, mechanismGroup);
    turretSpeedSpinBox_ = makeDoubleSpin(
        QStringLiteral("testTurretSpeedSpinBox"), 1.0, 20.0, 1.0,
        mechanismGroup);
    actionWidgets_ += {turretAngleSpinBox_, turretSpeedSpinBox_};
    turretFields->addRow(QStringLiteral("云台角度"), turretAngleSpinBox_);
    turretFields->addRow(QStringLiteral("插补速度 (degree/s)"),
                         turretSpeedSpinBox_);
    mechanismLayout->addLayout(turretFields);
    auto *turretButton = makeButton(QStringLiteral("执行云台动作"),
                                     QStringLiteral("testTurretButton"),
                                     mechanismGroup);
    connect(turretButton, &QPushButton::clicked, this,
            &ActionTestPage::requestTurret);
    actionWidgets_.push_back(turretButton);
    mechanismLayout->addWidget(turretButton);

    auto *namedRow = new QHBoxLayout;
    platformPositionCombo_ = new QComboBox(mechanismGroup);
    platformPositionCombo_->setObjectName(QStringLiteral("platformPositionCombo"));
    for (int position = 1; position <= 3; ++position) {
        platformPositionCombo_->addItem(QStringLiteral("平台位置 %1").arg(position),
                                        position);
    }
    auto *platformButton = makeButton(QStringLiteral("执行平台位置"),
                                      QStringLiteral("platformPositionButton"),
                                      mechanismGroup);
    auto *openButton = makeButton(QStringLiteral("夹爪打开"),
                                   QStringLiteral("gripperOpenButton"),
                                   mechanismGroup);
    auto *closeButton = makeButton(QStringLiteral("夹爪关闭"),
                                    QStringLiteral("gripperCloseButton"),
                                    mechanismGroup);
    namedRow->addWidget(platformPositionCombo_);
    namedRow->addWidget(platformButton);
    namedRow->addWidget(openButton);
    namedRow->addWidget(closeButton);
    connect(platformButton, &QPushButton::clicked, this,
            &ActionTestPage::requestPlatformPosition);
    connect(openButton, &QPushButton::clicked, this,
            &ActionTestPage::requestGripperOpen);
    connect(closeButton, &QPushButton::clicked, this,
            &ActionTestPage::requestGripperClose);
    actionWidgets_ += {platformButton, openButton, closeButton};
    actionWidgets_.push_back(platformPositionCombo_);
    mechanismLayout->addLayout(namedRow);
    pageLayout->addWidget(mechanismGroup);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("actionStatusLabel"));
    statusLabel_->setWordWrap(true);
    pageLayout->addWidget(statusLabel_);
    pageLayout->addStretch();

    connect(unlockButton_, &QPushButton::clicked, this,
            &ActionTestPage::requestUnlock);
    connect(stopButton_, &QPushButton::clicked, this,
            &ActionTestPage::requestStop);
    setConnected(false);
}

void ActionTestPage::setConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        setTestActionsEnabled(false);
        emergencyLocked_ = false;
        unlockStatusLabel_->setText(QStringLiteral("未解锁"));
        remainingLabel_->setText(QStringLiteral("剩余：—"));
        setStatus(QString());
    }
    unlockButton_->setEnabled(connected_ && !emergencyLocked_ &&
                              !testActionsEnabled_);
    stopButton_->setEnabled(connected_);
    if (connected_) {
        setActionWidgetsEnabled(testActionsEnabled_ && !emergencyLocked_);
    }
}

void ActionTestPage::setTestActionsEnabled(bool enabled, qint64 remainingMs) {
    testActionsEnabled_ = enabled && connected_ && !emergencyLocked_;
    setActionWidgetsEnabled(testActionsEnabled_);
    unlockButton_->setEnabled(connected_ && !emergencyLocked_ &&
                              !testActionsEnabled_);
    unlockStatusLabel_->setText(testActionsEnabled_ ? QStringLiteral("已解锁")
                                                    : QStringLiteral("未解锁"));
    remainingLabel_->setText(testActionsEnabled_
                                 ? QStringLiteral("剩余：%1 ms").arg(remainingMs)
                                 : QStringLiteral("剩余：—"));
}

void ActionTestPage::setEmergencyLocked(bool locked) {
    emergencyLocked_ = locked;
    if (locked) {
        setTestActionsEnabled(false);
        setStatus(QStringLiteral("设备处于急停锁定，动作已禁用"));
    }
    unlockButton_->setEnabled(connected_ && !emergencyLocked_ &&
                              !testActionsEnabled_);
    stopButton_->setEnabled(connected_);
}

void ActionTestPage::setDeviceError(const QString &message) {
    setStatus(message);
}

void ActionTestPage::requestUnlock() {
    if (connected_ && !emergencyLocked_ && !testActionsEnabled_) {
        setStatus(QStringLiteral("正在请求动作解锁…"));
        emit unlockRequested();
    }
}

void ActionTestPage::requestStop() {
    if (connected_) {
        emit stopRequested();
        setStatus(QStringLiteral("已请求立即停止"));
    }
}

bool ActionTestPage::confirmAction(const QString &description) const {
    return QMessageBox::question(
               const_cast<ActionTestPage *>(this), QStringLiteral("确认调试动作"),
               QStringLiteral("即将执行：%1\n确认发送？").arg(description),
               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) ==
           QMessageBox::Yes;
}

void ActionTestPage::requestDirectional(qint32 vx, qint32 vy, qint32 w,
                                        const QString &description) {
    if (!testActionsEnabled_ || !confirmAction(description)) {
        return;
    }
    emit chassisRequested(vx, vy, w, chassisDurationSpinBox_->value());
}

void ActionTestPage::requestChassis() {
    if (!testActionsEnabled_ || !confirmAction(QStringLiteral("自定义底盘动作"))) {
        return;
    }
    emit chassisRequested(chassisVxSpinBox_->value(), chassisVySpinBox_->value(),
                          chassisWSpinBox_->value(),
                          chassisDurationSpinBox_->value());
}

void ActionTestPage::requestHorizontal() {
    if (!testActionsEnabled_ || !confirmAction(QStringLiteral("水平机构动作"))) {
        return;
    }
    emit horizontalRequested(horizontalTargetSpinBox_->value(),
                             horizontalSpeedSpinBox_->value(),
                             horizontalAccelSpinBox_->value());
}

void ActionTestPage::requestLift() {
    if (!testActionsEnabled_ || !confirmAction(QStringLiteral("升降机构动作"))) {
        return;
    }
    emit liftRequested(liftTargetSpinBox_->value(), liftSpeedSpinBox_->value(),
                       liftAccelSpinBox_->value());
}

void ActionTestPage::requestTurret() {
    if (!testActionsEnabled_ || !confirmAction(QStringLiteral("云台动作"))) {
        return;
    }
    emit turretRequested(turretAngleSpinBox_->value(),
                         turretSpeedSpinBox_->value());
}

void ActionTestPage::requestPlatformPosition() {
    if (!testActionsEnabled_ ||
        !confirmAction(QStringLiteral("平台命名位置 %1")
                           .arg(platformPositionCombo_->currentData().toInt()))) {
        return;
    }
    emit platformPositionRequested(platformPositionCombo_->currentData().toInt());
}

void ActionTestPage::requestGripperOpen() {
    if (testActionsEnabled_ && confirmAction(QStringLiteral("夹爪打开"))) {
        emit gripperRequested(true);
    }
}

void ActionTestPage::requestGripperClose() {
    if (testActionsEnabled_ && confirmAction(QStringLiteral("夹爪关闭"))) {
        emit gripperRequested(false);
    }
}

void ActionTestPage::setActionWidgetsEnabled(bool enabled) {
    for (QWidget *widget : actionWidgets_) {
        if (widget != nullptr) {
            widget->setEnabled(enabled);
        }
    }
}

void ActionTestPage::setStatus(const QString &message) {
    if (statusLabel_ != nullptr) {
        statusLabel_->setText(message);
    }
}
