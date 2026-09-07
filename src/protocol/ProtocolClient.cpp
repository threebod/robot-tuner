#include "protocol/ProtocolClient.h"

#include "protocol/FrameCodec.h"

#include <QVector>

#include <utility>

ProtocolClient::ProtocolClient(QObject *parent)
    : ProtocolClient(kDefaultTimeoutMs, parent) {}

ProtocolClient::ProtocolClient(int timeoutMs, QObject *parent)
    : QObject(parent), timeoutMs_(qMax(1, timeoutMs)) {
    deadlineTimer_.setSingleShot(false);
    deadlineTimer_.setTimerType(Qt::PreciseTimer);
    connect(&deadlineTimer_, &QTimer::timeout, this,
            &ProtocolClient::checkDeadlines);
}

quint8 ProtocolClient::sendRequest(protocol::Command command,
                                   QByteArray payload) {
    const quint8 sequence = allocateSequence();
    if (pending_.size() == 256 || pending_.contains(sequence)) {
        emit requestFailed(sequence, QStringLiteral("请求序号已用尽"));
        return sequence;
    }

    protocol::Frame frame;
    frame.flags = protocol::Request;
    frame.sequence = sequence;
    frame.command = static_cast<quint8>(command);
    frame.payload = std::move(payload);
    const QByteArray encoded = encodeFrame(frame);
    if (encoded.isEmpty()) {
        emit requestFailed(sequence, QStringLiteral("请求编码失败"));
        return sequence;
    }

    pending_.insert(sequence,
                    PendingRequest{frame, 1, QDeadlineTimer(timeoutMs_)});
    startDeadlineTimer();
    emit bytesReady(encoded);
    return sequence;
}

void ProtocolClient::ingestBytes(QByteArrayView bytes) {
    const QVector<protocol::Frame> frames = parser_.push(bytes);
    for (const protocol::Frame &frame : frames) {
        if ((frame.flags & protocol::Event) != 0) {
            emit eventReceived(frame);
            continue;
        }

        if ((frame.flags & (protocol::Response | protocol::Error)) == 0) {
            continue;
        }

        auto pending = pending_.find(frame.sequence);
        if (pending == pending_.end()) {
            continue;
        }
        if (pending->frame.command != frame.command) {
            continue;
        }

        pending_.erase(pending);
        if (pending_.isEmpty()) {
            deadlineTimer_.stop();
        }
        emit responseReceived(frame);
    }
}

void ProtocolClient::clearPending() {
    QVector<quint8> sequences;
    sequences.reserve(pending_.size());
    for (auto pending = pending_.cbegin(); pending != pending_.cend();
         ++pending) {
        sequences.push_back(pending.key());
    }

    pending_.clear();
    parser_.reset();
    deadlineTimer_.stop();

    emit connectionCleared();

    for (const quint8 sequence : sequences) {
        emit requestFailed(sequence, QString::fromUtf8("连接已断开"));
    }
}

void ProtocolClient::checkDeadlines() {
    struct Retry {
        quint8 sequence;
        QByteArray bytes;
    };
    QVector<Retry> retries;
    QVector<quint8> failures;
    for (auto pending = pending_.begin(); pending != pending_.end();) {
        if (!pending->deadline.hasExpired()) {
            ++pending;
            continue;
        }

        if (pending->retriesRemaining > 0) {
            --pending->retriesRemaining;
            pending->deadline = QDeadlineTimer(timeoutMs_);
            retries.push_back({pending.key(), encodeFrame(pending->frame)});
            ++pending;
            continue;
        }

        failures.push_back(pending.key());
        pending = pending_.erase(pending);
    }

    for (const Retry &retry : retries) {
        if (pending_.contains(retry.sequence)) {
            emit bytesReady(retry.bytes);
        }
    }
    for (const quint8 sequence : failures) {
        emit requestFailed(sequence, QStringLiteral("请求超时"));
    }

    if (pending_.isEmpty()) {
        deadlineTimer_.stop();
    }
}

quint8 ProtocolClient::allocateSequence() {
    const quint8 firstCandidate = nextSequence_;
    quint8 candidate = firstCandidate;
    do {
        if (!pending_.contains(candidate)) {
            nextSequence_ = static_cast<quint8>(candidate + 1);
            return candidate;
        }
        candidate = static_cast<quint8>(candidate + 1);
    } while (candidate != firstCandidate);

    return firstCandidate;
}

void ProtocolClient::startDeadlineTimer() {
    if (!deadlineTimer_.isActive()) {
        deadlineTimer_.start(qMax(1, qMin(timeoutMs_ / 4, 10)));
    }
}
