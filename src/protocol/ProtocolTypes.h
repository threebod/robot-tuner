#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace protocol {

enum Flag : quint8 {
    Request = 0x01,
    Response = 0x02,
    Event = 0x04,
    Error = 0x08
};

enum class Command : quint8 {
    Hello = 0x01,
    GetStatus = 0x02,
    GetParamGroup = 0x10,
    SetParamGroup = 0x11,
    SetTelemetry = 0x20,
    ImuCalibrate = 0x21,
    SetPose = 0x22,
    TestUnlock = 0x30,
    TestAction = 0x31,
    Stop = 0x32,
    EmergencyStop = 0x33,
    ClearEmergencyStop = 0x34,
    StatusTelemetry = 0x80,
    ImuTelemetry = 0x81,
    PidTelemetry = 0x82,
    PoseTelemetry = 0x83
};

enum Capability : quint32 {
    Pose = 0x00000001
};

struct Frame {
    quint8 version{1};
    quint8 flags{};
    quint8 sequence{};
    quint8 command{};
    QByteArray payload;
};

}  // namespace protocol
