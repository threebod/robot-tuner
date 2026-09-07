#include <QByteArray>
#include <QByteArrayView>
#include <QVector>

#include <iostream>
#include <utility>

#include "protocol/FrameCodec.h"
#include "protocol/FrameParser.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

protocol::Frame makeFrame(quint8 sequence, quint8 command, QByteArray payload) {
    protocol::Frame frame;
    frame.flags = protocol::Request;
    frame.sequence = sequence;
    frame.command = command;
    frame.payload = std::move(payload);
    return frame;
}

bool sameFrame(const protocol::Frame &actual, const protocol::Frame &expected) {
    return actual.version == expected.version &&
           actual.flags == expected.flags &&
           actual.sequence == expected.sequence &&
           actual.command == expected.command &&
           actual.payload == expected.payload;
}

}  // namespace

int main() {
    const protocol::Frame first = makeFrame(1, 0x01, QByteArray("A"));
    const protocol::Frame second = makeFrame(2, 0x02, QByteArray("BC"));
    const QByteArray firstBytes = encodeFrame(first);
    const QByteArray secondBytes = encodeFrame(second);

    FrameParser splitParser;
    if (!require(splitParser.push(QByteArrayView(firstBytes.constData(), 3)).isEmpty(),
                 "a partial frame was emitted before its final bytes")) {
        return 1;
    }
    const QVector<protocol::Frame> splitResult = splitParser.push(
        QByteArrayView(firstBytes.constData() + 3, firstBytes.size() - 3));
    if (!require(splitResult.size() == 1 && sameFrame(splitResult.front(), first),
                 "a split frame was not reconstructed")) {
        return 1;
    }
    const QVector<protocol::Frame> stickyResult = splitParser.push(
        QByteArrayView(firstBytes + secondBytes));
    if (!require(stickyResult.size() == 2 && sameFrame(stickyResult.at(0), first) &&
                     sameFrame(stickyResult.at(1), second),
                 "concatenated frames were not both emitted")) {
        return 1;
    }

    FrameParser noiseParser;
    const QByteArray noise = QByteArray::fromHex("00 04 05 06");
    const QVector<protocol::Frame> noiseResult = noiseParser.push(
        QByteArrayView(noise + firstBytes));
    if (!require(noiseResult.size() == 1 && sameFrame(noiseResult.front(), first),
                 "noise before a frame prevented recovery")) {
        return 1;
    }
    if (!require(noiseParser.stats().discardedBytes == static_cast<quint64>(noise.size()),
                 "discarded noise byte count mismatch")) {
        return 1;
    }

    FrameParser crcParser;
    QByteArray corrupted = firstBytes;
    corrupted[8] = static_cast<char>(static_cast<quint8>(corrupted.at(8)) ^ 0x01);
    const QVector<protocol::Frame> crcResult = crcParser.push(
        QByteArrayView(corrupted + secondBytes));
    if (!require(crcResult.size() == 1 && sameFrame(crcResult.front(), second),
                 "a CRC error did not recover at the next frame")) {
        return 1;
    }
    if (!require(crcParser.stats().crcErrors == 1,
                 "CRC error was not counted exactly once")) {
        return 1;
    }

    FrameParser lengthParser;
    const QByteArray oversizedHeader = QByteArray::fromHex(
        "AA 55 01 01 10 20 81 00");
    const QVector<protocol::Frame> lengthResult = lengthParser.push(
        QByteArrayView(oversizedHeader + secondBytes));
    if (!require(lengthResult.size() == 1 && sameFrame(lengthResult.front(), second),
                 "an oversized length did not recover at the next frame")) {
        return 1;
    }
    if (!require(lengthParser.stats().lengthErrors == 1,
                 "oversized length was not counted exactly once")) {
        return 1;
    }

    FrameParser trailingHeaderParser;
    const QByteArray trailingAa = QByteArray::fromHex("00 AA");
    if (!require(trailingHeaderParser.push(QByteArrayView(trailingAa)).isEmpty(),
                 "incomplete trailing sync unexpectedly emitted a frame")) {
        return 1;
    }
    const QVector<protocol::Frame> trailingResult = trailingHeaderParser.push(
        QByteArrayView(QByteArray("\x55", 1) + firstBytes.mid(2)));
    if (!require(trailingResult.size() == 1 && sameFrame(trailingResult.front(), first),
                 "a trailing sync byte was not retained across pushes")) {
        return 1;
    }

    return 0;
}
