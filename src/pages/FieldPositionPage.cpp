#include "pages/FieldPositionPage.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kFieldSizeMm = 2400.0;

void drawFieldLabel(QPainter *painter, const QRectF &fieldRect,
                    QPointF fieldPoint, const QString &text) {
    const QPointF center =
        FieldMapWidget::fieldToScreen(fieldPoint, fieldRect);
    const QRectF labelRect(center.x() - 55, center.y() - 11, 110, 22);
    painter->drawText(labelRect, Qt::AlignCenter, text);
}

}  // namespace

FieldMapWidget::FieldMapWidget(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("fieldMapWidget"));
    setMinimumSize(420, 420);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QPointF FieldMapWidget::fieldToScreen(QPointF fieldPoint,
                                      QRectF screenRect) {
    return {screenRect.left() + fieldPoint.x() / kFieldSizeMm * screenRect.width(),
            screenRect.bottom() -
                fieldPoint.y() / kFieldSizeMm * screenRect.height()};
}

QPointF FieldMapWidget::headingVector(double yawDegrees) {
    const double radians = yawDegrees * 3.14159265358979323846 / 180.0;
    return {std::cos(radians), -std::sin(radians)};
}

QPointF FieldMapWidget::displayedFieldPosition() const {
    return {std::clamp<double>(pose_.xMm, 0.0, kFieldSizeMm),
            std::clamp<double>(pose_.yMm, 0.0, kFieldSizeMm)};
}

void FieldMapWidget::setPoseSample(PoseSample sample) {
    pose_ = sample;
    update();
}

QRectF FieldMapWidget::fieldRect() const {
    constexpr qreal margin = 42.0;
    const qreal side = std::max<qreal>(1.0, std::min(width(), height()) -
                                               2.0 * margin);
    return {0.5 * (width() - side), 0.5 * (height() - side), side, side};
}

void FieldMapWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QRectF rect = fieldRect();
    if (!rect.contains(event->position())) {
        return;
    }
    const qreal x = (event->position().x() - rect.left()) / rect.width() *
                    kFieldSizeMm;
    const qreal y = (rect.bottom() - event->position().y()) / rect.height() *
                    kFieldSizeMm;
    emit fieldPointSelected({x, y});
}

void FieldMapWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().window());

    const QRectF area = fieldRect();
    const auto fieldBox = [&area](double x, double y, double width,
                                  double height) {
        const QPointF topLeft = FieldMapWidget::fieldToScreen(
            {x, y + height}, area);
        const QPointF bottomRight = FieldMapWidget::fieldToScreen(
            {x + width, y}, area);
        return QRectF(topLeft, bottomRight);
    };

    painter.setPen(QPen(QColor(80, 84, 90), 2));
    painter.setBrush(QColor(248, 247, 239));
    painter.drawRect(area);

    painter.fillRect(fieldBox(1000, 0, 400, 2400), QColor(205, 207, 210));
    painter.fillRect(fieldBox(0, 1000, 2400, 400), QColor(205, 207, 210));

    painter.setBrush(QColor(255, 248, 196));
    painter.setPen(QPen(QColor(214, 199, 119), 1));
    for (const QPointF &origin : {QPointF(550, 550), QPointF(1400, 550),
                                  QPointF(550, 1400), QPointF(1400, 1400)}) {
        painter.drawRect(fieldBox(origin.x(), origin.y(), 450, 450));
    }

    painter.fillRect(fieldBox(2100, 2100, 300, 300), QColor(48, 103, 224));
    painter.fillRect(fieldBox(2100, 0, 300, 300), QColor(48, 103, 224));
    painter.setPen(QColor(36, 42, 48));
    drawFieldLabel(&painter, area, {2250, 2250}, QStringLiteral("启停区 1"));
    drawFieldLabel(&painter, area, {2250, 150}, QStringLiteral("启停区 2"));
    drawFieldLabel(&painter, area, {1200, 2320}, QStringLiteral("原料区"));
    drawFieldLabel(&painter, area, {1200, 80}, QStringLiteral("粗加工区"));
    drawFieldLabel(&painter, area, {120, 1200}, QStringLiteral("暂存区"));
    drawFieldLabel(&painter, area, {2280, 1200}, QStringLiteral("二维码区"));

    painter.setPen(QPen(QColor(40, 45, 50), 1));
    painter.drawText(QRectF(area.left(), area.bottom() + 8, area.width(), 24),
                     Qt::AlignCenter, QStringLiteral("X →  2400 mm"));
    painter.save();
    painter.translate(area.left() - 34, area.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-area.height() / 2, 0, area.height(), 24),
                     Qt::AlignCenter, QStringLiteral("Y →  2400 mm"));
    painter.restore();

    const QPointF center = fieldToScreen(displayedFieldPosition(), area);
    const QPointF direction = headingVector(pose_.yawDegrees);
    QPolygonF robot;
    robot << center + direction * 18.0
          << center + QPointF(-direction.y(), direction.x()) * 10.0 -
                 direction * 10.0
          << center + QPointF(direction.y(), -direction.x()) * 10.0 -
                 direction * 10.0;
    painter.setPen(QPen(QColor(153, 27, 27), 2));
    painter.setBrush(QColor(239, 68, 68));
    painter.drawPolygon(robot);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 13, 13);
}

