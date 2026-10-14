#include "pages/RouteEditorPage.h"
#include "device/RouteSequenceRunner.h"
#include "../../../test/mecanum_jog/vision_defaults.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QSpinBox *spin(QWidget *parent, const char *name, int low, int high, int value, const QString &suffix = {}) {
    auto *result = new QSpinBox(parent);
    result->setObjectName(name); result->setRange(low, high); result->setValue(value); result->setSuffix(suffix);
    return result;
}
QPushButton *button(QWidget *parent, const QString &text, const char *name) {
    auto *result = new QPushButton(text, parent); result->setObjectName(name); return result;
}
}

RouteEditorPage::RouteEditorPage(RouteSequenceRunner *runner, QWidget *parent)
    : QWidget(parent), runner_(runner) {
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(QStringLiteral("方向相对当前车头；视觉对准后直接从当前位置继续，不回退。运行须保持串口连接。"), this);
    notice->setWordWrap(true); layout->addWidget(notice);
    editControls_ = new QWidget(this);
    auto *edit = new QVBoxLayout(editControls_);
    edit->setContentsMargins(0, 0, 0, 0);
    auto *files = new QHBoxLayout;
    files->addWidget(new QLabel(QStringLiteral("路线名称"), this));
    name_ = new QLineEdit(sequence_.name, this); name_->setObjectName("routeName"); files->addWidget(name_, 1);
    auto *open = button(this, QStringLiteral("打开 JSON"), "routeOpen");
    auto *save = button(this, QStringLiteral("保存 JSON"), "routeSave");
    files->addWidget(open); files->addWidget(save); edit->addLayout(files);
    table_ = new QTableWidget(0, 2, this); table_->setObjectName("routeSteps");
    table_->setHorizontalHeaderLabels({QStringLiteral("步骤"), QStringLiteral("动作与参数")});
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setMinimumHeight(170); edit->addWidget(table_, 1);
    auto *order = new QHBoxLayout;
    auto *add = button(this, QStringLiteral("添加步骤"), "routeAdd");
    auto *replace = button(this, QStringLiteral("修改选中"), "routeReplace");
    auto *copy = button(this, QStringLiteral("复制"), "routeCopy");
    auto *remove = button(this, QStringLiteral("删除"), "routeDelete");
    auto *up = button(this, QStringLiteral("上移"), "routeUp");
    auto *down = button(this, QStringLiteral("下移"), "routeDown");
    for (auto *b : {add, replace, copy, remove, up, down}) order->addWidget(b);
    order->addStretch(); edit->addLayout(order);
    type_ = new QComboBox(this); type_->setObjectName("routeType");
    type_->addItems({QStringLiteral("相对移动"), QStringLiteral("转向"), QStringLiteral("视觉对准"),
        QStringLiteral("动作组"), QStringLiteral("等待"), QStringLiteral("人工确认")});
    auto *typeRow = new QHBoxLayout; typeRow->addWidget(new QLabel(QStringLiteral("步骤类型"), this));
    typeRow->addWidget(type_); typeRow->addStretch(); edit->addLayout(typeRow);
    parameters_ = new QStackedWidget(this);
    auto panel = [this] {
        auto *page = new QWidget(parameters_); parameters_->addWidget(page); return new QFormLayout(page);
    };
    auto *move = panel();
    moveDirection_ = new QComboBox(this); moveDirection_->setObjectName("routeMoveDirection");
    moveDirection_->addItems({QStringLiteral("前进"), QStringLiteral("后退"), QStringLiteral("左移"), QStringLiteral("右移")});
    distance_ = spin(this, "routeDistance", 1, 2400, 500, " mm");
    rpm_ = spin(this, "routeRpm", 10, 250, 100, " RPM");
    move->addRow(QStringLiteral("方向"), moveDirection_); move->addRow(QStringLiteral("距离"), distance_); move->addRow(QStringLiteral("速度"), rpm_);
    auto *turn = panel();
    turnDirection_ = new QComboBox(this); turnDirection_->setObjectName("routeTurnDirection");
    turnDirection_->addItems({QStringLiteral("左转"), QStringLiteral("右转")});
    degrees_ = spin(this, "routeDegrees", 1, 180, 90, "°");
    turn->addRow(QStringLiteral("方向"), turnDirection_); turn->addRow(QStringLiteral("角度"), degrees_);
    auto *vision = panel();
    visionMode_ = new QComboBox(this); visionMode_->setObjectName("routeVisionMode");
    visionMode_->addItems({QStringLiteral("物料颜色"), QStringLiteral("圆环")});
    target_ = spin(this, "routeVisionTarget", 1, 6, 1);
    forward_ = spin(this, "routeForwardScale", 50, 2000, 640);
    right_ = spin(this, "routeRightScale", 50, 2000, 673);
    vision->addRow(QStringLiteral("识别类型"), visionMode_); vision->addRow(QStringLiteral("目标编号"), target_);
    vision->addRow(QStringLiteral("前后比例（mm/pixel ×1000）"), forward_); vision->addRow(QStringLiteral("左右比例（mm/pixel ×1000）"), right_);
    auto *colors = new QLabel(QStringLiteral("颜色：1红 2黄 3蓝 4绿 5黑 6浅蓝。仅对准，抓取须另加动作组。"), this);
    colors->setWordWrap(true); vision->addRow(colors);
    auto *action = panel();
    actionLabel_ = new QLabel(QStringLiteral("尚未导入动作组"), this); actionLabel_->setWordWrap(true);
    auto *import = button(this, QStringLiteral("导入动作组 JSON"), "routeImportAction");
    action->addRow(actionLabel_); action->addRow(import);
    auto *actionNotice = new QLabel(QStringLiteral("执行前须在动作录入页准备并初始化机构；动作组内容随路线保存。"), this);
    actionNotice->setWordWrap(true); action->addRow(actionNotice);
    auto *wait = panel(); waitMs_ = spin(this, "routeWaitMs", 0, 60000, 1000, " ms"); wait->addRow(QStringLiteral("等待时间"), waitMs_);
    auto *confirm = panel(); prompt_ = new QLineEdit(QStringLiteral("检查车辆和机构后继续"), this);
    prompt_->setObjectName("routePrompt"); confirm->addRow(QStringLiteral("确认提示"), prompt_);
    edit->addWidget(parameters_); layout->addWidget(editControls_, 1);
    auto *execution = new QHBoxLayout;
    selected_ = button(this, QStringLiteral("测试选中步骤"), "routeRunSelected");
    from_ = button(this, QStringLiteral("从选中步骤开始"), "routeRunFrom");
    all_ = button(this, QStringLiteral("整段运行"), "routeRunAll");
    confirm_ = button(this, QStringLiteral("确认并继续"), "routeConfirm");
    stop_ = button(this, QStringLiteral("停止路线"), "routeStop");
    for (auto *b : {selected_, from_, all_, confirm_, stop_}) execution->addWidget(b);
    execution->addStretch(); layout->addLayout(execution);
    status_ = new QLabel(QStringLiteral("离线编辑；连接文本串口后可测试"), this);
    status_->setObjectName("routeStatus"); status_->setWordWrap(true); layout->addWidget(status_);

    connect(type_, qOverload<int>(&QComboBox::currentIndexChanged), parameters_, &QStackedWidget::setCurrentIndex);
    auto resetVisionScale = [this] {
        const bool ring2 = visionMode_->currentIndex() == 1 && target_->value() == 2;
        forward_->setValue(ring2 ? VISION_RING2_FORWARD_MILLI : VISION_MATERIAL_FORWARD_MILLI);
        right_->setValue(ring2 ? VISION_RING2_RIGHT_MILLI : VISION_MATERIAL_RIGHT_MILLI);
    };
    connect(visionMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, resetVisionScale](int mode) {
        target_->setMaximum(mode ? 3 : 6);
        resetVisionScale();
    });
    connect(target_, qOverload<int>(&QSpinBox::valueChanged), this, [this, resetVisionScale](int) {
        if (visionMode_->currentIndex() == 1) resetVisionScale();
    });
    connect(table_, &QTableWidget::currentCellChanged, this, [this] { loadSelected(); updateControls(); });
    connect(add, &QPushButton::clicked, this, [this] {
        auto candidate = sequence(); candidate.steps.append(editedStep()); QString error;
        if (!validateRouteSequence(candidate, &error)) { status_->setText(error); return; }
        sequence_ = candidate; refresh(sequence_.steps.size() - 1);
    });
    connect(replace, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow(); if (row < 0) return;
        auto candidate = sequence(); candidate.steps[row] = editedStep(); QString error;
        if (!validateRouteSequence(candidate, &error)) { status_->setText(error); return; }
        sequence_ = candidate; refresh(row);
    });
    connect(copy, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow(); if (row < 0) return;
        sequence_.steps.insert(row + 1, sequence_.steps[row]); refresh(row + 1);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow(); if (row < 0) return;
        sequence_.steps.removeAt(row); refresh(qMin(row, int(sequence_.steps.size()) - 1));
    });
    for (auto pair : {qMakePair(up, -1), qMakePair(down, 1)}) {
        connect(pair.first, &QPushButton::clicked, this, [this, delta = pair.second] {
            const int row = table_->currentRow(); const int next = row + delta;
            if (row < 0 || next < 0 || next >= sequence_.steps.size()) return;
            sequence_.steps.swapItemsAt(row, next); refresh(next);
        });
    }
    connect(import, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入动作组"), {}, "JSON (*.json)");
        if (path.isEmpty()) return;
        QString error; MechanismSequence action;
        if (!loadMechanismSequence(path, &action, &error)) { status_->setText(error); return; }
        importedAction_ = action;
        RouteStep step; step.type = RouteStepType::Action; step.action = action;
        actionLabel_->setText(routeStepDescription(step));
    });
    connect(open, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开路线"), {}, "JSON (*.json)");
        if (path.isEmpty()) return; QString error;
        if (!loadFile(path, &error)) status_->setText(error); else status_->setText(QStringLiteral("路线已打开"));
    });
    connect(save, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存路线"), {}, "JSON (*.json)");
        if (path.isEmpty()) return; QString error;
        if (!saveFile(path, &error)) status_->setText(error); else status_->setText(QStringLiteral("路线已保存"));
    });
    connect(selected_, &QPushButton::clicked, this, [this] { requestRun(table_->currentRow(), true); });
    connect(from_, &QPushButton::clicked, this, [this] { requestRun(table_->currentRow(), false); });
    connect(all_, &QPushButton::clicked, this, [this] { requestRun(0, false); });
    connect(confirm_, &QPushButton::clicked, runner_, &RouteSequenceRunner::confirm);
    connect(stop_, &QPushButton::clicked, runner_, &RouteSequenceRunner::stop);
    connect(runner_, &RouteSequenceRunner::runningChanged, this, [this] { updateControls(); });
    connect(runner_, &RouteSequenceRunner::confirmationChanged, this, [this] { updateControls(); });
    connect(runner_, &RouteSequenceRunner::progressChanged, this, [this](int row, const QString &text) {
        table_->selectRow(row); status_->setText(text);
    });
    connect(runner_, &RouteSequenceRunner::failed, status_, &QLabel::setText);
    connect(runner_, &RouteSequenceRunner::completed, this, [this] { status_->setText(QStringLiteral("路线执行完成")); });
    updateControls();
}

