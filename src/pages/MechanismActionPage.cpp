#include "pages/MechanismActionPage.h"

#include <QComboBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QSpinBox *spin(QWidget *parent, const char *name, int minimum, int maximum,
               int value, const QString &suffix = {}) {
    auto *box = new QSpinBox(parent);
    box->setObjectName(QString::fromLatin1(name));
    box->setRange(minimum, maximum);
    box->setValue(value);
    box->setSuffix(suffix);
    return box;
}

QPushButton *button(QWidget *parent, const QString &text, const char *name) {
    auto *result = new QPushButton(text, parent);
    result->setObjectName(QString::fromLatin1(name));
    return result;
}

}  // namespace

MechanismActionPage::MechanismActionPage(QWidget *parent) : QWidget(parent) {
    waitTimer_.setSingleShot(true);
    connect(&waitTimer_, &QTimer::timeout, this, [this] {
        ++replayIndex_;
        dispatchCurrentStep();
    });

    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(
        QStringLiteral("仅用于人工调试：位置来自命令积分估计。确认机构已人工放到初始姿态后再初始化。"),
        this);
    notice->setWordWrap(true);
    layout->addWidget(notice);

    auto *initialGroup = new QGroupBox(QStringLiteral("动作与人工初始姿态"), this);
    auto *initialLayout = new QGridLayout(initialGroup);
    nameEdit_ = new QLineEdit(QStringLiteral("mechanism_action"), initialGroup);
    nameEdit_->setObjectName(QStringLiteral("actionNameEdit"));
    initialHorizontal_ = spin(initialGroup, "actionInitialHorizontalSpin", -1220,
                              650, 0, QStringLiteral(" ×0.1 mm"));
    initialLift_ = spin(initialGroup, "actionInitialLiftSpin", 0, 1500, 0,
                        QStringLiteral(" ×0.1 mm"));
    initialTurret_ = spin(initialGroup, "actionInitialTurretSpin", 0, 3600, 0,
                          QStringLiteral(" ×0.1°"));
    initialGripper_ = new QComboBox(initialGroup);
    initialGripper_->addItem(QStringLiteral("张开"), 1);
    initialGripper_->addItem(QStringLiteral("闭合"), 0);
    initialPlatform_ = new QComboBox(initialGroup);
    for (int position = 1; position <= 3; ++position) {
        initialPlatform_->addItem(QStringLiteral("位置 %1").arg(position),
                                  position);
    }
    gripperOpenAngle_ = spin(initialGroup, "actionGripperOpenAngleSpin", 0, 270,
                             70, QStringLiteral("°"));
    gripperCloseAngle_ = spin(initialGroup, "actionGripperCloseAngleSpin", 0,
                              270, 35, QStringLiteral("°"));
    gripperSpeed_ = spin(initialGroup, "actionGripperSpeedSpin", 10, 1800, 1200,
                         QStringLiteral(" ×0.1°/s"));
    platformSpeed_ = spin(initialGroup, "actionPlatformSpeedSpin", 10, 1800, 1200,
                          QStringLiteral(" ×0.1°/s"));
    const int platformDefaults[] = {26, 146, 264};
    for (int index = 0; index < 3; ++index) {
        platformAngles_[index] =
            spin(initialGroup, qPrintable(QStringLiteral("actionPlatform%1AngleSpin")
                                              .arg(index + 1)),
                 0, 270, platformDefaults[index], QStringLiteral("°"));
    }
    initializeButton_ = button(initialGroup, QStringLiteral("确认人工初始姿态"),
                               "actionInitializeButton");
    initialLayout->addWidget(new QLabel(QStringLiteral("动作名"), initialGroup), 0, 0);
    initialLayout->addWidget(nameEdit_, 0, 1);
    initialLayout->addWidget(new QLabel(QStringLiteral("6号水平"), initialGroup), 0, 2);
    initialLayout->addWidget(initialHorizontal_, 0, 3);
    initialLayout->addWidget(new QLabel(QStringLiteral("5号升降"), initialGroup), 0, 4);
    initialLayout->addWidget(initialLift_, 0, 5);
    initialLayout->addWidget(new QLabel(QStringLiteral("转台"), initialGroup), 0, 6);
    initialLayout->addWidget(initialTurret_, 0, 7);
    initialLayout->addWidget(new QLabel(QStringLiteral("夹爪状态"), initialGroup), 1, 0);
    initialLayout->addWidget(initialGripper_, 1, 1);
    initialLayout->addWidget(gripperOpenAngle_, 1, 2);
    initialLayout->addWidget(gripperCloseAngle_, 1, 3);
    initialLayout->addWidget(new QLabel(QStringLiteral("平台状态/角度"), initialGroup), 1, 4);
    initialLayout->addWidget(initialPlatform_, 1, 5);
    initialLayout->addWidget(platformAngles_[0], 1, 6);
    initialLayout->addWidget(platformAngles_[1], 1, 7);
    initialLayout->addWidget(platformAngles_[2], 1, 8);
    initialLayout->addWidget(new QLabel(QStringLiteral("夹爪速度"), initialGroup), 2, 0);
    initialLayout->addWidget(gripperSpeed_, 2, 1);
    initialLayout->addWidget(new QLabel(QStringLiteral("平台速度"), initialGroup), 2, 2);
    initialLayout->addWidget(platformSpeed_, 2, 3);
    initialLayout->addWidget(initializeButton_, 0, 8);
    layout->addWidget(initialGroup);

    auto *positionGroup =
        new QGroupBox(QStringLiteral("初始姿态确认后的三轴微调"), this);
    auto *positionLayout = new QGridLayout(positionGroup);
    horizontalPositionSlider_ = new QSlider(Qt::Horizontal, positionGroup);
    horizontalPositionSlider_->setObjectName(
        QStringLiteral("actionHorizontalPositionSlider"));
    horizontalPositionSlider_->setRange(-1220, 650);
    liftPositionSlider_ = new QSlider(Qt::Horizontal, positionGroup);
    liftPositionSlider_->setObjectName(QStringLiteral("actionLiftPositionSlider"));
    liftPositionSlider_->setRange(0, 1500);
    turretPositionSlider_ = new QSlider(Qt::Horizontal, positionGroup);
    turretPositionSlider_->setObjectName(QStringLiteral("actionTurretPositionSlider"));
    turretPositionSlider_->setRange(0, 3600);
    horizontalPositionLabel_ = new QLabel(QStringLiteral("0 ×0.1 mm"), positionGroup);
    horizontalPositionLabel_->setObjectName(
        QStringLiteral("actionHorizontalPositionLabel"));
    liftPositionLabel_ = new QLabel(QStringLiteral("0 ×0.1 mm"), positionGroup);
    liftPositionLabel_->setObjectName(QStringLiteral("actionLiftPositionLabel"));
    turretPositionLabel_ = new QLabel(QStringLiteral("0 ×0.1°"), positionGroup);
    turretPositionLabel_->setObjectName(QStringLiteral("actionTurretPositionLabel"));
    const QStringList positionLabels{QStringLiteral("6号水平"),
                                     QStringLiteral("5号升降"),
                                     QStringLiteral("转台")};
    QWidget *positionSliders[] = {horizontalPositionSlider_, liftPositionSlider_,
                                  turretPositionSlider_};
    QWidget *positionValues[] = {horizontalPositionLabel_, liftPositionLabel_,
                                 turretPositionLabel_};
    for (int index = 0; index < 3; ++index) {
        positionLayout->addWidget(new QLabel(positionLabels[index], positionGroup),
                                  index, 0);
        positionLayout->addWidget(positionSliders[index], index, 1);
        positionLayout->addWidget(positionValues[index], index, 2);
    }
    layout->addWidget(positionGroup);

    connect(horizontalPositionSlider_, &QSlider::valueChanged, this,
            [this](int value) {
                horizontalPositionLabel_->setText(
                    QStringLiteral("%1 ×0.1 mm").arg(value));
            });
    connect(liftPositionSlider_, &QSlider::valueChanged, this,
            [this](int value) {
                liftPositionLabel_->setText(
                    QStringLiteral("%1 ×0.1 mm").arg(value));
            });
    connect(turretPositionSlider_, &QSlider::valueChanged, this,
            [this](int value) {
                turretPositionLabel_->setText(
                    QStringLiteral("%1 ×0.1°").arg(value));
            });
    connect(horizontalPositionSlider_, &QSlider::sliderReleased, this,
            [this] {
                requestManualPoseAdjustment(0, horizontalPositionSlider_->value());
            });
    connect(liftPositionSlider_, &QSlider::sliderReleased, this, [this] {
        requestManualPoseAdjustment(1, liftPositionSlider_->value());
    });
    connect(turretPositionSlider_, &QSlider::sliderReleased, this, [this] {
        requestManualPoseAdjustment(2, turretPositionSlider_->value());
    });

    auto *poseGroup = new QGroupBox(QStringLiteral("组合姿态步骤"), this);
    auto *poseLayout = new QGridLayout(poseGroup);
    poseHorizontal_ = spin(poseGroup, "actionPoseHorizontalSpin", -1220, 650, 0);
    poseLift_ = spin(poseGroup, "actionPoseLiftSpin", 0, 1500, 0);
    poseTurret_ = spin(poseGroup, "actionPoseTurretSpin", 0, 3600, 0);
    horizontalRpm_ = spin(poseGroup, "actionHorizontalRpmSpin", 10, 2000, 30,
                          QStringLiteral(" RPM"));
    horizontalAccel_ = spin(poseGroup, "actionHorizontalAccelSpin", 1, 240, 50);
    liftRpm_ = spin(poseGroup, "actionLiftRpmSpin", 10, 2000, 30,
                    QStringLiteral(" RPM"));
    liftAccel_ = spin(poseGroup, "actionLiftAccelSpin", 1, 240, 50);
    turretSpeed_ = spin(poseGroup, "actionTurretSpeedSpin", 10, 1800, 1200,
                        QStringLiteral(" ×0.1°/s"));
    stepWait_ = spin(poseGroup, "actionStepWaitSpin", 0, 60000, 0,
                     QStringLiteral(" ms"));
    auto *addPose = button(poseGroup, QStringLiteral("添加姿态"),
                           "actionAddPoseButton");
    const QStringList labels{QStringLiteral("水平"), QStringLiteral("升降"),
                             QStringLiteral("转台"), QStringLiteral("水平转速"),
                             QStringLiteral("水平加速"), QStringLiteral("升降转速"),
                             QStringLiteral("升降加速"), QStringLiteral("转台速度"),
                             QStringLiteral("到位等待")};
    QWidget *fields[] = {poseHorizontal_, poseLift_, poseTurret_, horizontalRpm_,
                         horizontalAccel_, liftRpm_, liftAccel_, turretSpeed_,
                         stepWait_};
    for (int index = 0; index < labels.size(); ++index) {
        poseLayout->addWidget(new QLabel(labels[index], poseGroup), 0, index);
        poseLayout->addWidget(fields[index], 1, index);
    }
    poseLayout->addWidget(addPose, 1, labels.size());
    layout->addWidget(poseGroup);

    auto *otherGroup = new QGroupBox(QStringLiteral("独立步骤"), this);
    auto *otherLayout = new QHBoxLayout(otherGroup);
    gripperStep_ = new QComboBox(otherGroup);
    gripperStep_->addItem(QStringLiteral("张开"), 1);
    gripperStep_->addItem(QStringLiteral("闭合"), 0);
    auto *addGripper = button(otherGroup, QStringLiteral("添加夹爪"),
                              "actionAddGripperButton");
    platformStep_ = new QComboBox(otherGroup);
    for (int position = 1; position <= 3; ++position) {
        platformStep_->addItem(QStringLiteral("平台 %1").arg(position), position);
    }
    auto *addPlatform = button(otherGroup, QStringLiteral("添加平台"),
                               "actionAddPlatformButton");
    servoChannel_ = spin(otherGroup, "actionServoChannelSpin", 2, 4, 2);
    servoAngle_ = spin(otherGroup, "actionServoAngleSpin", 0, 360, 90,
                       QStringLiteral("°"));
    auto *addServo = button(otherGroup, QStringLiteral("添加舵机"),
                            "actionAddServoButton");
    waitOnly_ = spin(otherGroup, "actionWaitSpin", 0, 60000, 500,
                     QStringLiteral(" ms"));
    auto *addWait = button(otherGroup, QStringLiteral("添加等待"),
                           "actionAddWaitButton");
    const QList<QWidget *> otherControls{gripperStep_, addGripper, platformStep_,
                                         addPlatform, servoChannel_, servoAngle_,
                                         addServo, waitOnly_, addWait};
    for (QWidget *widget : otherControls) {
        otherLayout->addWidget(widget);
    }
    layout->addWidget(otherGroup);

    stepTable_ = new QTableWidget(0, 3, this);
    stepTable_->setObjectName(QStringLiteral("actionStepTable"));
    stepTable_->setHorizontalHeaderLabels(
        {QStringLiteral("序号"), QStringLiteral("动作"), QStringLiteral("到位等待")});
    stepTable_->horizontalHeader()->setStretchLastSection(true);
    stepTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stepTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(stepTable_, 1);

    auto *editRow = new QHBoxLayout;
    auto *remove = button(this, QStringLiteral("删除"), "actionRemoveStepButton");
    replaceTypeCombo_ = new QComboBox(this);
    replaceTypeCombo_->setObjectName(QStringLiteral("actionReplaceTypeCombo"));
    replaceTypeCombo_->addItem(QStringLiteral("姿态"),
                               static_cast<int>(MechanismStepType::Pose));
    replaceTypeCombo_->addItem(QStringLiteral("夹爪"),
                               static_cast<int>(MechanismStepType::Gripper));
    replaceTypeCombo_->addItem(QStringLiteral("平台"),
                               static_cast<int>(MechanismStepType::Platform));
    replaceTypeCombo_->addItem(QStringLiteral("舵机"),
                               static_cast<int>(MechanismStepType::Servo));
    replaceTypeCombo_->addItem(QStringLiteral("等待"),
                               static_cast<int>(MechanismStepType::Wait));
    replaceTypeCombo_->setToolTip(
        QStringLiteral("先选目标动作类型，再填写对应编辑区的参数"));
    replaceStepButton_ = button(this, QStringLiteral("替换选中动作"),
                                "actionReplaceStepButton");
    replaceStepButton_->setToolTip(
        QStringLiteral("用所选类型替换当前行，保留该行动作顺序和总数"));
    auto *moveUp = button(this, QStringLiteral("上移"), "actionMoveUpButton");
    auto *moveDown = button(this, QStringLiteral("下移"), "actionMoveDownButton");
    auto *load = button(this, QStringLiteral("打开 JSON"), "actionLoadButton");
    auto *save = button(this, QStringLiteral("保存 JSON"), "actionSaveButton");
    auto *exportC = button(this, QStringLiteral("导出 C 表"), "actionExportButton");
    const QList<QWidget *> editControls{remove, replaceTypeCombo_,
                                        replaceStepButton_, moveUp, moveDown,
                                        load, save, exportC};
    for (QWidget *widget : editControls) {
        editRow->addWidget(widget);
    }
    editRow->addStretch();
    layout->addLayout(editRow);

    auto *testRow = new QHBoxLayout;
    playSelectedButton_ = button(this, QStringLiteral("测试选中"),
                                 "actionPlaySelectedButton");
    playFromButton_ = button(this, QStringLiteral("从选中开始"),
                             "actionPlayFromButton");
    playAllButton_ = button(this, QStringLiteral("完整测试"),
                            "actionPlayAllButton");
    loopEnabled_ = new QCheckBox(QStringLiteral("循环模式"), this);
    loopEnabled_->setObjectName(QStringLiteral("actionLoopEnabledCheck"));
    loopCount_ = spin(this, "actionLoopCountSpin", 2, 999, 2,
                      QStringLiteral(" 次"));
    loopCount_->setEnabled(false);
    returnInitialButton_ = button(this, QStringLiteral("回到初始位置"),
                                  "actionReturnInitialButton");
    stopButton_ = button(this, QStringLiteral("停止"), "actionStopButton");
    const QList<QWidget *> testControls{
        playSelectedButton_, playFromButton_, playAllButton_, loopEnabled_,
        loopCount_, returnInitialButton_, stopButton_};
    for (QWidget *widget : testControls) {
        testRow->addWidget(widget);
    }
    stateLabel_ = new QLabel(QStringLiteral("未连接"), this);
    stateLabel_->setObjectName(QStringLiteral("actionRecorderStateLabel"));
    testRow->addWidget(stateLabel_, 1);
    layout->addLayout(testRow);

    connect(initializeButton_, &QPushButton::clicked, this, [this] {
        updateInitialState();
        pendingInitialPose_ = sequence_.initial.pose;
        initialGripperAngle_ = sequence_.initial.gripperOpen
                                   ? sequence_.initial.gripperOpenDeg
                                   : sequence_.initial.gripperCloseDeg;
        initialPlatformAngle_ =
            sequence_.initial.platformDeg[sequence_.initial.platform - 1];
        initialGripperSpeed_ = sequence_.initial.gripperDps10;
        initialPlatformSpeed_ = sequence_.initial.platformDps10;
        initialServoStage_ = 0;
        awaitingInitialConfirmation_ = true;
        confirmedPoseValid_ = false;
        manualAdjustmentPending_ = false;
        initialized_ = false;
        updateControls();
        emit initializationRequested(sequence_.initial.pose);
        stateLabel_->setText(QStringLiteral("等待固件确认初始姿态"));
    });
    connect(addPose, &QPushButton::clicked, this, [this] {
        MechanismStep step;
        step.type = MechanismStepType::Pose;
        step.pose = editedPose();
        step.waitMs = stepWait_->value();
        sequence_.steps.push_back(step);
        refreshTable();
    });
    connect(addGripper, &QPushButton::clicked, this, [this] {
        MechanismStep step;
        step.type = MechanismStepType::Gripper;
        step.value = gripperStep_->currentData().toInt();
        sequence_.steps.push_back(step);
        refreshTable();
    });
    connect(addPlatform, &QPushButton::clicked, this, [this] {
        MechanismStep step;
        step.type = MechanismStepType::Platform;
        step.value = platformStep_->currentData().toInt();
        sequence_.steps.push_back(step);
        refreshTable();
    });
    connect(addServo, &QPushButton::clicked, this, [this] {
        if (servoChannel_->value() != 4 && servoAngle_->value() > 270) {
            showError(QStringLiteral("舵机 2/3 角度不能超过 270°"));
            return;
        }
        MechanismStep step;
        step.type = MechanismStepType::Servo;
        step.channel = servoChannel_->value();
        step.value = servoAngle_->value();
        sequence_.steps.push_back(step);
        refreshTable();
    });
    connect(addWait, &QPushButton::clicked, this, [this] {
        MechanismStep step;
        step.type = MechanismStepType::Wait;
        step.waitMs = waitOnly_->value();
        sequence_.steps.push_back(step);
        refreshTable();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = stepTable_->currentRow();
        if (row >= 0) sequence_.steps.removeAt(row);
        refreshTable();
    });
    connect(replaceStepButton_, &QPushButton::clicked, this,
            &MechanismActionPage::replaceSelectedStep);
    connect(stepTable_, &QTableWidget::currentCellChanged, this,
            [this](int, int, int, int) {
                loadSelectedStep();
                updateControls();
            });
    connect(moveUp, &QPushButton::clicked, this, [this] {
        const int row = stepTable_->currentRow();
        if (row > 0) {
            sequence_.steps.swapItemsAt(row, row - 1);
            refreshTable();
            stepTable_->selectRow(row - 1);
        }
    });
    connect(moveDown, &QPushButton::clicked, this, [this] {
        const int row = stepTable_->currentRow();
        if (row >= 0 && row + 1 < sequence_.steps.size()) {
            sequence_.steps.swapItemsAt(row, row + 1);
            refreshTable();
            stepTable_->selectRow(row + 1);
        }
    });
    connect(load, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开动作"), {}, QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) return;
        MechanismSequence loaded;
        QString error;
        if (!loadMechanismSequence(path, &loaded, &error)) {
            showError(error);
            return;
        }
        sequence_ = loaded;
        applySequenceToEditors();
        refreshTable();
        setMechanismInitialized(false);
        stateLabel_->setText(QStringLiteral("动作已载入，请人工复位后重新初始化"));
    });
    connect(save, &QPushButton::clicked, this, [this] {
        updateInitialState();
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存动作"), sequence_.name + QStringLiteral(".json"),
            QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) return;
        QString error;
        if (!saveMechanismSequence(path, sequence_, &error)) showError(error);
        else stateLabel_->setText(QStringLiteral("JSON 已保存"));
    });
    connect(exportC, &QPushButton::clicked, this, [this] {
        updateInitialState();
        QString error;
        const QString source = exportMechanismActionC(sequence_, &error);
        if (source.isEmpty()) {
            showError(error);
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出 C 动作表"), sequence_.name + QStringLiteral(".c"),
            QStringLiteral("C source (*.c)"));
        if (path.isEmpty()) return;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(source.toUtf8()) < 0 || !file.commit()) {
            showError(file.errorString());
        } else {
            stateLabel_->setText(QStringLiteral("C 动作表已导出"));
        }
    });
    connect(playSelectedButton_, &QPushButton::clicked, this, [this] {
        const int row = stepTable_->currentRow();
        if (row >= 0) startReplay({sequence_.steps[row]}, QStringLiteral("测试选中步骤"));
    });
    connect(playFromButton_, &QPushButton::clicked, this, [this] {
        const int row = stepTable_->currentRow();
        if (row >= 0) startReplay(sequence_.steps.mid(row), QStringLiteral("从选中步骤测试"));
    });
    connect(playAllButton_, &QPushButton::clicked, this, [this] {
        const int cycles = loopEnabled_->isChecked() ? loopCount_->value() : 1;
        startReplay(sequence_.steps, QStringLiteral("完整测试"), cycles);
    });
    connect(loopEnabled_, &QCheckBox::toggled, this, [this](bool enabled) {
        loopCount_->setEnabled(enabled && !replaying_);
    });
    connect(returnInitialButton_, &QPushButton::clicked, this, [this] {
        updateInitialState();
        MechanismStep pose;
        pose.type = MechanismStepType::Pose;
        pose.pose = sequence_.initial.pose;
        MechanismStep gripper;
        gripper.type = MechanismStepType::Gripper;
        gripper.value = sequence_.initial.gripperOpen ? 1 : 0;
        MechanismStep platform;
        platform.type = MechanismStepType::Platform;
        platform.value = sequence_.initial.platform;
        startReplay({pose, gripper, platform}, QStringLiteral("回到初始位置"));
    });
    connect(stopButton_, &QPushButton::clicked, this, [this] {
        cancelReplay();
        initialized_ = false;
        confirmedPoseValid_ = false;
        awaitingInitialConfirmation_ = false;
        manualAdjustmentPending_ = false;
        emit stopRequested();
        stateLabel_->setText(QStringLiteral("已请求停止；请人工复位后重新初始化"));
        updateControls();
    });
    setConnected(false);
}

