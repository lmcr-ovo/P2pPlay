#include "MediaService.h"

#include "VideoSampleCodec.h"
#include "TraceManager.h"
#include "P2pSession.h"

MediaService::MediaService(QObject* parent)
        : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new MediaServiceWorker;
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    connect(worker_, &MediaServiceWorker::logReceived,
            this, &MediaService::logReceived);
    connect(worker_, &MediaServiceWorker::errorOccurred,
            this, &MediaService::errorOccurred);

    thread_->start();
}

MediaService::~MediaService() {
    thread_->quit();
    thread_->wait();
    worker_ = nullptr;
}

MediaServiceWorker* MediaService::worker() const {
    return worker_;
}

void MediaService::setRole(Role role) {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, role] {
                worker->setRole(role);
            },
            Qt::BlockingQueuedConnection
            );
}

Role MediaService::role() const {
    Role result;
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, &result] {
                result = worker->role();
            },
            Qt::QueuedConnection
            );
    return result;
}

void MediaService::start() {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_] {
                worker->start();
            },
            Qt::BlockingQueuedConnection
    );
}

void MediaService::stop() {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_] {
                worker->stop();
            },
            Qt::BlockingQueuedConnection
    );

}

bool MediaService::isRunning() const {
    bool result;
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, &result] {
                result = worker->isRunning();
            },
            Qt::QueuedConnection
    );
    return result;
}
