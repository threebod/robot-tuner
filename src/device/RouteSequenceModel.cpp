#include "device/RouteSequenceModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}

const QStringList types{QStringLiteral("move"), QStringLiteral("turn"),
    QStringLiteral("vision"), QStringLiteral("action"), QStringLiteral("wait"), QStringLiteral("confirm")};

bool number(const QJsonObject &object, const QString &key, int *out) {
    const auto value = object.value(key);
    if (!value.isDouble() || value.toDouble() != value.toInt(-1)) return false;
    *out = value.toInt();
    return true;
}
}

QString routeStepDescription(const RouteStep &step) {
    switch (step.type) {
    case RouteStepType::Move: {
        const QString direction = step.direction == "W" ? QStringLiteral("前进") :
            step.direction == "S" ? QStringLiteral("后退") :
            step.direction == "A" ? QStringLiteral("左移") : QStringLiteral("右移");
        return QStringLiteral("%1 %2 mm，%3 RPM").arg(direction).arg(step.distanceMm).arg(step.rpm);
    }
    case RouteStepType::Turn:
        return QStringLiteral("%1转 %2°").arg(step.direction == "L" ? QStringLiteral("左") : QStringLiteral("右")).arg(step.degrees);
    case RouteStepType::Vision:
        return QStringLiteral("%1 %2 对准，比例 %3 / %4，不回退")
            .arg(step.ring ? QStringLiteral("圆环") : QStringLiteral("物料颜色"))
            .arg(step.target).arg(step.forwardMilli).arg(step.rightMilli);
    case RouteStepType::Action:
        return QStringLiteral("动作组：%1（%2步，%3轮）").arg(step.action.name)
            .arg(step.action.steps.size()).arg(step.action.loopEnabled ? step.action.loopCount : 1);
    case RouteStepType::Wait: return QStringLiteral("等待 %1 ms").arg(step.waitMs);
    case RouteStepType::Confirm: return QStringLiteral("人工确认：%1").arg(step.prompt);
    }
    return {};
}

bool validateRouteSequence(const RouteSequence &sequence, QString *error) {
    if (sequence.name.trimmed().isEmpty()) return fail(error, QStringLiteral("路线名称不能为空"));
    for (int i = 0; i < sequence.steps.size(); ++i) {
        const auto &step = sequence.steps[i];
        bool valid = false;
        switch (step.type) {
        case RouteStepType::Move:
            valid = QStringList{"W", "S", "A", "D"}.contains(step.direction) &&
                step.distanceMm >= 1 && step.distanceMm <= 2400 && step.rpm >= 10 && step.rpm <= 250;
            break;
        case RouteStepType::Turn:
            valid = (step.direction == "L" || step.direction == "R") && step.degrees >= 1 && step.degrees <= 180;
            break;
        case RouteStepType::Vision:
            valid = step.target >= 1 && step.target <= (step.ring ? 3 : 6) &&
                step.forwardMilli >= 50 && step.forwardMilli <= 2000 && step.rightMilli >= 50 && step.rightMilli <= 2000;
            break;
        case RouteStepType::Action:
            valid = !step.action.steps.isEmpty() && validateMechanismSequence(step.action, error);
            break;
        case RouteStepType::Wait: valid = step.waitMs >= 0 && step.waitMs <= 60000; break;
        case RouteStepType::Confirm: valid = !step.prompt.trimmed().isEmpty(); break;
        }
        if (!valid) return fail(error, QStringLiteral("第 %1 步参数无效").arg(i + 1));
    }
    if (error) error->clear();
    return true;
}

bool saveRouteSequence(const QString &path, const RouteSequence &sequence, QString *error) {
    if (!validateRouteSequence(sequence, error)) return false;
    QJsonArray steps;
    for (const auto &step : sequence.steps) {
        QJsonObject object{{"type", types[static_cast<int>(step.type)]}};
        switch (step.type) {
        case RouteStepType::Move:
            object.insert("direction", step.direction); object.insert("distanceMm", step.distanceMm);
            object.insert("rpm", step.rpm); break;
        case RouteStepType::Turn:
            object.insert("direction", step.direction); object.insert("degrees", step.degrees); break;
        case RouteStepType::Vision:
            object.insert("ring", step.ring); object.insert("target", step.target);
            object.insert("forwardMilli", step.forwardMilli); object.insert("rightMilli", step.rightMilli); break;
        case RouteStepType::Action: object.insert("action", mechanismSequenceJson(step.action)); break;
        case RouteStepType::Wait: object.insert("waitMs", step.waitMs); break;
        case RouteStepType::Confirm: object.insert("prompt", step.prompt); break;
        }
        steps.append(object);
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    const QByteArray json = QJsonDocument(QJsonObject{{"version", 1}, {"name", sequence.name}, {"steps", steps}}).toJson();
    if (file.write(json) != json.size() || !file.commit()) return fail(error, file.errorString());
    return true;
}

bool loadRouteSequence(const QString &path, RouteSequence *sequence, QString *error) {
    if (!sequence) return fail(error, QStringLiteral("输出对象为空"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return fail(error, QStringLiteral("路线 JSON 格式错误"));
    const auto root = doc.object();
    if (root.value("version").toDouble(-1) != 1) return fail(error, QStringLiteral("不支持的路线文件版本"));
    if (!root.value("name").isString() || !root.value("steps").isArray()) return fail(error, QStringLiteral("路线文件字段不完整"));
    RouteSequence result;
    result.name = root.value("name").toString();
    for (const auto &value : root.value("steps").toArray()) {
        if (!value.isObject()) return fail(error, QStringLiteral("路线步骤格式错误"));
        const auto object = value.toObject();
        const int type = object.value("type").isString() ? types.indexOf(object.value("type").toString()) : -1;
        if (type < 0) return fail(error, QStringLiteral("未知路线步骤类型"));
        RouteStep step;
        step.type = static_cast<RouteStepType>(type);
        bool valid = false;
        switch (step.type) {
        case RouteStepType::Move:
            step.direction = object.value("direction").toString();
            valid = number(object, "distanceMm", &step.distanceMm) && number(object, "rpm", &step.rpm); break;
        case RouteStepType::Turn:
            step.direction = object.value("direction").toString();
            valid = number(object, "degrees", &step.degrees); break;
        case RouteStepType::Vision:
            step.ring = object.value("ring").toBool();
            valid = object.value("ring").isBool() && number(object, "target", &step.target) &&
                number(object, "forwardMilli", &step.forwardMilli) && number(object, "rightMilli", &step.rightMilli); break;
        case RouteStepType::Action:
            valid = object.value("action").isObject() && readMechanismSequenceJson(object.value("action").toObject(), &step.action, error); break;
        case RouteStepType::Wait: valid = number(object, "waitMs", &step.waitMs); break;
        case RouteStepType::Confirm:
            step.prompt = object.value("prompt").toString(); valid = object.value("prompt").isString(); break;
        }
        if (!valid) return fail(error, QStringLiteral("路线步骤字段无效"));
        result.steps.append(step);
    }
    if (!validateRouteSequence(result, error)) return false;
    *sequence = result;
    return true;
}
