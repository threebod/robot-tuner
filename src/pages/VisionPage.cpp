#include "pages/VisionPage.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

VisionPage::VisionPage(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    auto *automatic = new QGroupBox(QStringLiteral("自动视觉微调"), this);
    auto *automaticLayout = new QFormLayout(automatic);
    modeCombo_ = new QComboBox(automatic);
    modeCombo_->setObjectName(QStringLiteral("visionModeCombo"));
    modeCombo_->addItem(QStringLiteral("物料"), 1);
    modeCombo_->addItem(QStringLiteral("圆环"), 2);
    targetCombo_ = new QComboBox(automatic);
    targetCombo_->setObjectName(QStringLiteral("visionTargetCombo"));
    automaticLayout->addRow(QStringLiteral("目标类型"), modeCombo_);
    automaticLayout->addRow(QStringLiteral("目标"), targetCombo_);
    forwardScaleSpin_ = new QDoubleSpinBox(automatic);
    forwardScaleSpin_->setObjectName(QStringLiteral("visionForwardScaleSpinBox"));
    forwardScaleSpin_->setRange(0.05, 2.0);
    forwardScaleSpin_->setDecimals(3);
    forwardScaleSpin_->setSingleStep(0.01);
    rightScaleSpin_ = new QDoubleSpinBox(automatic);
    rightScaleSpin_->setObjectName(QStringLiteral("visionRightScaleSpinBox"));
    rightScaleSpin_->setRange(0.05, 2.0);
    rightScaleSpin_->setDecimals(3);
    rightScaleSpin_->setSingleStep(0.01);
    applyScaleButton_ = new QPushButton(QStringLiteral("应用当前圆环比例"), automatic);
    applyScaleButton_->setObjectName(QStringLiteral("visionApplyScaleButton"));
    scaleLabel_ = new QLabel(QStringLiteral("初值来自圆环 2 粗测；待应用"), automatic);
    automaticLayout->addRow(QStringLiteral("前后 mm/像素（向右→后退）"), forwardScaleSpin_);
    automaticLayout->addRow(QStringLiteral("左右 mm/像素（向下→左移）"), rightScaleSpin_);
    automaticLayout->addRow(applyScaleButton_, scaleLabel_);
    auto *automaticButtons = new QHBoxLayout;
    startButton_ = new QPushButton(QStringLiteral("开始自动对准"), automatic);
    startButton_->setObjectName(QStringLiteral("visionStartButton"));
    pauseButton_ = new QPushButton(QStringLiteral("暂停并接管"), automatic);
    pauseButton_->setObjectName(QStringLiteral("visionPauseButton"));
    auto *statusButton = new QPushButton(QStringLiteral("刷新状态"), automatic);
    statusButton->setObjectName(QStringLiteral("visionStatusButton"));
    automaticButtons->addWidget(startButton_);
    automaticButtons->addWidget(pauseButton_);
    automaticButtons->addWidget(statusButton);
    automaticLayout->addRow(automaticButtons);
    layout->addWidget(automatic);

    auto *manual = new QGroupBox(QStringLiteral("人工小步接管"), this);
    auto *manualLayout = new QVBoxLayout(manual);
    auto *settings = new QHBoxLayout;
    stepCombo_ = new QComboBox(manual);
    stepCombo_->setObjectName(QStringLiteral("visionStepCombo"));
    for (int step : {5, 10, 20}) {
        stepCombo_->addItem(QStringLiteral("%1 mm").arg(step), step);
    }
    rpmSpin_ = new QSpinBox(manual);
    rpmSpin_->setObjectName(QStringLiteral("visionRpmSpinBox"));
    rpmSpin_->setRange(10, 30);
    rpmSpin_->setValue(20);
    rpmSpin_->setSuffix(QStringLiteral(" RPM"));
    settings->addWidget(new QLabel(QStringLiteral("步长"), manual));
    settings->addWidget(stepCombo_);
    settings->addWidget(new QLabel(QStringLiteral("速度"), manual));
    settings->addWidget(rpmSpin_);
    settings->addStretch();
    manualLayout->addLayout(settings);
    jogControls_ = new QWidget(manual);
    auto *jogLayout = new QGridLayout(jogControls_);
    auto addJog = [this, jogLayout](const QString &text, const QString &name,
                                    int row, int column, int forward, int right) {
        auto *button = new QPushButton(text, jogControls_);
        button->setObjectName(name);
        jogLayout->addWidget(button, row, column);
        connect(button, &QPushButton::clicked, this,
                [this, forward, right] { requestJog(forward, right); });
    };
    addJog(QStringLiteral("前"), QStringLiteral("visionForwardButton"),
           0, 1, 1, 0);
    addJog(QStringLiteral("左"), QStringLiteral("visionLeftButton"),
           1, 0, 0, -1);
    addJog(QStringLiteral("后"), QStringLiteral("visionBackButton"),
           1, 1, -1, 0);
    addJog(QStringLiteral("右"), QStringLiteral("visionRightButton"),
           1, 2, 0, 1);
    manualLayout->addWidget(jogControls_);
    layout->addWidget(manual);

    stateLabel_ = new QLabel(QStringLiteral("状态：未连接"), this);
    stateLabel_->setObjectName(QStringLiteral("visionStateLabel"));
    sampleLabel_ = new QLabel(QStringLiteral("尚无视觉样本"), this);
    sampleLabel_->setObjectName(QStringLiteral("visionSampleLabel"));
    sampleLabel_->setWordWrap(true);
    layout->addWidget(stateLabel_);
    layout->addWidget(sampleLabel_);
    layout->addStretch();

    connect(modeCombo_, &QComboBox::currentIndexChanged, this,
            [this] { rebuildTargets(); });
    connect(targetCombo_, &QComboBox::currentIndexChanged, this,
            [this] { refreshScaleInputs(); });
    connect(forwardScaleSpin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        const int ring = targetCombo_->currentData().toInt();
        if (modeCombo_->currentData().toInt() == 2 && ring >= 1 && ring <= 3) {
            forwardScales_[ring - 1] = qRound(value * 1000.0);
            scalesApplied_[ring - 1] = false;
            scaleLabel_->setText(QStringLiteral("比例已修改，待应用"));
            refreshControls();
        }
    });
    connect(rightScaleSpin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        const int ring = targetCombo_->currentData().toInt();
        if (modeCombo_->currentData().toInt() == 2 && ring >= 1 && ring <= 3) {
            rightScales_[ring - 1] = qRound(value * 1000.0);
            scalesApplied_[ring - 1] = false;
            scaleLabel_->setText(QStringLiteral("比例已修改，待应用"));
            refreshControls();
        }
    });
    connect(applyScaleButton_, &QPushButton::clicked, this, [this] {
        const int ring = targetCombo_->currentData().toInt();
        if (ring >= 1 && ring <= 3) {
            emit ringScaleRequested(ring, forwardScales_[ring - 1],
                                    rightScales_[ring - 1]);
            scaleLabel_->setText(QStringLiteral("等待主控确认"));
        }
    });
    connect(startButton_, &QPushButton::clicked, this, [this] {
        if (modeCombo_->currentData().toInt() == 1) {
            emit materialPickupRequested(targetCombo_->currentData().toInt());
        } else {
            emit ringAlignmentRequested(targetCombo_->currentData().toInt());
        }
    });
    connect(pauseButton_, &QPushButton::clicked, this,
            &VisionPage::pauseRequested);
    connect(statusButton, &QPushButton::clicked, this,
            &VisionPage::statusRequested);
    rebuildTargets();
    refreshControls();
}

