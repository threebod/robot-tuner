#include "protocol/FrameParser.h"

#include "protocol/Crc16.h"

#include <utility>

namespace {

int findSync(const QByteArray &buffer) {
    for (int index = 0; index + 1 < buffer.size(); ++index) {
        if (static_cast<quint8>(buffer.at(index)) == 0xAA &&
            static_cast<quint8>(buffer.at(index + 1)) == 0x55) {
            return index;
        }
    }
    return -1;
}

quint16 readLittleEndian(const QByteArray &bytes, int offset) {
    return static_cast<quint16>(
        static_cast<quint8>(bytes.at(offset)) |
        (static_cast<quint16>(static_cast<quint8>(bytes.at(offset + 1))) << 8));
}

}  // namespace

QVector<protocol::Frame> FrameParser::push(QByteArrayView bytes) {
    QVector<protocol::Frame> frames;
    for (const char byte : bytes) {
        buffer_.append(byte);
        parseAvailable(frames);
    }
    return frames;
}

FrameParser::Stats FrameParser::stats() const {
    return stats_;
}

void FrameParser::reset() {
    buffer_.clear();
    stats_ = {};
}

void FrameParser::parseAvailable(QVector<protocol::Frame> &frames) {
    while (true) {
        const int syncOffset = findSync(buffer_);
        if (syncOffset < 0) {
            if (!buffer_.isEmpty()) {
                const bool keepTrailingAa =
                    static_cast<quint8>(buffer_.back()) == 0xAA;
                const int discardCount =
                    buffer_.size() - (keepTrailingAa ? 1 : 0);
                if (discardCount > 0) {
                    stats_.discardedBytes += static_cast<quint64>(discardCount);
                    buffer_.remove(0, discardCount);
                }
            }
            return;
        }

        if (syncOffset > 0) {
            stats_.discardedBytes += static_cast<quint64>(syncOffset);
            buffer_.remove(0, syncOffset);
            continue;
        }

        if (buffer_.size() < kHeaderSize) {
            return;
        }

        const quint16 payloadLength = readLittleEndian(buffer_, 6);
        if (payloadLength > kMaxPayload) {
            ++stats_.lengthErrors;
            buffer_.remove(0, kSyncSize);
            continue;
        }

        const int frameSize = kHeaderSize + payloadLength + kTrailerSize;
        if (buffer_.size() < frameSize) {
            return;
        }

        const auto body = QByteArrayView(buffer_.constData(), buffer_.size())
                              .sliced(kSyncSize, kHeaderSize - kSyncSize +
                                                     payloadLength);
        const quint16 expectedCrc = readLittleEndian(buffer_, frameSize - 2);
        if (crc16CcittFalse(body) != expectedCrc) {
            ++stats_.crcErrors;
            buffer_.remove(0, 1);
            continue;
        }

        if (static_cast<quint8>(buffer_.at(2)) != 1) {
            buffer_.remove(0, frameSize);
            continue;
        }

        protocol::Frame frame;
        frame.version = static_cast<quint8>(buffer_.at(2));
        frame.flags = static_cast<quint8>(buffer_.at(3));
        frame.sequence = static_cast<quint8>(buffer_.at(4));
        frame.command = static_cast<quint8>(buffer_.at(5));
        frame.payload = buffer_.mid(kHeaderSize, payloadLength);
        frames.push_back(std::move(frame));
        buffer_.remove(0, frameSize);
    }
}
