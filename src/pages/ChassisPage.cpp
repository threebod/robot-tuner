#include "pages/ChassisPage.h"

#include "widgets/TelemetryPlot.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace {

QWidget *fieldWithUnit(QAbstractSpinBox *control, const QString &unit,
                       QWidget *parent) {
    auto *field = new QWidget(parent);
    auto *layout = new QHBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(control);
    if (!unit.isEmpty()) {
        auto *unitLabel = new QLabel(unit, field);
        unitLabel->setObjectName(control->objectName() + QStringLiteral("Unit"));
        layout->addWidget(unitLabel);
    }
    return field;
}

QPushButton *makeButton(const QString &text, const QString &objectName,
                        QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    return button;
}

}  // namespace

ChassisPage::ChassisPage(QWidget *parent)
    : QWidget(parent), catalog_(ParameterCatalog::instance()) {
    auto *pageLayout = new QVBoxLayout(this);

    auto *ramOnlyNotice = new QLabel(
        QStringLiteral("参数仅写入 STM32 RAM；不会写入 Flash，设备重启后恢复固件默认值。"),
        this);
    ramOnlyNotice->setObjectName(QStringLiteral("ramOnlyNotice"));
    ramOnlyNotice->setWordWrap(true);
    pageLayout->addWidget(ramOnlyNotice);

    auto *pidGroup = new QGroupBox(QStringLiteral("PID（组 0x10）"), this);
    auto *pidLayout = new QVBoxLayout(pidGroup);
    auto *pidContentLayout = new QHBoxLayout;
    auto *pidSettingsLayout = new QVBoxLayout;
    auto *pidProfileRow = new QHBoxLayout;
    pidProfileRow->addWidget(new QLabel(QStringLiteral("PID 配置"), pidGroup));
    pidProfileCombo_ = new QComboBox(pidGroup);
    pidProfileCombo_->setObjectName(QStringLiteral("pidProfileCombo"));
    for (int profile = 0; profile < 5; ++profile) {
        pidProfileCombo_->addItem(QStringLiteral("配置 %1").arg(profile),
                                  profile);
    }
    pidProfileRow->addWidget(pidProfileCombo_);
    pidProfileRow->addStretch();
    pidSettingsLayout->addLayout(pidProfileRow);

    auto *pidFields = new QFormLayout;
    const QStringList pidObjectNames = {
        QStringLiteral("pidKpSpinBox"),
        QStringLiteral("pidKiSpinBox"),
        QStringLiteral("pidKdSpinBox"),
        QStringLiteral("pidIntegralLimitSpinBox"),
        QStringLiteral("pidOutputLimitSpinBox"),
    };
    for (int offset = 0; offset < pidObjectNames.size(); ++offset) {
        const ParameterSpec *spec = catalog_.find(
            static_cast<quint16>(0x1000 + offset));
        if (spec == nullptr) {
            continue;
        }
        pidSpecs_.push_back(spec);
        auto *control = createSpinBox(*spec, pidObjectNames.at(offset), pidGroup);
        pidControls_.push_back(control);
        interactiveWidgets_.push_back(control);
        addFieldRow(pidFields, *spec, control);
    }
    pidSettingsLayout->addLayout(pidFields);

    auto *pidButtons = new QHBoxLayout;
    pidReadButton_ = makeButton(QStringLiteral("读取 PID"),
                                 QStringLiteral("pidReadButton"), pidGroup);
    pidWriteButton_ = makeButton(QStringLiteral("写入 RAM"),
                                  QStringLiteral("pidWriteButton"), pidGroup);
    pidButtons->addWidget(pidReadButton_);
    pidButtons->addWidget(pidWriteButton_);
    pidButtons->addStretch();
    pidSettingsLayout->addLayout(pidButtons);
    interactiveWidgets_.push_back(pidProfileCombo_);
    interactiveWidgets_.push_back(pidReadButton_);
    interactiveWidgets_.push_back(pidWriteButton_);
    pidContentLayout->addLayout(pidSettingsLayout);

    auto *pidPlotsLayout = new QHBoxLayout;
    pidAnglePlot_ = new TelemetryPlot(2, 1000, pidGroup);
    pidAnglePlot_->setObjectName(QStringLiteral("pidAnglePlot"));
    pidAnglePlot_->setChannelNames(
        {QStringLiteral("目标角度"), QStringLiteral("实际角度")});
    pidOutputPlot_ = new TelemetryPlot(1, 1000, pidGroup);
    pidOutputPlot_->setObjectName(QStringLiteral("pidOutputPlot"));
    pidOutputPlot_->setChannelNames({QStringLiteral("控制输出")});
    auto *anglePlotGroup =
        new QGroupBox(QStringLiteral("目标 / 实际角度（°）"), pidGroup);
    auto *anglePlotLayout = new QVBoxLayout(anglePlotGroup);
    anglePlotLayout->addWidget(pidAnglePlot_);
    auto *outputPlotGroup =
        new QGroupBox(QStringLiteral("控制输出"), pidGroup);
    auto *outputPlotLayout = new QVBoxLayout(outputPlotGroup);
    outputPlotLayout->addWidget(pidOutputPlot_);
    pidPlotsLayout->addWidget(anglePlotGroup);
    pidPlotsLayout->addWidget(outputPlotGroup);
    pidContentLayout->addLayout(pidPlotsLayout, 1);
    pidLayout->addLayout(pidContentLayout);
    pageLayout->addWidget(pidGroup);

    auto *chassisGroup = new QGroupBox(QStringLiteral("底盘（组 0x20）"), this);
    auto *chassisLayout = new QVBoxLayout(chassisGroup);
    auto *chassisFields = new QFormLayout;
    const QStringList chassisObjectNames = {
        QStringLiteral("chassisVxSpinBox"),
        QStringLiteral("chassisVySpinBox"),
        QStringLiteral("chassisWSpinBox"),
        QStringLiteral("chassisDurationSpinBox"),
        QStringLiteral("chassisAccelerationSpinBox"),
    };
    const QVector<ParameterSpec> chassisSpecs = catalog_.group(kChassisGroup);
    for (int index = 0; index < chassisSpecs.size(); ++index) {
        const ParameterSpec &spec = chassisSpecs.at(index);
        auto *control = createSpinBox(spec, chassisObjectNames.at(index),
                                      chassisGroup);
        controls_.insert(spec.id, control);
        interactiveWidgets_.push_back(control);
        addFieldRow(chassisFields, spec, control);
    }
    chassisLayout->addLayout(chassisFields);
    auto *chassisButtons = new QHBoxLayout;
    chassisReadButton_ = makeButton(QStringLiteral("读取底盘"),
                                     QStringLiteral("chassisReadButton"),
                                     chassisGroup);
    chassisWriteButton_ = makeButton(QStringLiteral("写入 RAM"),
                                      QStringLiteral("chassisWriteButton"),
                                      chassisGroup);
    chassisButtons->addWidget(chassisReadButton_);
    chassisButtons->addWidget(chassisWriteButton_);
    chassisButtons->addStretch();
    chassisLayout->addLayout(chassisButtons);
    interactiveWidgets_.push_back(chassisReadButton_);
    interactiveWidgets_.push_back(chassisWriteButton_);
    pageLayout->addWidget(chassisGroup);

    auto *imuGroup = new QGroupBox(QStringLiteral("IMU 参数（组 0x40）"), this);
    auto *imuLayout = new QVBoxLayout(imuGroup);
    auto *imuFields = new QFormLayout;
    const QVector<ParameterSpec> imuSpecs = catalog_.group(kImuGroup);
    for (const ParameterSpec &spec : imuSpecs) {
        auto *control = createSpinBox(spec, QStringLiteral("imuRateSpinBox"),
                                      imuGroup);
        controls_.insert(spec.id, control);
        interactiveWidgets_.push_back(control);
        addFieldRow(imuFields, spec, control);
    }
    imuLayout->addLayout(imuFields);
    auto *imuButtons = new QHBoxLayout;
    imuReadButton_ = makeButton(QStringLiteral("读取 IMU 参数"),
                                 QStringLiteral("imuReadButton"), imuGroup);
    imuWriteButton_ = makeButton(QStringLiteral("写入 RAM"),
                                 QStringLiteral("imuWriteButton"), imuGroup);
    imuButtons->addWidget(imuReadButton_);
    imuButtons->addWidget(imuWriteButton_);
    imuButtons->addStretch();
    imuLayout->addLayout(imuButtons);
    interactiveWidgets_.push_back(imuReadButton_);
    interactiveWidgets_.push_back(imuWriteButton_);
    pageLayout->addWidget(imuGroup);

    auto *bottomButtons = new QHBoxLayout;
    restoreInitialButton_ = makeButton(
        QStringLiteral("恢复本次连接初值"),
        QStringLiteral("restoreInitialButton"), this);
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("parameterStatusLabel"));
    bottomButtons->addWidget(restoreInitialButton_);
    bottomButtons->addWidget(statusLabel_, 1);
    pageLayout->addLayout(bottomButtons);
    interactiveWidgets_.push_back(restoreInitialButton_);

    connect(pidProfileCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ChassisPage::handlePidProfileChanged);
    connect(pidReadButton_, &QPushButton::clicked, this,
            &ChassisPage::readPid);
    connect(pidWriteButton_, &QPushButton::clicked, this,
            &ChassisPage::writePid);
    connect(chassisReadButton_, &QPushButton::clicked, this,
            &ChassisPage::readChassis);
    connect(chassisWriteButton_, &QPushButton::clicked, this,
            &ChassisPage::writeChassis);
    connect(imuReadButton_, &QPushButton::clicked, this,
            &ChassisPage::readImu);
    connect(imuWriteButton_, &QPushButton::clicked, this,
            &ChassisPage::writeImu);
    connect(restoreInitialButton_, &QPushButton::clicked, this,
            &ChassisPage::restoreInitial);

    setConnected(false);
}