void VisionPage::setConnected(bool connected) {
    connected_ = connected;
    if (!connected) {
        scalesApplied_.fill(false);
        running_ = false;
        manualEnabled_ = false;
        stateLabel_->setText(QStringLiteral("状态：未连接"));
        scaleLabel_->setText(QStringLiteral("连接断开；请重新确认主控比例"));
    }
    refreshControls();
}

void VisionPage::setOtherMotionRunning(bool running) {
    otherMotionRunning_ = running;
    refreshControls();
}

void VisionPage::setVisionRunning(bool running) {
    running_ = running;
    if (running) manualEnabled_ = false;
    refreshControls();
}

void VisionPage::setVisionState(const QString &state) {
    running_ = state != QStringLiteral("IDLE") &&
               state != QStringLiteral("ALIGNED") &&
               state != QStringLiteral("FAILED") &&
               state != QStringLiteral("PAUSED") &&
               state != QStringLiteral("PICK_DONE");
    manualEnabled_ = state == QStringLiteral("PAUSED");
    stateLabel_->setText(state == QStringLiteral("PICK_DONE")
                             ? QStringLiteral("状态：夹取动作结束，请人工确认是否夹到")
                             : QStringLiteral("状态：%1").arg(state));
    refreshControls();
}

void VisionPage::setVisionSample(int du, int dv, double forwardMm,
                                 double rightMm, int quality, int iteration) {
    sampleLabel_->setText(
        QStringLiteral("像素误差：(%1, %2)；修正：前后 %3 mm、左右 %4 mm；"
                       "质量 %5；第 %6 次")
            .arg(du).arg(dv).arg(forwardMm, 0, 'f', 1)
            .arg(rightMm, 0, 'f', 1).arg(quality).arg(iteration));
}

void VisionPage::showVisionError(const QString &error) {
    running_ = false;
    manualEnabled_ = false;
    stateLabel_->setText(QStringLiteral("错误：%1").arg(error));
    if (scaleLabel_->text() == QStringLiteral("等待主控确认")) {
        scaleLabel_->setText(QStringLiteral("应用失败；请检查错误"));
    }
    refreshControls();
}

