#include <QApplication>

#include <iostream>

#include "widgets/TelemetryPlot.h"

namespace {

bool require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    TelemetryPlot plot(2, 1000);
    for (int sample = 0; sample < 1100; ++sample) {
        plot.append(sample, {static_cast<double>(sample),
                             static_cast<double>(-sample)});
    }

    if (!require(plot.sampleCount() == 1000,
                 "telemetry plot did not enforce its fixed capacity") ||
        !require(plot.oldestTimestamp() == 100,
                 "telemetry plot retained the wrong oldest timestamp") ||
        !require(plot.latestTimestamp() == 1099,
                 "telemetry plot retained the wrong latest timestamp")) {
        return 1;
    }

    plot.clear();
    return require(plot.sampleCount() == 0,
                   "telemetry plot clear did not remove samples")
               ? 0
               : 1;
}