void ChassisPage::setConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        pidAnglePlot_->clear();
        pidOutputPlot_->clear();
        values_.clear();
        connectionInitialValues_.clear();
        connectionInitialGroups_.clear();
        pendingReadGroups_.clear();
        pendingWriteGroups_.clear();
        for (QAbstractSpinBox *control : pidControls_) {
            if (control != nullptr) {
                if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
                    doubleSpin->setValue(doubleSpin->minimum());
                } else if (auto *intSpin = qobject_cast<QSpinBox *>(control)) {
                    intSpin->setValue(intSpin->minimum());
                }
            }
        }
        for (auto it = controls_.cbegin(); it != controls_.cend(); ++it) {
            QAbstractSpinBox *control = it.value();
            if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
                doubleSpin->setValue(doubleSpin->minimum());
            } else if (auto *intSpin = qobject_cast<QSpinBox *>(control)) {
                intSpin->setValue(intSpin->minimum());
            }
        }
        pidProfileCombo_->setCurrentIndex(0);
        setStatus(QString());
    }
    for (QWidget *widget : interactiveWidgets_) {
        if (widget != nullptr) {
            widget->setEnabled(connected_);
        }
    }
    refreshRestoreButton();
}

void ChassisPage::setPidSample(const PidSample &sample) {
    pidAnglePlot_->append(sample.timestampMs,
                          {sample.targetDegrees, sample.actualDegrees});
    pidOutputPlot_->append(sample.timestampMs, {sample.output});
}

