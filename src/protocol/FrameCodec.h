#pragma once

#include <QByteArray>

#include "protocol/ProtocolTypes.h"

QByteArray encodeFrame(const protocol::Frame &frame);