MechanismPoseData MechanismActionPage::initialPose() const {
    MechanismPoseData pose = editedPose();
    pose.horizontalDmm = initialHorizontal_->value();
    pose.liftDmm = initialLift_->value();
    pose.turretDdeg = initialTurret_->value();
    return pose;
}

MechanismPoseData MechanismActionPage::editedPose() const {
    MechanismPoseData pose;
    pose.horizontalDmm = poseHorizontal_->value();
    pose.liftDmm = poseLift_->value();
    pose.turretDdeg = poseTurret_->value();
    pose.horizontalRpm = horizontalRpm_->value();
    pose.horizontalAccel = horizontalAccel_->value();
    pose.liftRpm = liftRpm_->value();
    pose.liftAccel = liftAccel_->value();
    pose.turretDps10 = turretSpeed_->value();
    return pose;
}

void MechanismActionPage::updateInitialState() {
    sequence_.name = nameEdit_->text().trimmed();
    sequence_.initial.pose = initialPose();
    sequence_.initial.gripperOpen = initialGripper_->currentData().toInt() != 0;
    sequence_.initial.platform = initialPlatform_->currentData().toInt();
    sequence_.initial.gripperOpenDeg = gripperOpenAngle_->value();
    sequence_.initial.gripperCloseDeg = gripperCloseAngle_->value();
    sequence_.initial.gripperDps10 = gripperSpeed_->value();
    sequence_.initial.platformDps10 = platformSpeed_->value();
    sequence_.loopEnabled = loopEnabled_->isChecked();
    sequence_.loopCount = loopCount_->value();
    for (int index = 0; index < 3; ++index) {
        sequence_.initial.platformDeg[index] = platformAngles_[index]->value();
    }
}