void ChassisPage::setValues(const QVector<ParameterValue> &values) {
    QSet<quint8> groupsInCall;
    for (const ParameterValue &value : values) {
        const ParameterSpec *spec = catalog_.find(value.id);
        if (spec == nullptr || !ownsGroup(spec->group) ||
            spec->type != value.type) {
            continue;
        }
        QString validationError;
        if (!catalog_.validate(value.id, value.value, &validationError)) {
            continue;
        }
        values_.insert(value.id, value);
        groupsInCall.insert(spec->group);
    }

    QSet<quint8> completedGroups;
    for (quint8 group : groupsInCall) {
        const bool isReadResponse =
            connected_ && pendingReadGroups_.contains(group) &&
            !pendingWriteGroups_.contains(group);
        if (isReadResponse && group == kPidGroup &&
            !hasCompleteGroup(group)) {
            continue;
        }
        if (isReadResponse && !connectionInitialGroups_.contains(group)) {
            for (const ParameterSpec &spec : catalog_.group(group)) {
                const auto it = values_.constFind(spec.id);
                if (it != values_.constEnd()) {
                    connectionInitialValues_.insert(spec.id, *it);
                }
            }
            connectionInitialGroups_.insert(group);
        }
        pendingWriteGroups_.remove(group);
        if (isReadResponse) {
            pendingReadGroups_.remove(group);
        }
        completedGroups.insert(group);
    }

    for (const auto &[id, control] : controls_.asKeyValueRange()) {
        const auto it = values_.constFind(id);
        if (it != values_.constEnd()) {
            applyValue(*it);
        }
    }
    const bool incompletePidRead =
        pendingReadGroups_.contains(kPidGroup) &&
        !hasCompleteGroup(kPidGroup);
    if (!incompletePidRead) {
        refreshPidControls();
    }
    refreshRestoreButton();
    if (!completedGroups.isEmpty()) {
        setStatus(QStringLiteral("已读取设备 RAM 参数"));
    }
}

