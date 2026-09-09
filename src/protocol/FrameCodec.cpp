#include "protocol/FrameCodec.h"

#include "protocol/Crc16.h"

bool isValidFrameFlags(quint8 flags) {
    return flags == protocol::Request || flags == protocol::Response ||
           flags == (protocol::Response | protocol::Error) ||
           flags == protocol::Event;
}

QByteArray encodeFrame(const protocol::Frame &frame) {
    if (frame.version != 1 || !isValidFrameFlags(frame.flags) ||
        frame.payload.size() > 128) {
        return {};
    }

    const auto length = static_cast<quint16>(frame.payload.size());
    QByteArray body;
    body.reserve(6 + frame.payload.size());
    body.append(static_cast<char>(frame.version));
    body.append(static_cast<char>(frame.flags));
    body.append(static_cast<char>(frame.sequence));
    body.append(static_cast<char>(frame.command));
    body.append(static_cast<char>(length & 0xFF));
    body.append(static_cast<char>((length >> 8) & 0xFF));
    body.append(frame.payload);

    const quint16 crc = crc16CcittFalse(QByteArrayView(body));
    QByteArray encoded;
    encoded.reserve(body.size() + 4);
    encoded.append(static_cast<char>(0xAA));
    encoded.append(static_cast<char>(0x55));
    encoded.append(body);
    encoded.append(static_cast<char>(crc & 0xFF));
    encoded.append(static_cast<char>((crc >> 8) & 0xFF));
    return encoded;
}
