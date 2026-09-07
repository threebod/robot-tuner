#include <QByteArray>
#include <QByteArrayView>
#include <QtGlobal>

#include <iostream>

#include "protocol/Crc16.h"
#include "protocol/FrameCodec.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    if (!require(crc16CcittFalse(QByteArrayView("123456789", 9)) == 0x29B1,
                 "CRC-16/CCITT-FALSE test vector mismatch")) {
        return 1;
    }

    protocol::Frame frame;
    frame.flags = protocol::Request;
    frame.sequence = 0x2A;
    frame.command = static_cast<quint8>(protocol::Command::GetStatus);
    frame.payload = QByteArray::fromHex("10 20 7F");

    // Expected bytes and CRC are hand-derived from the protocol fields.
    const QByteArray expected = QByteArray::fromHex(
        "AA 55 01 01 2A 02 03 00 10 20 7F 55 86");
    const QByteArray encoded = encodeFrame(frame);
    if (!require(encoded == expected, "encoded frame bytes mismatch")) {
        return 1;
    }
    if (!require(encoded.size() == 10 + frame.payload.size(),
                 "encoded frame size mismatch")) {
        return 1;
    }
    if (!require(static_cast<quint8>(encoded.at(0)) == 0xAA &&
                     static_cast<quint8>(encoded.at(1)) == 0x55,
                 "frame header mismatch")) {
        return 1;
    }
    if (!require(static_cast<quint8>(encoded.at(6)) == 0x03 &&
                     static_cast<quint8>(encoded.at(7)) == 0x00,
                 "payload length is not little-endian")) {
        return 1;
    }
    if (!require(encoded.mid(8, frame.payload.size()) == frame.payload,
                 "payload was not preserved")) {
        return 1;
    }

    const auto body = QByteArrayView(encoded).sliced(2, encoded.size() - 4);
    const quint16 recomputed = crc16CcittFalse(body);
    const auto crcOffset = encoded.size() - 2;
    const quint16 encodedCrc = static_cast<quint16>(
        static_cast<quint8>(encoded.at(crcOffset)) |
        (static_cast<quint16>(static_cast<quint8>(encoded.at(crcOffset + 1)))
         << 8));
    if (!require(recomputed == encodedCrc,
                 "trailing CRC does not match the encoded body")) {
        return 1;
    }

    frame.payload = QByteArray(128, '\xA5');
    const QByteArray maxEncoded = encodeFrame(frame);
    if (!require(maxEncoded.size() == 10 + 128,
                 "128-byte payload was not encoded at the protocol limit")) {
        return 1;
    }

    frame.payload = QByteArray(129, '\0');
    if (!require(encodeFrame(frame).isEmpty(),
                 "payload longer than 128 bytes was not rejected")) {
        return 1;
    }

    return 0;
}
