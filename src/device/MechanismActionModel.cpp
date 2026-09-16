#include "MechanismActionModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>

namespace {
bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}

bool validPose(const MechanismPoseData &pose) {
    return pose.horizontalDmm >= -1220 && pose.horizontalDmm <= 650 &&
           pose.liftDmm >= 0 && pose.liftDmm <= 1350 &&
           pose.turretDdeg >= 0 && pose.turretDdeg <= 3600 &&
           pose.horizontalRpm >= 10 && pose.horizontalRpm <= 2000 &&
           pose.liftRpm >= 10 && pose.liftRpm <= 2000 &&
           pose.horizontalAccel >= 1 && pose.horizontalAccel <= 240 &&
           pose.liftAccel >= 1 && pose.liftAccel <= 240 &&
           pose.turretDps10 >= 10 && pose.turretDps10 <= 300;
}

QJsonObject poseJson(const MechanismPoseData &pose) {
    return {{QStringLiteral("horizontalDmm"), pose.horizontalDmm},
            {QStringLiteral("liftDmm"), pose.liftDmm},
            {QStringLiteral("turretDdeg"), pose.turretDdeg},
            {QStringLiteral("horizontalRpm"), pose.horizontalRpm},
            {QStringLiteral("horizontalAccel"), pose.horizontalAccel},
            {QStringLiteral("liftRpm"), pose.liftRpm},
            {QStringLiteral("liftAccel"), pose.liftAccel},
            {QStringLiteral("turretDps10"), pose.turretDps10}};
}

bool integer(const QJsonObject &object, const QString &key, int minimum,
             int maximum, int *value, QString *error) {
    const QJsonValue json = object.value(key);
    if (!json.isDouble()) return fail(error, QStringLiteral("缺少整数字段：%1").arg(key));
    const int result = json.toInt(minimum - 1);
    if (result < minimum || result > maximum)
        return fail(error, QStringLiteral("字段越界：%1").arg(key));
    *value = result;
    return true;
}

bool readPose(const QJsonObject &object, MechanismPoseData *pose, QString *error) {
    return integer(object, QStringLiteral("horizontalDmm"), -1220, 650,
                   &pose->horizontalDmm, error) &&
           integer(object, QStringLiteral("liftDmm"), 0, 1350,
                   &pose->liftDmm, error) &&
           integer(object, QStringLiteral("turretDdeg"), 0, 3600,
                   &pose->turretDdeg, error) &&
           integer(object, QStringLiteral("horizontalRpm"), 10, 2000,
                   &pose->horizontalRpm, error) &&
           integer(object, QStringLiteral("horizontalAccel"), 1, 240,
                   &pose->horizontalAccel, error) &&
           integer(object, QStringLiteral("liftRpm"), 10, 2000,
                   &pose->liftRpm, error) &&
           integer(object, QStringLiteral("liftAccel"), 1, 240,
                   &pose->liftAccel, error) &&
           integer(object, QStringLiteral("turretDps10"), 10, 300,
                   &pose->turretDps10, error);
}

QString typeName(MechanismStepType type) {
    switch (type) {
    case MechanismStepType::Pose: return QStringLiteral("pose");
    case MechanismStepType::Gripper: return QStringLiteral("gripper");
    case MechanismStepType::Platform: return QStringLiteral("platform");
    case MechanismStepType::Servo: return QStringLiteral("servo");
    case MechanismStepType::Wait: return QStringLiteral("wait");
    }
    return {};
}

QString cIdentifier(QString name) {
    QString result;
    bool underscore = false;
    for (const QChar character : name) {
        const ushort code = character.unicode();
        const bool allowed = (code >= 'a' && code <= 'z') ||
                             (code >= 'A' && code <= 'Z') ||
                             (code >= '0' && code <= '9');
        if (allowed) {
            result += character.toLower();
            underscore = false;
        } else if (!result.isEmpty() && !underscore) {
            result += QLatin1Char('_');
            underscore = true;
        }
    }
    while (result.endsWith(QLatin1Char('_'))) result.chop(1);
    if (result.isEmpty()) result = QStringLiteral("sequence");
    return QStringLiteral("action_%1").arg(result);
}
}

