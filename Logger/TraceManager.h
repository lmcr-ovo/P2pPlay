//
// Created by ASUS on 2026/8/7.
//

#ifndef P2PPLAY_TRACEMANAGER_H
#define P2PPLAY_TRACEMANAGER_H

#include <QObject>
#include <QMutex>
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

private:
    mutable QMutex mutex_;
    std::unique_ptr<Logger> logger_;
};

#endif //P2PPLAY_TRACEMANAGER_H
