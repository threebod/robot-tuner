#include "widgets/TelemetryPlot.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPalette>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

const QVector<QColor> kChannelColors = {
    QColor(44, 123, 182), QColor(230, 85, 13), QColor(49, 163, 84),
    QColor(117, 107, 177), QColor(217, 95, 14), QColor(99, 99, 99),
};

QColor channelColor(int channel) {
    return kChannelColors.at(channel % kChannelColors.size());
}

}  // namespace

TelemetryPlot::TelemetryPlot(int channelCount, int capacity, QWidget *parent)
    : QWidget(parent), channelCount_(qMax(1, channelCount)),
      capacity_(qMax(1, capacity)) {
    samples_.resize(capacity_);
    setMinimumSize(220, 120);
    repaintClock_.start();
    repaintTimer_.setSingleShot(true);
    connect(&repaintTimer_, &QTimer::timeout, this, [this]() {
        lastRepaintMs_ = repaintClock_.elapsed();
        update();
    });
}

void TelemetryPlot::append(qint64 timestampMs, QVector<double> values) {
    QVector<double> normalized(channelCount_, qQNaN());
    const int valueCount = qMin(channelCount_, values.size());
    for (int index = 0; index < valueCount; ++index) {
        normalized[index] = values.at(index);
    }

    const int writeIndex =
        sampleCount_ < capacity_
            ? (oldestIndex_ + sampleCount_) % capacity_
            : oldestIndex_;
    samples_[writeIndex] = Sample{timestampMs, std::move(normalized)};
    if (sampleCount_ < capacity_) {
        ++sampleCount_;
    } else {
        oldestIndex_ = (oldestIndex_ + 1) % capacity_;
    }
    scheduleRepaint();
}

void TelemetryPlot::clear() {
    sampleCount_ = 0;
    oldestIndex_ = 0;
    for (Sample &sample : samples_) {
        sample = Sample{};
    }
    repaintTimer_.stop();
    update();
}

void TelemetryPlot::setChannelNames(QStringList names) {
    channelNames_ = std::move(names);
    update();
}

int TelemetryPlot::sampleCount() const {
    return sampleCount_;
}

qint64 TelemetryPlot::oldestTimestamp() const {
    return sampleCount_ == 0 ? 0 : samples_.at(oldestIndex_).timestampMs;
}

qint64 TelemetryPlot::latestTimestamp() const {
    if (sampleCount_ == 0) {
        return 0;
    }
    const int latestIndex =
        (oldestIndex_ + sampleCount_ - 1) % capacity_;
    return samples_.at(latestIndex).timestampMs;
}

void TelemetryPlot::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().base());

    const QRectF plotRect = rect().adjusted(46, 26, -14, -26);
    painter.setPen(QPen(palette().mid().color(), 1));
    painter.drawRect(plotRect);

    const QVector<Sample> samples = orderedSamples();
    if (samples.isEmpty()) {
        painter.setPen(palette().text().color());
        painter.drawText(plotRect, Qt::AlignCenter,
                         QStringLiteral("暂无遥测数据"));
        return;
    }

    double minimum = 0.0;
    double maximum = 0.0;
    bool hasValue = false;
    for (const Sample &sample : samples) {
        for (double value : sample.values) {
            if (!std::isfinite(value)) {
                continue;
            }
            if (!hasValue) {
                minimum = maximum = value;
                hasValue = true;
            } else {
                minimum = std::min(minimum, value);
                maximum = std::max(maximum, value);
            }
        }
    }
    if (!hasValue) {
        minimum = -1.0;
        maximum = 1.0;
    } else if (qFuzzyCompare(minimum, maximum)) {
        const double padding = qMax(1.0, std::abs(minimum) * 0.1);
        minimum -= padding;
        maximum += padding;
    }

    painter.setPen(QPen(palette().mid().color(), 1, Qt::DashLine));
    if (minimum < 0.0 && maximum > 0.0) {
        const double zeroY = plotRect.bottom() -
                             ((0.0 - minimum) / (maximum - minimum)) *
                                 plotRect.height();
        painter.drawLine(QPointF(plotRect.left(), zeroY),
                         QPointF(plotRect.right(), zeroY));
    }

    const qint64 firstTimestamp = samples.first().timestampMs;
    const qint64 lastTimestamp = samples.last().timestampMs;
    const double timestampSpan =
        qMax<qint64>(1, lastTimestamp - firstTimestamp);
    for (int channel = 0; channel < channelCount_; ++channel) {
        QPainterPath path;
        bool pathStarted = false;
        for (const Sample &sample : samples) {
            if (channel >= sample.values.size() ||
                !std::isfinite(sample.values.at(channel))) {
                pathStarted = false;
                continue;
            }
            const double x = plotRect.left() +
                             (sample.timestampMs - firstTimestamp) /
                                 timestampSpan * plotRect.width();
            const double y = plotRect.bottom() -
                             ((sample.values.at(channel) - minimum) /
                              (maximum - minimum)) * plotRect.height();
            if (!pathStarted) {
                path.moveTo(x, y);
                pathStarted = true;
            } else {
                path.lineTo(x, y);
            }
        }
        painter.setPen(QPen(channelColor(channel), 1.5));
        painter.drawPath(path);
    }

    painter.setPen(palette().text().color());
    painter.drawText(QRectF(2, plotRect.top() - 1, 40, 18), Qt::AlignRight,
                     QString::number(maximum, 'f', 2));
    painter.drawText(QRectF(2, plotRect.bottom() - 17, 40, 18),
                     Qt::AlignRight, QString::number(minimum, 'f', 2));

    int legendX = static_cast<int>(plotRect.left());
    for (int channel = 0; channel < channelCount_; ++channel) {
        painter.setPen(channelColor(channel));
        const QString name = channel < channelNames_.size()
                                 ? channelNames_.at(channel)
                                 : QStringLiteral("CH%1").arg(channel + 1);
        painter.drawText(QRectF(legendX, 2, 80, 20), Qt::AlignLeft, name);
        legendX += 70;
    }
}

void TelemetryPlot::scheduleRepaint() {
    const qint64 elapsed = repaintClock_.elapsed();
    const qint64 sinceLast = elapsed - lastRepaintMs_;
    if (sinceLast >= kRefreshIntervalMs && !repaintTimer_.isActive()) {
        lastRepaintMs_ = elapsed;
        update();
        return;
    }
    if (!repaintTimer_.isActive()) {
        repaintTimer_.start(static_cast<int>(qMax<qint64>(
            1, kRefreshIntervalMs - sinceLast)));
    }
}

QVector<TelemetryPlot::Sample> TelemetryPlot::orderedSamples() const {
    QVector<Sample> ordered;
    ordered.reserve(sampleCount_);
    for (int index = 0; index < sampleCount_; ++index) {
        ordered.push_back(samples_.at((oldestIndex_ + index) % capacity_));
    }
    return ordered;
}