bool validateMechanismSequence(const MechanismSequence &sequence, QString *error) {
    if (sequence.name.trimmed().isEmpty()) return fail(error, QStringLiteral("动作名称不能为空"));
    if (!validPose(sequence.initial.pose)) return fail(error, QStringLiteral("初始姿态越界"));
    if (sequence.initial.platform < 1 || sequence.initial.platform > 3 ||
        sequence.initial.gripperOpenDeg < 0 || sequence.initial.gripperOpenDeg > 270 ||
        sequence.initial.gripperCloseDeg < 0 || sequence.initial.gripperCloseDeg > 270)
        return fail(error, QStringLiteral("初始夹爪或平台参数越界"));
    for (int angle : sequence.initial.platformDeg)
        if (angle < 0 || angle > 270) return fail(error, QStringLiteral("平台角度越界"));
    for (const MechanismStep &step : sequence.steps) {
        if (step.waitMs < 0 || step.waitMs > 60000)
            return fail(error, QStringLiteral("步骤等待时间越界"));
        if (step.type == MechanismStepType::Pose && !validPose(step.pose))
            return fail(error, QStringLiteral("步骤姿态越界"));
        if (step.type == MechanismStepType::Gripper && step.value != 0 && step.value != 1)
            return fail(error, QStringLiteral("夹爪状态无效"));
        if (step.type == MechanismStepType::Platform && (step.value < 1 || step.value > 3))
            return fail(error, QStringLiteral("平台位置无效"));
        if (step.type == MechanismStepType::Servo &&
            (step.channel < 2 || step.channel > 4 || step.value < 0 ||
             step.value > (step.channel == 4 ? 360 : 270)))
            return fail(error, QStringLiteral("舵机步骤越界"));
    }
    if (error) error->clear();
    return true;
}

bool saveMechanismSequence(const QString &path, const MechanismSequence &sequence,
                           QString *error) {
    if (!validateMechanismSequence(sequence, error)) return false;
    QJsonObject initial = poseJson(sequence.initial.pose);
    initial.insert(QStringLiteral("gripper"),
                   sequence.initial.gripperOpen ? QStringLiteral("open") : QStringLiteral("close"));
    initial.insert(QStringLiteral("platform"), sequence.initial.platform);
    initial.insert(QStringLiteral("gripperOpenDeg"), sequence.initial.gripperOpenDeg);
    initial.insert(QStringLiteral("gripperCloseDeg"), sequence.initial.gripperCloseDeg);
    QJsonArray platformAngles;
    for (int angle : sequence.initial.platformDeg) platformAngles.append(angle);
    initial.insert(QStringLiteral("platformDeg"), platformAngles);
    QJsonArray steps;
    for (const MechanismStep &step : sequence.steps) {
        QJsonObject object{{QStringLiteral("type"), typeName(step.type)},
                           {QStringLiteral("waitMs"), step.waitMs}};
        if (step.type == MechanismStepType::Pose) object.insert(QStringLiteral("pose"), poseJson(step.pose));
        if (step.type == MechanismStepType::Gripper || step.type == MechanismStepType::Platform)
            object.insert(QStringLiteral("value"), step.value);
        if (step.type == MechanismStepType::Servo) {
            object.insert(QStringLiteral("channel"), step.channel);
            object.insert(QStringLiteral("value"), step.value);
        }
        steps.append(object);
    }
    QJsonObject root{{QStringLiteral("version"), 1},
                     {QStringLiteral("name"), sequence.name},
                     {QStringLiteral("initialState"), initial},
                     {QStringLiteral("steps"), steps}};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
        return fail(error, file.errorString());
    if (error) error->clear();
    return true;
}