void MechanismActionPage::applySequenceToEditors() {
    const auto &initial = sequence_.initial;
    nameEdit_->setText(sequence_.name);
    initialHorizontal_->setValue(initial.pose.horizontalDmm);
    initialLift_->setValue(initial.pose.liftDmm);
    initialTurret_->setValue(initial.pose.turretDdeg);
    initialGripper_->setCurrentIndex(initial.gripperOpen ? 0 : 1);
    initialPlatform_->setCurrentIndex(initial.platform - 1);
    gripperOpenAngle_->setValue(initial.gripperOpenDeg);
    gripperCloseAngle_->setValue(initial.gripperCloseDeg);
    gripperSpeed_->setValue(initial.gripperDps10);
    platformSpeed_->setValue(initial.platformDps10);
    loopEnabled_->setChecked(sequence_.loopEnabled);
    loopCount_->setValue(sequence_.loopCount);
    for (int index = 0; index < 3; ++index) {
        platformAngles_[index]->setValue(initial.platformDeg[index]);
    }
}

void MechanismActionPage::loadSelectedStep() {
    const int row = stepTable_->currentRow();
    if (row < 0 || row >= sequence_.steps.size()) return;

    const MechanismStep &step = sequence_.steps[row];
    replaceTypeCombo_->setCurrentIndex(
        replaceTypeCombo_->findData(static_cast<int>(step.type)));
    stepWait_->setValue(step.waitMs);
    switch (step.type) {
    case MechanismStepType::Pose:
        poseHorizontal_->setValue(step.pose.horizontalDmm);
        poseLift_->setValue(step.pose.liftDmm);
        poseTurret_->setValue(step.pose.turretDdeg);
        horizontalRpm_->setValue(step.pose.horizontalRpm);
        horizontalAccel_->setValue(step.pose.horizontalAccel);
        liftRpm_->setValue(step.pose.liftRpm);
        liftAccel_->setValue(step.pose.liftAccel);
        turretSpeed_->setValue(step.pose.turretDps10);
        break;
    case MechanismStepType::Gripper:
        gripperStep_->setCurrentIndex(gripperStep_->findData(step.value));
        break;
    case MechanismStepType::Platform:
        platformStep_->setCurrentIndex(platformStep_->findData(step.value));
        break;
    case MechanismStepType::Servo:
        servoChannel_->setValue(step.channel);
        servoAngle_->setValue(step.value);
        break;
    case MechanismStepType::Wait:
        waitOnly_->setValue(step.waitMs);
        break;
    }
}

