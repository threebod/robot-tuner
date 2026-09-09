#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QVector>
#include <QtGlobal>

#include "protocol/ProtocolTypes.h"

class FrameParser {
public:
    struct Stats {
        quint64 crcErrors{0};
        quint64 lengthErrors{0};
        quint64 flagErrors{0};
        quint64 discardedBytes{0};
    };

    QVector<protocol::Frame> push(QByteArrayView bytes);
    Stats stats() const;
    void reset();

private:
    static constexpr int kSyncSize = 2;
    static constexpr int kHeaderSize = 8;
    static constexpr int kMaxPayload = 128;
    static constexpr int kTrailerSize = 2;

    void parseAvailable(QVector<protocol::Frame> &frames);

    QByteArray buffer_;
    Stats stats_;
};
