//
// Created by ASUS on 2026/8/25.
//

#include "ControlService.h"

ControlChannelService::ControlChannelService(QObject* parent)
        : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new ControlServiceWorker;

    worker_->moveToThread(thread_);

    connect(thread_,
            &QThread::finished,
            worker_,
            &QObject::deleteLater);

    connect(worker_,
            &ControlServiceWorker::logReceived,
            this,
            &ControlChannelService::logReceived);

    connect(worker_,
            &ControlServiceWorker::errorOccurred,
            this,
            &ControlChannelService::errorOccurred);

    thread_->start();
}

ControlChannelService::~ControlChannelService() {
    thread_->quit();
    thread_->wait();

    worker_ = nullptr;
}

ControlServiceWorker* ControlChannelService::worker() const {
    return worker_;
}

void ControlChannelService::setRole(Role role) {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, role] {
                worker->setRole(role);
            },
            Qt::BlockingQueuedConnection);
}

Role ControlChannelService::role() const {
    Role result = Role::Unknown;

    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, &result] {
                result = worker->role();
            },
            Qt::BlockingQueuedConnection);

    return result;
}

void ControlChannelService::start() {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_] {
                worker->start();
            },
            Qt::BlockingQueuedConnection);
}

void ControlChannelService::stop() {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_] {
                worker->stop();
            },
            Qt::BlockingQueuedConnection);
}

bool ControlChannelService::isRunning() const {
    bool result = false;

    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, &result] {
                result = worker->isRunning();
            },
            Qt::BlockingQueuedConnection);

    return result;
}