void MechanismActionPage::replaceSelectedStep() {
    const int row = stepTable_->currentRow();
    if (row < 0 || row >= sequence_.steps.size()) return;

    MechanismStep replacement;
    replacement.type = static_cast<MechanismStepType>(
        replaceTypeCombo_->currentData().toInt());
    replacement.waitMs = sequence_.steps[row].waitMs;
    switch (replacement.type) {
    case MechanismStepType::Pose:
        replacement.pose = editedPose();
        replacement.waitMs = stepWait_->value();
        break;
    case MechanismStepType::Gripper:
        replacement.value = gripperStep_->currentData().toInt();
        break;
    case MechanismStepType::Platform:
        replacement.value = platformStep_->currentData().toInt();
        break;
    case MechanismStepType::Servo:
        if (servoChannel_->value() != 4 && servoAngle_->value() > 270) {
            showError(QStringLiteral("舵机 2/3 角度不能超过 270°"));
            return;
        }
        replacement.channel = servoChannel_->value();
        replacement.value = servoAngle_->value();
        break;
    case MechanismStepType::Wait:
        replacement.waitMs = waitOnly_->value();
        break;
    }
    sequence_.steps[row] = replacement;
    refreshTable();
    stepTable_->selectRow(row);
    loadSelectedStep();
}