void ChassisPage::clearPendingRead(quint8 group, QString reason) {
    pendingReadGroups_.remove(group);
    if (group == kPidGroup) {
        for (const ParameterSpec &spec : catalog_.group(group)) {
            values_.remove(spec.id);
        }
        refreshPidControls();
    }
    refreshRestoreButton();
    setStatus(reason.isEmpty()
                  ? QStringLiteral("参数读取失败，可重试")
                  : QStringLiteral("%1，可重试").arg(reason));
}

void ChassisPage::handlePidProfileChanged(int) {
    const int previousProfile = activePidProfile_;
    activePidProfile_ = pidProfileCombo_->currentData().toInt();
    if (connected_) {
        capturePidControls(previousProfile);
    }
    refreshPidControls();
}

void ChassisPage::readPid() {
    if (connected_) {
        if (pendingReadGroups_.contains(kPidGroup)) {
            return;
        }
        for (const ParameterSpec &spec : catalog_.group(kPidGroup)) {
            values_.remove(spec.id);
        }
        pendingWriteGroups_.remove(kPidGroup);
        pendingReadGroups_.insert(kPidGroup);
        emit readRequested(kPidGroup);
        setStatus(QStringLiteral("正在读取 PID 参数…"));
    }
}

void ChassisPage::writePid() {
    if (!connected_) {
        return;
    }
    if (pendingReadGroups_.contains(kPidGroup) &&
        !hasCompleteGroup(kPidGroup)) {
        setStatus(QStringLiteral("正在等待完整 PID 参数…"));
        return;
    }
    const QVector<ParameterValue> values = collectGroup(kPidGroup);
    if (!values.isEmpty()) {
        pendingWriteGroups_.insert(kPidGroup);
        emit writeRequested(kPidGroup, values);
        setStatus(QStringLiteral("已请求写入 PID RAM，等待设备回读"));
    }
}

void ChassisPage::readChassis() {
    if (connected_) {
        pendingWriteGroups_.remove(kChassisGroup);
        pendingReadGroups_.insert(kChassisGroup);
        emit readRequested(kChassisGroup);
        setStatus(QStringLiteral("正在读取底盘参数…"));
    }
}