bool loadMechanismSequence(const QString &path, MechanismSequence *sequence,
                           QString *error) {
    if (!sequence) return fail(error, QStringLiteral("输出对象为空"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("JSON 格式错误"));
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1)
        return fail(error, QStringLiteral("不支持的动作文件版本"));
    if (!root.value(QStringLiteral("name")).isString() ||
        !root.value(QStringLiteral("initialState")).isObject() ||
        !root.value(QStringLiteral("steps")).isArray())
        return fail(error, QStringLiteral("动作文件字段不完整"));
    MechanismSequence result;
    result.name = root.value(QStringLiteral("name")).toString();
    const QJsonObject initial = root.value(QStringLiteral("initialState")).toObject();
    if (!readPose(initial, &result.initial.pose, error)) return false;
    const QString gripper = initial.value(QStringLiteral("gripper")).toString();
    if (gripper != QStringLiteral("open") && gripper != QStringLiteral("close"))
        return fail(error, QStringLiteral("初始夹爪状态无效"));
    result.initial.gripperOpen = gripper == QStringLiteral("open");
    if (!integer(initial, QStringLiteral("platform"), 1, 3, &result.initial.platform, error) ||
        !integer(initial, QStringLiteral("gripperOpenDeg"), 0, 270, &result.initial.gripperOpenDeg, error) ||
        !integer(initial, QStringLiteral("gripperCloseDeg"), 0, 270, &result.initial.gripperCloseDeg, error))
        return false;
    const QJsonArray platformAngles = initial.value(QStringLiteral("platformDeg")).toArray();
    if (platformAngles.size() != 3) return fail(error, QStringLiteral("平台角度数量错误"));
    for (int index = 0; index < 3; ++index) {
        if (!platformAngles[index].isDouble()) return fail(error, QStringLiteral("平台角度无效"));
        result.initial.platformDeg[index] = platformAngles[index].toInt(-1);
    }
    const QJsonArray steps = root.value(QStringLiteral("steps")).toArray();
    for (const QJsonValue &value : steps) {
        if (!value.isObject()) return fail(error, QStringLiteral("步骤格式错误"));
        const QJsonObject object = value.toObject();
        const QString type = object.value(QStringLiteral("type")).toString();
        MechanismStep step;
        if (!integer(object, QStringLiteral("waitMs"), 0, 60000, &step.waitMs, error)) return false;
        if (type == QStringLiteral("pose")) {
            step.type = MechanismStepType::Pose;
            if (!object.value(QStringLiteral("pose")).isObject() ||
                !readPose(object.value(QStringLiteral("pose")).toObject(), &step.pose, error)) return false;
        } else if (type == QStringLiteral("gripper")) {
            step.type = MechanismStepType::Gripper;
            if (!integer(object, QStringLiteral("value"), 0, 1, &step.value, error)) return false;
        } else if (type == QStringLiteral("platform")) {
            step.type = MechanismStepType::Platform;
            if (!integer(object, QStringLiteral("value"), 1, 3, &step.value, error)) return false;
        } else if (type == QStringLiteral("servo")) {
            step.type = MechanismStepType::Servo;
            if (!integer(object, QStringLiteral("channel"), 2, 4, &step.channel, error) ||
                !integer(object, QStringLiteral("value"), 0, step.channel == 4 ? 360 : 270,
                         &step.value, error)) return false;
        } else if (type == QStringLiteral("wait")) {
            step.type = MechanismStepType::Wait;
        } else return fail(error, QStringLiteral("未知步骤类型"));
        result.steps.push_back(step);
    }
    if (!validateMechanismSequence(result, error)) return false;
    *sequence = result;
    return true;
}

QString exportMechanismActionC(const MechanismSequence &sequence, QString *error) {
    if (!validateMechanismSequence(sequence, error)) return {};
    const QString id = cIdentifier(sequence.name);
    const MechanismInitialData &initial = sequence.initial;
    const MechanismPoseData &p = initial.pose;
    QStringList lines{
        QStringLiteral("#include \"mechanism_action.h\""), QString(),
        QStringLiteral("static const MechanismInitialState %1_initial =").arg(id),
        QStringLiteral("    MECH_INITIAL_STATE(%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15);")
            .arg(p.horizontalDmm).arg(p.liftDmm).arg(p.turretDdeg)
            .arg(p.horizontalRpm).arg(p.horizontalAccel).arg(p.liftRpm)
            .arg(p.liftAccel).arg(p.turretDps10).arg(initial.gripperOpen ? 1 : 0)
            .arg(initial.platform).arg(initial.gripperOpenDeg).arg(initial.gripperCloseDeg)
            .arg(initial.platformDeg[0]).arg(initial.platformDeg[1]).arg(initial.platformDeg[2]),
        QString(), QStringLiteral("static const MechanismAction %1[] = {").arg(id)};
    for (const MechanismStep &step : sequence.steps) {
        QString macro;
        if (step.type == MechanismStepType::Pose) {
            const MechanismPoseData &pose = step.pose;
            macro = QStringLiteral("    MECH_POSE(%1, %2, %3, %4, %5, %6, %7, %8, %9),")
                        .arg(pose.horizontalDmm).arg(pose.liftDmm).arg(pose.turretDdeg)
                        .arg(pose.horizontalRpm).arg(pose.horizontalAccel).arg(pose.liftRpm)
                        .arg(pose.liftAccel).arg(pose.turretDps10).arg(step.waitMs);
        } else if (step.type == MechanismStepType::Gripper) {
            macro = QStringLiteral("    MECH_GRIPPER_%1(%2),")
                        .arg(step.value ? QStringLiteral("OPEN") : QStringLiteral("CLOSE"))
                        .arg(step.waitMs);
        } else if (step.type == MechanismStepType::Platform) {
            macro = QStringLiteral("    MECH_PLATFORM(%1, %2),").arg(step.value).arg(step.waitMs);
        } else if (step.type == MechanismStepType::Servo) {
            macro = QStringLiteral("    MECH_SERVO(%1, %2, %3),")
                        .arg(step.channel).arg(step.value).arg(step.waitMs);
        } else macro = QStringLiteral("    MECH_WAIT(%1),").arg(step.waitMs);
        lines.push_back(macro);
    }
    lines << QStringLiteral("};") << QString()
          << QStringLiteral("#define %1_COUNT ((uint16_t)(sizeof(%2) / sizeof(%2[0])))")
                 .arg(id.toUpper(), id)
          << QString();
    if (error) error->clear();
    return lines.join(QLatin1Char('\n'));
}
