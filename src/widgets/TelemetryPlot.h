#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QPaintEvent;

class TelemetryPlot : public QWidget {
    Q_OBJECT

public:
    explicit TelemetryPlot(int channelCount, int capacity = 1000,
                           QWidget *parent = nullptr);

    void append(qint64 timestampMs, QVector<double> values);
    void clear();

    int sampleCount() const;
    qint64 oldestTimestamp() const;
    qint64 latestTimestamp() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Sample {
        qint64 timestampMs{};
        QVector<double> values;
    };

    static constexpr int kRefreshIntervalMs = 40;

    void scheduleRepaint();
    QVector<Sample> orderedSamples() const;

    const int channelCount_;
    const int capacity_;
    QVector<Sample> samples_;
    int oldestIndex_{};
    int sampleCount_{};
    QElapsedTimer repaintClock_;
    qint64 lastRepaintMs_{-kRefreshIntervalMs};
    QTimer repaintTimer_;
};