void VisionPage::setRingScaleApplied(int ring, int forwardMilli, int rightMilli) {
    if (ring < 1 || ring > 3) return;
    scalesApplied_[ring - 1] = forwardScales_[ring - 1] == forwardMilli &&
                               rightScales_[ring - 1] == rightMilli;
    if (modeCombo_->currentData().toInt() == 2 &&
        targetCombo_->currentData().toInt() == ring) {
        if (scalesApplied_[ring - 1]) {
            stateLabel_->setText(QStringLiteral("状态：圆环 %1 比例已应用，可开始对准").arg(ring));
            scaleLabel_->setText(QStringLiteral("圆环 %1 已应用：%2 / %3 mm/像素")
                                     .arg(ring)
                                     .arg(forwardMilli / 1000.0, 0, 'f', 3)
                                     .arg(rightMilli / 1000.0, 0, 'f', 3));
        } else {
            scaleLabel_->setText(QStringLiteral("主控已确认旧值；当前输入待应用"));
        }
    }
    refreshControls();
}

void VisionPage::rebuildTargets() {
    targetCombo_->clear();
    startButton_->setText(modeCombo_->currentData().toInt() == 1
                              ? QStringLiteral("识别指定颜色并夹取")
                              : QStringLiteral("开始自动对准"));
    if (modeCombo_->currentData().toInt() == 1) {
        const QStringList colors{QStringLiteral("红"), QStringLiteral("黄"),
                                 QStringLiteral("蓝"), QStringLiteral("绿"),
                                 QStringLiteral("黑"), QStringLiteral("浅蓝")};
        for (int index = 0; index < colors.size(); ++index) {
            targetCombo_->addItem(colors.at(index), index + 1);
        }
    } else {
        for (int ring = 1; ring <= 3; ++ring) {
            targetCombo_->addItem(QStringLiteral("圆环 %1").arg(ring), ring);
        }
    }
    refreshScaleInputs();
    refreshControls();
}

void VisionPage::refreshScaleInputs() {
    const int ring = targetCombo_->currentData().toInt();
    if (modeCombo_->currentData().toInt() == 2 && ring >= 1 && ring <= 3) {
        const QSignalBlocker forwardBlocker(forwardScaleSpin_);
        const QSignalBlocker rightBlocker(rightScaleSpin_);
        forwardScaleSpin_->setValue(forwardScales_[ring - 1] / 1000.0);
        rightScaleSpin_->setValue(rightScales_[ring - 1] / 1000.0);
        scaleLabel_->setText(scalesApplied_[ring - 1]
                                 ? QStringLiteral("圆环 %1 已应用；断电后需重新应用").arg(ring)
                                 : QStringLiteral("圆环 %1 待应用；初值来自圆环 2 粗测").arg(ring));
    }
    refreshControls();
}

void VisionPage::refreshControls() {
    const bool canScale = connected_ && !running_ && !manualEnabled_ &&
                          !otherMotionRunning_ &&
                          modeCombo_->currentData().toInt() == 2;
    forwardScaleSpin_->setEnabled(canScale);
    rightScaleSpin_->setEnabled(canScale);
    applyScaleButton_->setEnabled(canScale);
    scaleLabel_->setVisible(modeCombo_->currentData().toInt() == 2);
    forwardScaleSpin_->setVisible(modeCombo_->currentData().toInt() == 2);
    rightScaleSpin_->setVisible(modeCombo_->currentData().toInt() == 2);
    applyScaleButton_->setVisible(modeCombo_->currentData().toInt() == 2);
    const int ring = targetCombo_->currentData().toInt();
    const bool ringReady = modeCombo_->currentData().toInt() != 2 ||
                           (ring >= 1 && ring <= 3 && scalesApplied_[ring - 1]);
    startButton_->setEnabled(connected_ && !running_ && !otherMotionRunning_ &&
                             ringReady);
    pauseButton_->setEnabled(connected_ && running_);
    modeCombo_->setEnabled(connected_ && !running_);
    targetCombo_->setEnabled(connected_ && !running_);
    jogControls_->setEnabled(connected_ && manualEnabled_ && !running_ &&
                             !otherMotionRunning_);
    stepCombo_->setEnabled(connected_ && manualEnabled_ && !running_ &&
                           !otherMotionRunning_);
    rpmSpin_->setEnabled(connected_ && manualEnabled_ && !running_ &&
                         !otherMotionRunning_);
}

void VisionPage::requestJog(int forwardSign, int rightSign) {
    const int step = stepCombo_->currentData().toInt();
    emit jogRequested(forwardSign * step, rightSign * step, rpmSpin_->value());
}