void ChassisPage::writeChassis() {
    if (!connected_) {
        return;
    }
    const QVector<ParameterValue> values = collectGroup(kChassisGroup);
    if (!values.isEmpty()) {
        pendingWriteGroups_.insert(kChassisGroup);
        emit writeRequested(kChassisGroup, values);
        setStatus(QStringLiteral("已请求写入底盘 RAM，等待设备回读"));
    }
}

void ChassisPage::readImu() {
    if (connected_) {
        pendingWriteGroups_.remove(kImuGroup);
        pendingReadGroups_.insert(kImuGroup);
        emit readRequested(kImuGroup);
        setStatus(QStringLiteral("正在读取 IMU 参数…"));
    }
}

void ChassisPage::writeImu() {
    if (!connected_) {
        return;
    }
    const QVector<ParameterValue> values = collectGroup(kImuGroup);
    if (!values.isEmpty()) {
        pendingWriteGroups_.insert(kImuGroup);
        emit writeRequested(kImuGroup, values);
        setStatus(QStringLiteral("已请求写入 IMU RAM，等待设备回读"));
    }
}

void ChassisPage::restoreInitial() {
    if (!connected_ || connectionInitialValues_.isEmpty()) {
        return;
    }
    for (const auto &[id, value] : connectionInitialValues_.asKeyValueRange()) {
        if (const ParameterSpec *spec = catalog_.find(id);
            spec != nullptr && ownsGroup(spec->group)) {
            values_.insert(id, value);
        }
    }
    for (const auto &[id, control] : controls_.asKeyValueRange()) {
        const auto it = values_.constFind(id);
        if (it != values_.constEnd()) {
            applyValue(*it);
        }
    }
    refreshPidControls();
    setStatus(QStringLiteral("已恢复本次连接初值；请点击“写入 RAM”后才会发送"));
}

bool ChassisPage::ownsGroup(quint8 group) const {
    return group == kPidGroup || group == kChassisGroup || group == kImuGroup;
}

bool ChassisPage::hasCompleteGroup(quint8 group) const {
    const QVector<ParameterSpec> specs = catalog_.group(group);
    if (specs.isEmpty()) {
        return false;
    }
    for (const ParameterSpec &spec : specs) {
        if (!values_.contains(spec.id)) {
            return false;
        }
    }
    return true;
}

quint16 ChassisPage::activePidId(int fieldOffset) const {
    const int profile = pidProfileCombo_ == nullptr
                            ? 0
                            : pidProfileCombo_->currentData().toInt();
    return static_cast<quint16>(0x1000 + profile * 0x10 + fieldOffset);
}

QAbstractSpinBox *ChassisPage::createSpinBox(const ParameterSpec &spec,
                                             const QString &objectName,
                                             QWidget *parent) {
    QAbstractSpinBox *control = nullptr;
    if (spec.type == ValueType::Float32) {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setDecimals(4);
        spin->setRange(spec.minimum, spec.maximum);
        spin->setSingleStep(0.01);
        control = spin;
    } else {
        auto *spin = new QSpinBox(parent);
        spin->setRange(static_cast<int>(std::ceil(spec.minimum)),
                       static_cast<int>(std::floor(spec.maximum)));
        spin->setSingleStep(1);
        control = spin;
    }
    control->setObjectName(objectName);
    return control;
}

void ChassisPage::addFieldRow(QFormLayout *layout, const ParameterSpec &spec,
                              QAbstractSpinBox *control) {
    if (layout == nullptr || control == nullptr) {
        return;
    }
    layout->addRow(spec.label, fieldWithUnit(control, spec.unit, this));
}

