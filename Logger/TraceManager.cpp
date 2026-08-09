//
// Created by ASUS on 2026/8/7.
//

#include "TraceManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QMutexLocker>

namespace {
constexpr qint64 TraceTimeoutUs = 2 * 1000 * 1000;

const std::initializer_list<TraceStage> HostStages = {
        TraceStage::CaptureEnd,
        TraceStage::EncodeEnd,
        TraceStage::PackEnd,
        TraceStage::SendEnd
};

const std::initializer_list<TraceStage> GuestStages = {
        TraceStage::ReassembleEnd,
        TraceStage::DecodeEnd,
        TraceStage::RenderEnd
};
}

TraceManager& TraceManager::instance() {
    static TraceManager manager(QCoreApplication::instance());
    return manager;
}

TraceManager::TraceManager(QObject* parent)
    : QObject(parent) {
    QString baseDir = QCoreApplication::instance() != nullptr
            ? QCoreApplication::applicationDirPath()
            : QDir::currentPath();
    QDir dir(baseDir);
    if (!dir.exists("logs")) {
        dir.mkpath("logs");
    }

    const QString fileName = dir.filePath(
            QString("logs/trace_%1.log")
                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")));
    logger_ = std::make_unique<Logger>(nullptr, fileName);
}

TraceManager::~TraceManager() {
    QMutexLocker locker(&mutex_);
    for (auto it = traces_.begin(); it != traces_.end(); ++it) {
        if (!it.value().written) {
            writeTraceLine(it.value(), true);
        }
    }
}

qint64 TraceManager::nowUs() {
    static const qint64 baseWallUs = QDateTime::currentMSecsSinceEpoch() * 1000LL;
    static QElapsedTimer timer;
    static const bool started = [] {
        timer.start();
        return true;
    }();
    Q_UNUSED(started)

    return baseWallUs + timer.nsecsElapsed() / 1000LL;
}

void TraceManager::record(const TraceEvent& event) {
    record(event.sampleId, event.stage, event.timestampUs);
}

void TraceManager::record(quint32 sampleId, TraceStage stage, qint64 timestampUs) {
    if (sampleId == 0) {
        return;
    }

    if (timestampUs <= 0) {
        return;
    }

    QMutexLocker locker(&mutex_);

    const quint64 key = traceKey(sampleId, stage);
    FrameTrace& trace = traces_[key];
    if (trace.written) {
        return;
    }

    trace.sampleId = sampleId;
    trace.timestampsUs[stage] = timestampUs;
    trace.wallTimestampsMs[stage] = QDateTime::currentMSecsSinceEpoch();
    if (trace.firstTimestampUs == 0 || timestampUs < trace.firstTimestampUs) {
        trace.firstTimestampUs = timestampUs;
    }
    if (timestampUs > trace.lastTimestampUs) {
        trace.lastTimestampUs = timestampUs;
    }

    flushIfComplete(trace);
    flushExpired(timestampUs);
}

void TraceManager::flushIfComplete(FrameTrace& trace) {
    if (trace.written) {
        return;
    }

    if (hasStages(trace, HostStages) || hasStages(trace, GuestStages)) {
        writeTraceLine(trace, false);
    }
}

void TraceManager::flushExpired(qint64 nowUsValue) {
    for (auto it = traces_.begin(); it != traces_.end(); ) {
        FrameTrace& trace = it.value();
        if (!trace.written
            && trace.lastTimestampUs > 0
            && nowUsValue - trace.lastTimestampUs >= TraceTimeoutUs) {
            writeTraceLine(trace, true);
        }

        if (trace.written) {
            it = traces_.erase(it);
        } else {
            ++it;
        }
    }
}

void TraceManager::writeTraceLine(const FrameTrace& trace, bool timeout) {
    if (logger_ == nullptr) {
        return;
    }

    logger_->writeLine(buildLine(trace, timeout));
    const_cast<FrameTrace&>(trace).written = true;
}

QString TraceManager::buildLine(const FrameTrace& trace, bool timeout) const {
    const QString status = timeout ? "timeout" : "complete";

    if (isHostTrace(trace)) {
        const QString missing = missingStages(trace, HostStages);
        QString line = QString("sample=%1 side=host status=%2 "
                               "transportHandoffWallMs=%3 "
                               "captureToEncode=%4ms "
                               "encodeToPack=%5ms "
                               "packToTransport=%6ms "
                               "hostTotal=%7ms")
                .arg(trace.sampleId)
                .arg(status)
                .arg(wallMs(trace, TraceStage::SendEnd))
                .arg(durationMs(trace, TraceStage::CaptureEnd, TraceStage::EncodeEnd))
                .arg(durationMs(trace, TraceStage::EncodeEnd, TraceStage::PackEnd))
                .arg(durationMs(trace, TraceStage::PackEnd, TraceStage::SendEnd))
                .arg(durationMs(trace, TraceStage::CaptureEnd, TraceStage::SendEnd));
        if (!missing.isEmpty()) {
            line += QString(" missing=%1").arg(missing);
        }
        return line;
    }

    const QString missing = missingStages(trace, GuestStages);
    QString line = QString("sample=%1 side=guest status=%2 "
                           "receiveWallMs=%3 "
                           "receiveToDecode=%4ms "
                           "decodeToRender=%5ms "
                           "guestTotal=%6ms")
            .arg(trace.sampleId)
            .arg(status)
            .arg(wallMs(trace, TraceStage::ReassembleEnd))
            .arg(durationMs(trace, TraceStage::ReassembleEnd, TraceStage::DecodeEnd))
            .arg(durationMs(trace, TraceStage::DecodeEnd, TraceStage::RenderEnd))
            .arg(durationMs(trace, TraceStage::ReassembleEnd, TraceStage::RenderEnd));
    if (!missing.isEmpty()) {
        line += QString(" missing=%1").arg(missing);
    }
    return line;
}

bool TraceManager::isHostTrace(const FrameTrace& trace) {
    return trace.timestampsUs.contains(TraceStage::CaptureEnd)
           || trace.timestampsUs.contains(TraceStage::EncodeEnd)
           || trace.timestampsUs.contains(TraceStage::PackEnd)
           || trace.timestampsUs.contains(TraceStage::SendEnd);
}

bool TraceManager::isGuestTrace(const FrameTrace& trace) {
    return trace.timestampsUs.contains(TraceStage::ReassembleEnd)
           || trace.timestampsUs.contains(TraceStage::DecodeEnd)
           || trace.timestampsUs.contains(TraceStage::RenderEnd);
}

bool TraceManager::isHostStage(TraceStage stage) {
    return stage == TraceStage::CaptureEnd
           || stage == TraceStage::EncodeEnd
           || stage == TraceStage::PackEnd
           || stage == TraceStage::SendEnd;
}

quint64 TraceManager::traceKey(quint32 sampleId, TraceStage stage) {
    const quint64 side = isHostStage(stage) ? 0ULL : 1ULL;
    return (side << 32) | static_cast<quint64>(sampleId);
}

bool TraceManager::hasStages(const FrameTrace& trace,
        std::initializer_list<TraceStage> stages) {
    for (TraceStage stage : stages) {
        if (!trace.timestampsUs.contains(stage)) {
            return false;
        }
    }
    return true;
}

QString TraceManager::durationMs(const FrameTrace& trace, TraceStage begin, TraceStage end) {
    if (!trace.timestampsUs.contains(begin) || !trace.timestampsUs.contains(end)) {
        return "NA";
    }

    const qint64 durationUs = trace.timestampsUs.value(end) - trace.timestampsUs.value(begin);
    if (durationUs < 0) {
        return "NA";
    }

    return QString::number(durationUs / 1000.0, 'f', 3);
}

QString TraceManager::wallMs(const FrameTrace& trace, TraceStage stage) {
    if (!trace.wallTimestampsMs.contains(stage)) {
        return "NA";
    }

    return QString::number(trace.wallTimestampsMs.value(stage));
}

QString TraceManager::missingStages(const FrameTrace& trace,
        std::initializer_list<TraceStage> stages) {
    QStringList missing;
    for (TraceStage stage : stages) {
        if (!trace.timestampsUs.contains(stage)) {
            missing << traceStageName(stage);
        }
    }
    return missing.join(",");
}