void MechanismActionPage::requestManualPoseAdjustment(int axis, int value) {
    if (!connected_ || !initialized_ || !confirmedPoseValid_ || replaying_ ||
        manualAdjustmentPending_) {
        if (confirmedPoseValid_) updatePositionSliders(confirmedPose_);
        return;
    }

    MechanismPoseData target = confirmedPose_;
    if (axis == 0) target.horizontalDmm = value;
    else if (axis == 1) target.liftDmm = value;
    else if (axis == 2) target.turretDdeg = value;
    else return;

    if (target.horizontalDmm == confirmedPose_.horizontalDmm &&
        target.liftDmm == confirmedPose_.liftDmm &&
        target.turretDdeg == confirmedPose_.turretDdeg) {
        updatePositionSliders(confirmedPose_);
        return;
    }
    if (!confirmHighSpeed(target)) {
        updatePositionSliders(confirmedPose_);
        return;
    }

    manualAdjustmentPending_ = true;
    stateLabel_->setText(QStringLiteral("正在执行三轴位置微调"));
    emit poseRequested(target);
    updateControls();
}

void MechanismActionPage::updatePositionSliders(
    const MechanismPoseData &pose) {
    if (!horizontalPositionSlider_->isSliderDown())
        horizontalPositionSlider_->setValue(pose.horizontalDmm);
    if (!liftPositionSlider_->isSliderDown())
        liftPositionSlider_->setValue(pose.liftDmm);
    if (!turretPositionSlider_->isSliderDown())
        turretPositionSlider_->setValue(pose.turretDdeg);
}

