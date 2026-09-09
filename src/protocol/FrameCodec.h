#pragma once

#include <QByteArray>

#include "protocol/ProtocolTypes.h"

bool isValidFrameFlags(quint8 flags);
QByteArray encodeFrame(const protocol::Frame &frame);