void ChassisPage::capturePidControls(int profile) {
    if (pidControls_.size() != pidSpecs_.size()) {
        return;
    }
    if (profile < 0) {
        profile = pidProfileCombo_ == nullptr
                      ? 0
                      : pidProfileCombo_->currentData().toInt();
    }
    for (int offset = 0; offset < pidControls_.size(); ++offset) {
        const ParameterSpec *profileZeroSpec = pidSpecs_.at(offset);
        if (profileZeroSpec == nullptr) {
            continue;
        }
        const quint16 id = static_cast<quint16>(0x1000 + profile * 0x10 + offset);
        const ParameterSpec *spec = catalog_.find(id);
        if (spec != nullptr) {
            values_.insert(id, ParameterValue{id, spec->type,
                                               controlValue(pidControls_.at(offset))});
        }
    }
}

QVariant ChassisPage::controlValue(QAbstractSpinBox *control) const {
    if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
        return QVariant(doubleSpin->value());
    }
    if (auto *intSpin = qobject_cast<QSpinBox *>(control)) {
        return QVariant(intSpin->value());
    }
    return {};
}

void ChassisPage::applyValue(const ParameterValue &value) {
    const ParameterSpec *spec = catalog_.find(value.id);
    if (spec == nullptr || spec->type != value.type) {
        return;
    }
    if (QAbstractSpinBox *control = controls_.value(value.id, nullptr)) {
        if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
            doubleSpin->setValue(value.value.toDouble());
        } else if (auto *intSpin = qobject_cast<QSpinBox *>(control)) {
            intSpin->setValue(value.value.toInt());
        }
    }
}

void ChassisPage::refreshPidControls() {
    for (int offset = 0; offset < pidControls_.size(); ++offset) {
        const quint16 id = activePidId(offset);
        QAbstractSpinBox *control = pidControls_.at(offset);
        const ParameterSpec *spec = catalog_.find(id);
        if (control == nullptr || spec == nullptr) {
            continue;
        }
        const auto it = values_.constFind(id);
        if (it != values_.constEnd()) {
            if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
                doubleSpin->setValue(it->value.toDouble());
            }
        } else if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
            doubleSpin->setValue(spec->minimum);
        }
    }
}

void ChassisPage::refreshRestoreButton() {
    if (restoreInitialButton_ != nullptr) {
        restoreInitialButton_->setEnabled(
            connected_ && !connectionInitialValues_.isEmpty());
    }
}

QVector<ParameterValue> ChassisPage::collectGroup(quint8 group) {
    QVector<ParameterValue> result;
    if (!ownsGroup(group)) {
        return result;
    }
    if (group == kPidGroup) {
        capturePidControls();
        result.reserve(pidControls_.size());
        for (int offset = 0; offset < pidControls_.size(); ++offset) {
            const quint16 id = activePidId(offset);
            const ParameterSpec *spec = catalog_.find(id);
            if (spec == nullptr) {
                result.clear();
                return result;
            }
            const QVariant value = controlValue(pidControls_.at(offset));
            QString error;
            if (!catalog_.validate(id, value, &error)) {
                setStatus(error);
                result.clear();
                return result;
            }
            result.push_back(ParameterValue{id, spec->type, value});
        }
        return result;
    }

    const QVector<ParameterSpec> specs = catalog_.group(group);
    result.reserve(specs.size());
    for (const ParameterSpec &spec : specs) {
        QAbstractSpinBox *control = controls_.value(spec.id, nullptr);
        if (control == nullptr) {
            result.clear();
            return result;
        }
        const QVariant value = controlValue(control);
        QString error;
        if (!catalog_.validate(spec.id, value, &error)) {
            setStatus(error);
            result.clear();
            return result;
        }
        values_.insert(spec.id, ParameterValue{spec.id, spec.type, value});
        result.push_back(ParameterValue{spec.id, spec.type, value});
    }
    return result;
}

void ChassisPage::setStatus(const QString &message) {
    if (statusLabel_ != nullptr) {
        statusLabel_->setText(message);
    }
}