QString MechanismActionPage::stepDescription(const MechanismStep &step) const {
    switch (step.type) {
    case MechanismStepType::Pose:
        return QStringLiteral("姿态 H=%1 L=%2 T=%3；%4/%5 RPM")
            .arg(step.pose.horizontalDmm).arg(step.pose.liftDmm)
            .arg(step.pose.turretDdeg).arg(step.pose.horizontalRpm)
            .arg(step.pose.liftRpm);
    case MechanismStepType::Gripper:
        return step.value ? QStringLiteral("夹爪张开") : QStringLiteral("夹爪闭合");
    case MechanismStepType::Platform:
        return QStringLiteral("平台位置 %1").arg(step.value);
    case MechanismStepType::Servo:
        return QStringLiteral("舵机 %1 → %2°").arg(step.channel).arg(step.value);
    case MechanismStepType::Wait:
        return QStringLiteral("等待");
    }
    return {};
}

void MechanismActionPage::refreshTable() {
    stepTable_->setRowCount(sequence_.steps.size());
    for (int row = 0; row < sequence_.steps.size(); ++row) {
        const auto &step = sequence_.steps[row];
        stepTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        stepTable_->setItem(row, 1, new QTableWidgetItem(stepDescription(step)));
        stepTable_->setItem(row, 2,
                            new QTableWidgetItem(QStringLiteral("%1 ms").arg(step.waitMs)));
    }
    updateControls();
}

