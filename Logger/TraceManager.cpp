//
// Created by ASUS on 2026/8/7.
//

#include "TraceManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QMutexLocker>

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

qint64 TraceManager::nowUs() {
    return QDateTime::currentMSecsSinceEpoch() * 1000LL;
}

void TraceManager::record(const TraceEvent& event) {
    record(event.sampleId, event.stage, event.timestampUs);
}

void TraceManager::record(quint32 sampleId, TraceStage stage, qint64 timestampUs) {
    Q_UNUSED(timestampUs)

    if (sampleId == 0) {
        return;
    }

    const qint64 wallMs = QDateTime::currentMSecsSinceEpoch();
    QString line;
    if (stage == TraceStage::SendEnd) {
        line = QString("sample=%1 side=host event=send wallMs=%2")
                .arg(sampleId)
                .arg(wallMs);
    } else if (stage == TraceStage::ReassembleEnd) {
        line = QString("sample=%1 side=guest event=receive wallMs=%2")
                .arg(sampleId)
                .arg(wallMs);
    } else {
        return;
    }

    QMutexLocker locker(&mutex_);
    logger_->writeLine(line);
}
