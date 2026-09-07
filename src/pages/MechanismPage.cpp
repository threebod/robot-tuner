#include "pages/MechanismPage.h"

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

MechanismPage::MechanismPage(QWidget *parent)
    : QWidget(parent), catalog_(ParameterCatalog::instance()) {
    auto *pageLayout = new QVBoxLayout(this);

    auto *ramOnlyNotice = new QLabel(
        QStringLiteral("机构参数仅写入 STM32 RAM；不会写入 Flash，设备重启后恢复固件默认值。"),
        this);
    ramOnlyNotice->setObjectName(QStringLiteral("mechanismRamOnlyNotice"));
    ramOnlyNotice->setWordWrap(true);
    pageLayout->addWidget(ramOnlyNotice);

    auto *mechanismGroup =
        new QGroupBox(QStringLiteral("机构参数（组 0x30）"), this);
    auto *mechanismLayout = new QVBoxLayout(mechanismGroup);
    auto *fields = new QFormLayout;
    const QStringList objectNames = {
        QStringLiteral("horizontalPositionSpinBox"),
        QStringLiteral("liftPositionSpinBox"),
        QStringLiteral("turretAngleSpinBox"),
        QStringLiteral("motorSpeedSpinBox"),
        QStringLiteral("mechanismAccelerationSpinBox"),
        QStringLiteral("interpolationSpeedSpinBox"),
    };
    const QVector<ParameterSpec> specs = catalog_.group(kMechanismGroup);
    for (int index = 0; index < specs.size(); ++index) {
        const ParameterSpec &spec = specs.at(index);
        auto *control = createSpinBox(spec, objectNames.at(index),
                                      mechanismGroup);
        controls_.insert(spec.id, control);
        interactiveWidgets_.push_back(control);
        addFieldRow(fields, spec, control);
    }
    mechanismLayout->addLayout(fields);

    auto *buttons = new QHBoxLayout;
    readButton_ = makeButton(QStringLiteral("读取机构参数"),
                             QStringLiteral("mechanismReadButton"),
                             mechanismGroup);
    writeButton_ = makeButton(QStringLiteral("写入 RAM"),
                              QStringLiteral("mechanismWriteButton"),
                              mechanismGroup);
    buttons->addWidget(readButton_);
    buttons->addWidget(writeButton_);
    buttons->addStretch();
    mechanismLayout->addLayout(buttons);
    interactiveWidgets_.push_back(readButton_);
    interactiveWidgets_.push_back(writeButton_);
    pageLayout->addWidget(mechanismGroup);

    auto *bottomButtons = new QHBoxLayout;
    restoreInitialButton_ = makeButton(
        QStringLiteral("恢复本次连接初值"),
        QStringLiteral("mechanismRestoreInitialButton"), this);
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("mechanismParameterStatusLabel"));
    bottomButtons->addWidget(restoreInitialButton_);
    bottomButtons->addWidget(statusLabel_, 1);
    pageLayout->addLayout(bottomButtons);
    interactiveWidgets_.push_back(restoreInitialButton_);

    connect(readButton_, &QPushButton::clicked, this,
            &MechanismPage::readMechanism);
    connect(writeButton_, &QPushButton::clicked, this,
            &MechanismPage::writeMechanism);
    connect(restoreInitialButton_, &QPushButton::clicked, this,
            &MechanismPage::restoreInitial);

    setConnected(false);
}

void MechanismPage::setConnected(bool connected) {
    connected_ = connected;
    if (!connected_) {
        values_.clear();
        connectionInitialValues_.clear();
        connectionInitialGroups_.clear();
        pendingReadGroups_.clear();
        pendingWriteGroups_.clear();
        for (const auto &[id, control] : controls_.asKeyValueRange()) {
            Q_UNUSED(id);
            if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
                doubleSpin->setValue(doubleSpin->minimum());
            } else if (auto *intSpin = qobject_cast<QSpinBox *>(control)) {
                intSpin->setValue(intSpin->minimum());
            }
        }
        setStatus(QString());
    }
    for (QWidget *widget : interactiveWidgets_) {
        if (widget != nullptr) {
            widget->setEnabled(connected_);
        }
    }
    refreshRestoreButton();
}