void MechanismActionPage::setConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        initialized_ = false;
        awaitingInitialConfirmation_ = false;
        confirmedPoseValid_ = false;
        manualAdjustmentPending_ = false;
        initialServoStage_ = 0;
        cancelReplay();
        stateLabel_->setText(QStringLiteral("未连接"));
    }
    updateControls();
}

void MechanismActionPage::setMechanismInitialized(bool initialized) {
    initialized_ = initialized;
    if (initialized_ && awaitingInitialConfirmation_) {
        confirmedPose_ = pendingInitialPose_;
        confirmedPoseValid_ = true;
        awaitingInitialConfirmation_ = false;
        updatePositionSliders(confirmedPose_);
    } else if (!initialized_) {
        awaitingInitialConfirmation_ = false;
        confirmedPoseValid_ = false;
        manualAdjustmentPending_ = false;
        initialServoStage_ = 0;
        cancelReplay();
    }
    stateLabel_->setText(initialized_ && initialServoStage_ != 0
                             ? QStringLiteral("初始姿态已确认，正在调整舵机") :
                         initialized_ ? QStringLiteral("初始姿态已确认，可测试动作")
                                      : QStringLiteral("位置已失效，请人工复位后重新初始化"));
    updateControls();
}

void MechanismActionPage::setMechanismEstimate(MechanismPoseData pose,
                                                QString state) {
    if (confirmedPoseValid_) {
        confirmedPose_.horizontalDmm = pose.horizontalDmm;
        confirmedPose_.liftDmm = pose.liftDmm;
        confirmedPose_.turretDdeg = pose.turretDdeg;
        if (!manualAdjustmentPending_ && !replaying_) {
            updatePositionSliders(confirmedPose_);
        }
    }
    if (initialServoStage_ == 0) {
        stateLabel_->setText(QStringLiteral("%1：H=%2 L=%3 T=%4")
                                 .arg(state).arg(pose.horizontalDmm)
                                 .arg(pose.liftDmm).arg(pose.turretDdeg));
    }
}

void MechanismActionPage::setMechanismCompleted(MechanismPoseData pose) {
    setMechanismEstimate(pose, QStringLiteral("到位"));
    manualAdjustmentPending_ = false;
    if (confirmedPoseValid_) updatePositionSliders(confirmedPose_);
    if (replaying_ && replaySteps_[replayIndex_].type == MechanismStepType::Pose) {
        finishCurrentStep();
    }
    updateControls();
}

void MechanismActionPage::handleDeviceLine(const QString &line) {
    if (awaitingInitialConfirmation_ &&
        line.startsWith(QStringLiteral("MECH INIT "))) {
        initialServoStage_ = 1;
        setMechanismInitialized(true);
        emit commandRequested(QStringLiteral("servo 2 %1 %2")
                                  .arg(initialGripperAngle_).arg(initialGripperSpeed_));
        return;
    }
    if (initialServoStage_ == 1 &&
        (line.startsWith(QStringLiteral("DONE servo=2")) ||
         line.startsWith(QStringLiteral("OK servo=2")))) {
        initialServoStage_ = 2;
        emit commandRequested(QStringLiteral("servo 3 %1 %2")
                                  .arg(initialPlatformAngle_).arg(initialPlatformSpeed_));
        return;
    }
    if (initialServoStage_ == 2 &&
        (line.startsWith(QStringLiteral("DONE servo=3")) ||
         line.startsWith(QStringLiteral("OK servo=3")))) {
        initialServoStage_ = 0;
        stateLabel_->setText(QStringLiteral("初始姿态与舵机已指令到位，可测试动作"));
        updateControls();
        return;
    }
    if (replaying_ && replayIndex_ < replaySteps_.size() &&
        replaySteps_[replayIndex_].type != MechanismStepType::Pose &&
        replaySteps_[replayIndex_].type != MechanismStepType::Wait &&
        (line.startsWith(QStringLiteral("DONE servo=")) ||
         line.startsWith(QStringLiteral("OK servo=")))) {
        finishCurrentStep();
    }
}

void MechanismActionPage::showError(const QString &error) {
    if (initialServoStage_ != 0) {
        initialized_ = false;
        confirmedPoseValid_ = false;
        initialServoStage_ = 0;
    }
    awaitingInitialConfirmation_ = false;
    manualAdjustmentPending_ = false;
    cancelReplay();
    if (confirmedPoseValid_) updatePositionSliders(confirmedPose_);
    stateLabel_->setText(QStringLiteral("错误：%1").arg(error));
    updateControls();
}

