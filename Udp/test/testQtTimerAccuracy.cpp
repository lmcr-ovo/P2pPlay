#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>
#include <QTimer>
#include <QVector>

#include <algorithm>

namespace {

struct TimerStats {
    int count = 0;
    double minMs = 0.0;
    double avgMs = 0.0;
    double p50Ms = 0.0;
    double p90Ms = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    double maxMs = 0.0;
    int over2Ms = 0;
    int over5Ms = 0;
    int over10Ms = 0;
};

QString timerTypeName(Qt::TimerType type) {
    switch (type) {
        case Qt::PreciseTimer:
            return "PreciseTimer";
        case Qt::CoarseTimer:
            return "CoarseTimer";
        case Qt::VeryCoarseTimer:
            return "VeryCoarseTimer";
    }
    return "UnknownTimer";
}

double percentile(const QVector<double>& sortedValues, double ratio) {
    if (sortedValues.isEmpty()) {
        return 0.0;
    }

    const int index = static_cast<int>((sortedValues.size() - 1) * ratio);
    return sortedValues[index];
}

TimerStats calculateStats(QVector<double> intervalsMs) {
    TimerStats stats;
    stats.count = intervalsMs.size();

    if (intervalsMs.isEmpty()) {
        return stats;
    }

    std::sort(intervalsMs.begin(), intervalsMs.end());

    double sum = 0.0;
    for (double value : intervalsMs) {
        sum += value;
        if (value > 2.0) {
            ++stats.over2Ms;
        }
        if (value > 5.0) {
            ++stats.over5Ms;
        }
        if (value > 10.0) {
            ++stats.over10Ms;
        }
    }

    stats.minMs = intervalsMs.first();
    stats.avgMs = sum / intervalsMs.size();
    stats.p50Ms = percentile(intervalsMs, 0.50);
    stats.p90Ms = percentile(intervalsMs, 0.90);
    stats.p95Ms = percentile(intervalsMs, 0.95);
    stats.p99Ms = percentile(intervalsMs, 0.99);
    stats.maxMs = intervalsMs.last();

    return stats;
}

void printUsage() {
    QTextStream out(stdout);
    out << "Usage:\n"
        << "  testQtTimerAccuracy [intervalMs=1] [samples=1000] [precise|coarse|both=both]\n\n"
        << "Examples:\n"
        << "  testQtTimerAccuracy\n"
        << "  testQtTimerAccuracy 1 2000 precise\n"
        << "  testQtTimerAccuracy 5 1000 both\n";
}

void printStats(Qt::TimerType type, int targetIntervalMs, const TimerStats& stats) {
    QTextStream out(stdout);
    out.setRealNumberNotation(QTextStream::FixedNotation);
    out.setRealNumberPrecision(3);

    out << "timerType=" << timerTypeName(type)
        << " targetIntervalMs=" << targetIntervalMs
        << " samples=" << stats.count
        << " min=" << stats.minMs << "ms"
        << " avg=" << stats.avgMs << "ms"
        << " p50=" << stats.p50Ms << "ms"
        << " p90=" << stats.p90Ms << "ms"
        << " p95=" << stats.p95Ms << "ms"
        << " p99=" << stats.p99Ms << "ms"
        << " max=" << stats.maxMs << "ms"
        << " over2ms=" << stats.over2Ms
        << " over5ms=" << stats.over5Ms
        << " over10ms=" << stats.over10Ms
        << "\n";
}

class TimerProbe : public QObject {
    Q_OBJECT

public:
    explicit TimerProbe(int targetIntervalMs,
                        int sampleCount,
                        QVector<Qt::TimerType> timerTypes,
                        QObject* parent = nullptr)
        : QObject(parent),
          targetIntervalMs_(targetIntervalMs),
          sampleCount_(sampleCount),
          timerTypes_(std::move(timerTypes)),
          timer_(this) {
        connect(&timer_, &QTimer::timeout, this, &TimerProbe::onTimeout);
    }

    void start() {
        startNextCase();
    }

private:
    void startNextCase() {
        if (currentCaseIndex_ >= timerTypes_.size()) {
            QCoreApplication::quit();
            return;
        }

        intervalsMs_.clear();
        intervalsMs_.reserve(sampleCount_);

        const Qt::TimerType timerType = timerTypes_[currentCaseIndex_];
        timer_.stop();
        timer_.setTimerType(timerType);
        timer_.setInterval(targetIntervalMs_);

        elapsed_.restart();
        timer_.start();
    }

    void onTimeout() {
        const double intervalMs = elapsed_.nsecsElapsed() / 1000000.0;
        elapsed_.restart();

        intervalsMs_.append(intervalMs);
        if (intervalsMs_.size() < sampleCount_) {
            return;
        }

        timer_.stop();

        const Qt::TimerType timerType = timerTypes_[currentCaseIndex_];
        printStats(timerType, targetIntervalMs_, calculateStats(intervalsMs_));

        ++currentCaseIndex_;
        QTimer::singleShot(0, this, &TimerProbe::startNextCase);
    }

private:
    int targetIntervalMs_ = 1;
    int sampleCount_ = 1000;
    QVector<Qt::TimerType> timerTypes_;
    int currentCaseIndex_ = 0;
    QTimer timer_;
    QElapsedTimer elapsed_;
    QVector<double> intervalsMs_;
};

QVector<Qt::TimerType> parseTimerTypes(const QString& mode) {
    if (mode.compare("precise", Qt::CaseInsensitive) == 0) {
        return {Qt::PreciseTimer};
    }

    if (mode.compare("coarse", Qt::CaseInsensitive) == 0) {
        return {Qt::CoarseTimer};
    }

    return {Qt::PreciseTimer, Qt::CoarseTimer};
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QStringList args = QCoreApplication::arguments();
    if (args.contains("-h") || args.contains("--help")) {
        printUsage();
        return 0;
    }

    const int targetIntervalMs = args.size() > 1 ? qMax(1, args[1].toInt()) : 1;
    const int sampleCount = args.size() > 2 ? qMax(1, args[2].toInt()) : 1000;
    const QString mode = args.size() > 3 ? args[3] : "both";

    TimerProbe probe(targetIntervalMs, sampleCount, parseTimerTypes(mode));
    QTimer::singleShot(0, &probe, &TimerProbe::start);

    return app.exec();
}

#include "testQtTimerAccuracy.moc"