void MechanismPage::setValues(const QVector<ParameterValue> &values) {
    bool receivedMechanismValues = false;
    for (const ParameterValue &value : values) {
        const ParameterSpec *spec = catalog_.find(value.id);
        if (spec == nullptr || spec->group != kMechanismGroup ||
            spec->type != value.type) {
            continue;
        }
        QString validationError;
        if (!catalog_.validate(value.id, value.value, &validationError)) {
            continue;
        }
        values_.insert(value.id, value);
        receivedMechanismValues = true;
    }
    const bool isWriteResponse = pendingWriteGroups_.contains(kMechanismGroup);
    if (receivedMechanismValues && !isWriteResponse &&
        !connectionInitialGroups_.contains(kMechanismGroup)) {
        for (const ParameterValue &value : values) {
            const ParameterSpec *spec = catalog_.find(value.id);
            if (spec != nullptr && spec->group == kMechanismGroup &&
                spec->type == value.type) {
                connectionInitialValues_.insert(value.id, value);
            }
        }
        connectionInitialGroups_.insert(kMechanismGroup);
    }
    if (receivedMechanismValues) {
        pendingReadGroups_.remove(kMechanismGroup);
        pendingWriteGroups_.remove(kMechanismGroup);
    }

    for (const auto &[id, control] : controls_.asKeyValueRange()) {
        const auto it = values_.constFind(id);
        if (it != values_.constEnd()) {
            applyValue(*it);
        }
        Q_UNUSED(control);
    }
    refreshRestoreButton();
    if (receivedMechanismValues) {
        setStatus(QStringLiteral("已读取设备 RAM 参数"));
    }
}

void MechanismPage::readMechanism() {
    if (connected_) {
        pendingWriteGroups_.remove(kMechanismGroup);
        pendingReadGroups_.insert(kMechanismGroup);
        emit readRequested(kMechanismGroup);
        setStatus(QStringLiteral("正在读取机构参数…"));
    }
}

void MechanismPage::writeMechanism() {
    if (!connected_) {
        return;
    }
    const QVector<ParameterValue> values = collectValues();
    if (!values.isEmpty()) {
        pendingWriteGroups_.insert(kMechanismGroup);
        emit writeRequested(kMechanismGroup, values);
        setStatus(QStringLiteral("已请求写入机构 RAM，等待设备回读"));
    }
}

void MechanismPage::restoreInitial() {
    if (!connected_ || connectionInitialValues_.isEmpty()) {
        return;
    }
    for (const auto &[id, value] : connectionInitialValues_.asKeyValueRange()) {
        values_.insert(id, value);
        applyValue(value);
    }
    setStatus(QStringLiteral("已恢复本次连接初值；请点击“写入 RAM”后才会发送"));
}

QAbstractSpinBox *MechanismPage::createSpinBox(const ParameterSpec &spec,
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

void MechanismPage::addFieldRow(QFormLayout *layout, const ParameterSpec &spec,
                                QAbstractSpinBox *control) {
    if (layout == nullptr || control == nullptr) {
        return;
    }
    layout->addRow(spec.label, fieldWithUnit(control, spec.unit, this));
}

QVariant MechanismPage::controlValue(QAbstractSpinBox *control) const {
    if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
        return QVariant(doubleSpin->value());
    }
    if (auto *intSpin = qobject_cast<QSpinBox *>(control)) {
        return QVariant(intSpin->value());
    }
    return {};
}

void MechanismPage::applyValue(const ParameterValue &value) {
    QAbstractSpinBox *control = controls_.value(value.id, nullptr);
    if (control == nullptr) {
        return;
    }
    if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(control)) {
        doubleSpin->setValue(value.value.toDouble());
    } else if (auto *intSpin = qobject_cast<QSpinBox *>(control)) {
        intSpin->setValue(value.value.toInt());
    }
}

QVector<ParameterValue> MechanismPage::collectValues() {
    QVector<ParameterValue> result;
    const QVector<ParameterSpec> specs = catalog_.group(kMechanismGroup);
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

void MechanismPage::refreshRestoreButton() {
    if (restoreInitialButton_ != nullptr) {
        restoreInitialButton_->setEnabled(
            connected_ && !connectionInitialValues_.isEmpty());
    }
}

void MechanismPage::setStatus(const QString &message) {
    if (statusLabel_ != nullptr) {
        statusLabel_->setText(message);
    }
}