void MechanismActionPage::updateControls() {
    initializeButton_->setEnabled(connected_ && !replaying_ &&
                                  initialServoStage_ == 0);
    const bool canReplay = connected_ && initialized_ && !replaying_ &&
                           initialServoStage_ == 0;
    const bool canAdjust = canReplay && confirmedPoseValid_ &&
                           !manualAdjustmentPending_;
    horizontalPositionSlider_->setEnabled(canAdjust);
    liftPositionSlider_->setEnabled(canAdjust);
    turretPositionSlider_->setEnabled(canAdjust);
    const bool canEditSelectedStep = !replaying_ &&
                                     stepTable_->currentRow() >= 0;
    replaceTypeCombo_->setEnabled(canEditSelectedStep);
    replaceStepButton_->setEnabled(canEditSelectedStep);
    playSelectedButton_->setEnabled(canReplay && stepTable_->currentRow() >= 0);
    playFromButton_->setEnabled(canReplay && stepTable_->currentRow() >= 0);
    playAllButton_->setEnabled(canReplay && !sequence_.steps.isEmpty());
    loopEnabled_->setEnabled(!replaying_);
    loopCount_->setEnabled(!replaying_ && loopEnabled_->isChecked());
    returnInitialButton_->setEnabled(canReplay);
    stopButton_->setEnabled(connected_);
}

bool MechanismActionPage::confirmHighSpeed(const MechanismPoseData &pose) {
    if (pose.horizontalRpm <= 120 && pose.liftRpm <= 120) return true;
    return QMessageBox::warning(
               this, QStringLiteral("高速动作确认"),
               QStringLiteral("该姿态包含超过 120 RPM 的电机速度。确认机构架空、行程安全并继续？"),
               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) ==
           QMessageBox::Yes;
}

void MechanismActionPage::startReplay(QVector<MechanismStep> steps,
                                      const QString &description,
                                      int cycleCount) {
    if (!connected_ || !initialized_ || replaying_ || steps.isEmpty()) return;
    replaySteps_ = std::move(steps);
    replayIndex_ = 0;
    replayCycle_ = 1;
    replayCycleCount_ = cycleCount;
    replayDescription_ = description;
    replaying_ = true;
    stateLabel_->setText(description);
    updateControls();
    dispatchCurrentStep();
}

void MechanismActionPage::dispatchCurrentStep() {
    if (!replaying_) return;
    if (replayIndex_ >= replaySteps_.size()) {
        if (replayCycle_ < replayCycleCount_) {
            ++replayCycle_;
            replayIndex_ = 0;
            stateLabel_->setText(QStringLiteral("%1：第 %2/%3 轮")
                                     .arg(replayDescription_)
                                     .arg(replayCycle_)
                                     .arg(replayCycleCount_));
            dispatchCurrentStep();
            return;
        }
        replaying_ = false;
        stateLabel_->setText(QStringLiteral("动作测试完成"));
        updateControls();
        return;
    }
    const MechanismStep &step = replaySteps_[replayIndex_];
    const QString progress = replayCycleCount_ > 1
        ? QStringLiteral("第 %1/%2 轮，执行 %3/%4：%5")
              .arg(replayCycle_).arg(replayCycleCount_)
              .arg(replayIndex_ + 1).arg(replaySteps_.size())
              .arg(stepDescription(step))
        : QStringLiteral("执行 %1/%2：%3")
              .arg(replayIndex_ + 1).arg(replaySteps_.size())
              .arg(stepDescription(step));
    stateLabel_->setText(progress);
    if (step.type == MechanismStepType::Pose) {
        if (!confirmHighSpeed(step.pose)) {
            cancelReplay();
            stateLabel_->setText(QStringLiteral("已取消高速动作"));
            return;
        }
        emit poseRequested(step.pose);
        return;
    }
    if (step.type == MechanismStepType::Wait) {
        waitTimer_.start(step.waitMs);
        return;
    }
    updateInitialState();
    int channel = step.channel;
    int angle = step.value;
    if (step.type == MechanismStepType::Gripper) {
        channel = 2;
        angle = step.value ? sequence_.initial.gripperOpenDeg
                           : sequence_.initial.gripperCloseDeg;
    } else if (step.type == MechanismStepType::Platform) {
        channel = 3;
        angle = sequence_.initial.platformDeg[step.value - 1];
    }
    int speed = sequence_.initial.pose.turretDps10;
    if (channel == 2) speed = sequence_.initial.gripperDps10;
    else if (channel == 3) speed = sequence_.initial.platformDps10;
    emit commandRequested(QStringLiteral("servo %1 %2 %3")
                              .arg(channel).arg(angle).arg(speed));
}

void MechanismActionPage::finishCurrentStep() {
    if (!replaying_ || replayIndex_ >= replaySteps_.size()) return;
    const int waitMs = replaySteps_[replayIndex_].waitMs;
    if (waitMs > 0) {
        waitTimer_.start(waitMs);
    } else {
        ++replayIndex_;
        dispatchCurrentStep();
    }
}

void MechanismActionPage::cancelReplay() {
    waitTimer_.stop();
    replaySteps_.clear();
    replayIndex_ = 0;
    replayCycle_ = 1;
    replayCycleCount_ = 1;
    replayDescription_.clear();
    replaying_ = false;
    updateControls();
}
