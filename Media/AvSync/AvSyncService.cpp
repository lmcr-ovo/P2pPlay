//
// Created by ASUS on 2026/8/29.
//

#include "AvSyncService.h"

AvSyncService::AvSyncService(QObject* parent)
        : QObject(parent),
          worker_(new AvSyncWorker()) {
    worker_->moveToThread(&workerThread_);

    connect(&workerThread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    connect(worker_, &AvSyncWorker::videoFrameToRender,
            this, &AvSyncService::videoFrameToRender);
    connect(worker_, &AvSyncWorker::logReceived,
            this, &AvSyncService::logReceived);
    connect(worker_, &AvSyncWorker::errorOccurred,
            this, &AvSyncService::errorOccurred);

    workerThread_.start();
}

AvSyncService::~AvSyncService() {
    if (workerThread_.isRunning()) {
        workerThread_.quit();
        workerThread_.wait();
    }
    worker_ = nullptr;
}

AvSyncWorker* AvSyncService::worker() const {
    return worker_;
}

void AvSyncService::setRole(Role role) {
    QMetaObject::invokeMethod(
            worker_,
            [this, role] { worker_->setRole(role); },
            Qt::QueuedConnection);
}

void AvSyncService::setVideoEnabled(bool enabled) {
    QMetaObject::invokeMethod(
            worker_,
            [this, enabled] { worker_->setVideoEnabled(enabled); },
            Qt::QueuedConnection);
}

void AvSyncService::setAudioEnabled(bool enabled) {
    QMetaObject::invokeMethod(
            worker_,
            [this, enabled] { worker_->setAudioEnabled(enabled); },
            Qt::QueuedConnection);
}

void AvSyncService::setAvSyncEnabled(bool enabled) {
    QMetaObject::invokeMethod(
            worker_,
            [this, enabled] { worker_->setAvSyncEnabled(enabled); },
            Qt::QueuedConnection);
}