FieldPositionPage::FieldPositionPage(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    map_ = new FieldMapWidget(this);
    layout->addWidget(map_, 1);

    auto *panel = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("场地位置同步"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    panel->addWidget(title);
    auto *notice = new QLabel(
        QStringLiteral("调试显示用途；现场布置可能偏离名义尺寸，不用于导航控制。"),
        this);
    notice->setWordWrap(true);
    panel->addWidget(notice);

    simulationCheckBox_ = new QCheckBox(QStringLiteral("本地模拟模式"), this);
    simulationCheckBox_->setObjectName(QStringLiteral("localSimulationCheckBox"));
    simulationCheckBox_->setChecked(true);
    panel->addWidget(simulationCheckBox_);

    auto *presetRow = new QHBoxLayout;
    auto *preset1 = new QPushButton(QStringLiteral("启停区 1"), this);
    preset1->setObjectName(QStringLiteral("startZone1Button"));
    auto *preset2 = new QPushButton(QStringLiteral("启停区 2"), this);
    preset2->setObjectName(QStringLiteral("startZone2Button"));
    presetRow->addWidget(preset1);
    presetRow->addWidget(preset2);
    panel->addLayout(presetRow);

    auto *form = new QFormLayout;
    xSpinBox_ = new QDoubleSpinBox(this);
    xSpinBox_->setObjectName(QStringLiteral("poseXSpinBox"));
    ySpinBox_ = new QDoubleSpinBox(this);
    ySpinBox_->setObjectName(QStringLiteral("poseYSpinBox"));
    yawSpinBox_ = new QDoubleSpinBox(this);
    yawSpinBox_->setObjectName(QStringLiteral("poseYawSpinBox"));
    for (QDoubleSpinBox *coordinate : {xSpinBox_, ySpinBox_}) {
        coordinate->setRange(-2147483648.0, 2147483647.0);
        coordinate->setDecimals(1);
        coordinate->setSuffix(QStringLiteral(" mm"));
    }
    yawSpinBox_->setRange(-327.68, 327.67);
    yawSpinBox_->setDecimals(2);
    yawSpinBox_->setSuffix(QStringLiteral(" °"));
    form->addRow(QStringLiteral("X（向右）"), xSpinBox_);
    form->addRow(QStringLiteral("Y（向上）"), ySpinBox_);
    form->addRow(QStringLiteral("航向（逆时针为正）"), yawSpinBox_);
    panel->addLayout(form);

    auto *apply = new QPushButton(QStringLiteral("应用位姿"), this);
    apply->setObjectName(QStringLiteral("poseApplyButton"));
    panel->addWidget(apply);

    valueLabel_ = new QLabel(QStringLiteral("x=0.0 mm  y=0.0 mm  yaw=0.00°"), this);
    valueLabel_->setObjectName(QStringLiteral("poseValueLabel"));
    sourceLabel_ = new QLabel(QStringLiteral("数据来源：尚无数据"), this);
    sourceLabel_->setObjectName(QStringLiteral("poseSourceLabel"));
    updatedLabel_ = new QLabel(QStringLiteral("更新时间：—"), this);
    updatedLabel_->setObjectName(QStringLiteral("poseUpdatedLabel"));
    statusLabel_ = new QLabel(QStringLiteral("状态：数据超时"), this);
    statusLabel_->setObjectName(QStringLiteral("poseStatusLabel"));
    capabilityLabel_ = new QLabel(QStringLiteral("设备位姿能力：未声明"), this);
    capabilityLabel_->setObjectName(QStringLiteral("poseCapabilityLabel"));
    panel->addWidget(valueLabel_);
    panel->addWidget(sourceLabel_);
    panel->addWidget(updatedLabel_);
    panel->addWidget(statusLabel_);
    panel->addWidget(capabilityLabel_);
    panel->addStretch();
    layout->addLayout(panel);

    staleTimer_ = new QTimer(this);
    staleTimer_->setInterval(50);
    connect(staleTimer_, &QTimer::timeout, this, &FieldPositionPage::refreshStatus);
    staleTimer_->start();
    connect(preset1, &QPushButton::clicked, this,
            [this] { applyPreset(2250, 2250); });
    connect(preset2, &QPushButton::clicked, this,
            [this] { applyPreset(2250, 150); });
    connect(apply, &QPushButton::clicked, this,
            &FieldPositionPage::applyInputPose);
    connect(map_, &FieldMapWidget::fieldPointSelected, this,
            &FieldPositionPage::selectFieldPoint);
}

void FieldPositionPage::setPoseSample(PoseSample sample,
                                      const QString &source) {
    pose_ = sample;
    outOfBounds_ = sample.xMm < 0 || sample.xMm > kFieldSizeMm ||
                   sample.yMm < 0 || sample.yMm > kFieldSizeMm;
    map_->setPoseSample(sample);
    xSpinBox_->setValue(sample.xMm);
    ySpinBox_->setValue(sample.yMm);
    yawSpinBox_->setValue(sample.yawDegrees);
    valueLabel_->setText(QStringLiteral("x=%1 mm  y=%2 mm  yaw=%3°")
                             .arg(sample.xMm)
                             .arg(sample.yMm)
                             .arg(sample.yawDegrees, 0, 'f', 2));
    sourceLabel_->setText(QStringLiteral("数据来源：%1").arg(source));
    updatedLabel_->setText(
        QStringLiteral("更新时间：%1（样本时间 %2 ms）")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")))
            .arg(sample.timestampMs));
    lastUpdate_.restart();
    refreshStatus();
}

void FieldPositionPage::setPoseCapabilityAvailable(bool available) {
    capabilityLabel_->setText(available ? QStringLiteral("设备位姿能力：已声明")
                                        : QStringLiteral("设备位姿能力：未声明"));
}

void FieldPositionPage::selectFieldPoint(QPointF fieldPoint) {
    xSpinBox_->setValue(fieldPoint.x());
    ySpinBox_->setValue(fieldPoint.y());
    if (simulationCheckBox_->isChecked()) {
        applyInputPose();
    }
}

void FieldPositionPage::applyInputPose() {
    PoseSample sample;
    sample.timestampMs = static_cast<quint32>(
        QDateTime::currentMSecsSinceEpoch() & 0xffffffff);
    sample.xMm = static_cast<qint32>(std::lround(xSpinBox_->value()));
    sample.yMm = static_cast<qint32>(std::lround(ySpinBox_->value()));
    sample.yawDegrees = yawSpinBox_->value();
    if (simulationCheckBox_->isChecked()) {
        setPoseSample(sample, QStringLiteral("本地模拟"));
    } else {
        emit setPoseRequested(sample);
    }
}

void FieldPositionPage::applyPreset(qint32 xMm, qint32 yMm) {
    xSpinBox_->setValue(xMm);
    ySpinBox_->setValue(yMm);
    yawSpinBox_->setValue(180.0);
    applyInputPose();
}

void FieldPositionPage::refreshStatus() {
    if (!lastUpdate_.isValid() || lastUpdate_.elapsed() > 500) {
        statusLabel_->setText(outOfBounds_
                                  ? QStringLiteral("状态：数据超时；越界（图标已限制在边界，数值保留原值）")
                                  : QStringLiteral("状态：数据超时"));
        statusLabel_->setStyleSheet(QStringLiteral("color: #b91c1c; font-weight: bold;"));
        return;
    }
    if (outOfBounds_) {
        statusLabel_->setText(QStringLiteral("状态：越界（图标已限制在边界，数值保留原值）"));
        statusLabel_->setStyleSheet(QStringLiteral("color: #b91c1c; font-weight: bold;"));
        return;
    }
    statusLabel_->setText(QStringLiteral("状态：正常"));
    statusLabel_->setStyleSheet({});
}