RouteSequence RouteEditorPage::sequence() const { auto result = sequence_; result.name = name_->text(); return result; }
void RouteEditorPage::showError(const QString &error) { status_->setText(error); }
void RouteEditorPage::setSequence(const RouteSequence &sequence) {
    if (runner_->running()) return;
    sequence_ = sequence; name_->setText(sequence.name); refresh(sequence.steps.isEmpty() ? -1 : 0);
}
bool RouteEditorPage::loadFile(const QString &path, QString *error) {
    if (runner_->running()) { if (error) *error = QStringLiteral("运行中不能打开路线"); return false; }
    RouteSequence loaded;
    if (!loadRouteSequence(path, &loaded, error)) return false;
    setSequence(loaded); return true;
}
bool RouteEditorPage::saveFile(const QString &path, QString *error) const { return saveRouteSequence(path, sequence(), error); }
void RouteEditorPage::setConnected(bool connected) {
    connected_ = connected;
    if (!connected) runner_->disconnected();
    updateControls();
}

RouteStep RouteEditorPage::editedStep() const {
    RouteStep step; step.type = static_cast<RouteStepType>(type_->currentIndex());
    step.direction = step.type == RouteStepType::Turn ? (turnDirection_->currentIndex() ? "R" : "L") :
        QStringList{"W", "S", "A", "D"}[moveDirection_->currentIndex()];
    step.distanceMm = distance_->value(); step.rpm = rpm_->value(); step.degrees = degrees_->value();
    step.ring = visionMode_->currentIndex() == 1; step.target = target_->value();
    step.forwardMilli = forward_->value(); step.rightMilli = right_->value();
    step.action = importedAction_; step.waitMs = waitMs_->value(); step.prompt = prompt_->text(); return step;
}

