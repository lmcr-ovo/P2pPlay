//
// Created by ASUS on 2026/8/7.
//

#ifndef P2PPLAY_TRACEMANAGER_H
#define P2PPLAY_TRACEMANAGER_H

#include <QObject>
#include <QMutex>
#include <QMap>
#include <memory>
#include "Logger.h"
#include "TraceEvent.h"

class TraceManager : public QObject {
    Q_OBJECT
public:
    static TraceManager& instance();
    static qint64 nowUs();

    void record(const TraceEvent& event);
    void record(quint32 sampleId, TraceStage stage, qint64 timestampUs);

    explicit TraceManager(QObject* parent = nullptr);
    ~TraceManager() override;

private:
    struct FrameTrace {
        quint32 sampleId = 0;
        QMap<TraceStage, qint64> timestampsUs;
        QMap<TraceStage, qint64> wallTimestampsMs;
        qint64 firstTimestampUs = 0;
        qint64 lastTimestampUs = 0;
        bool written = false;
    };

    QString buildLine(const FrameTrace& trace, bool timeout) const;
    void flushIfComplete(FrameTrace& trace);
    void flushExpired(qint64 nowUs);
    void writeTraceLine(const FrameTrace& trace, bool timeout);
    static bool isHostTrace(const FrameTrace& trace);
    static bool isGuestTrace(const FrameTrace& trace);
    static bool isHostStage(TraceStage stage);
    static quint64 traceKey(quint32 sampleId, TraceStage stage);
    static bool hasStages(const FrameTrace& trace, std::initializer_list<TraceStage> stages);
    static QString durationMs(const FrameTrace& trace, TraceStage begin, TraceStage end);
    static QString wallMs(const FrameTrace& trace, TraceStage stage);
    static QString missingStages(const FrameTrace& trace, std::initializer_list<TraceStage> stages);

private:
    mutable QMutex mutex_;
    std::unique_ptr<Logger> logger_;
    QMap<quint64, FrameTrace> traces_;
};

#endif //P2PPLAY_TRACEMANAGER_H