void RouteEditorPage::loadSelected() {
    if (runner_->running()) return;
    const int row = table_->currentRow(); if (row < 0 || row >= sequence_.steps.size()) return;
    const auto &step = sequence_.steps[row]; type_->setCurrentIndex(static_cast<int>(step.type));
    if (step.type == RouteStepType::Move) moveDirection_->setCurrentIndex(QStringList{"W", "S", "A", "D"}.indexOf(step.direction));
    if (step.type == RouteStepType::Turn) turnDirection_->setCurrentIndex(step.direction == "R" ? 1 : 0);
    distance_->setValue(step.distanceMm); rpm_->setValue(step.rpm); degrees_->setValue(step.degrees);
    visionMode_->setCurrentIndex(step.ring ? 1 : 0); target_->setValue(step.target);
    forward_->setValue(step.forwardMilli); right_->setValue(step.rightMilli);
    if (step.type == RouteStepType::Action) { importedAction_ = step.action; actionLabel_->setText(routeStepDescription(step)); }
    waitMs_->setValue(step.waitMs); prompt_->setText(step.prompt);
}

void RouteEditorPage::refresh(int selected) {
    table_->blockSignals(true); table_->setRowCount(sequence_.steps.size());
    for (int row = 0; row < sequence_.steps.size(); ++row) {
        table_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        table_->setItem(row, 1, new QTableWidgetItem(routeStepDescription(sequence_.steps[row])));
    }
    table_->blockSignals(false); table_->selectRow(selected); loadSelected(); updateControls();
}

void RouteEditorPage::updateControls() {
    const bool idle = !runner_->running(); editControls_->setEnabled(idle);
    const bool selected = table_->currentRow() >= 0;
    selected_->setEnabled(connected_ && idle && selected); from_->setEnabled(connected_ && idle && selected);
    all_->setEnabled(connected_ && idle && !sequence_.steps.isEmpty());
    stop_->setEnabled(connected_ && !idle); confirm_->setEnabled(runner_->awaitingConfirmation());
}

void RouteEditorPage::requestRun(int from, bool selectedOnly) {
    if (!connected_ || runner_->running() || from < 0) return;
    QString error; const auto route = sequence();
    if (!validateRouteSequence(route, &error)) { status_->setText(error); return; }
    if (QMessageBox::question(this, QStringLiteral("确认路线测试"),
        QStringLiteral("确认当前位置、车头方向、机构初始状态和急停已检查。将执行所选路线，继续？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    emit executionRequested(route, from, selectedOnly);
}
