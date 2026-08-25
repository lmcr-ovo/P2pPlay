//
// Created by ASUS on 2026/8/25.
//

#include "ControlService.h"

ControlService::ControlService(QObject* parent)
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
            &ControlService::logReceived);

    connect(worker_,
            &ControlServiceWorker::errorOccurred,
            this,
            &ControlService::errorOccurred);

    thread_->start();
}

ControlService::~ControlService() {
    thread_->quit();
    thread_->wait();

    worker_ = nullptr;
}

ControlServiceWorker* ControlService::worker() const {
    return worker_;
}

void ControlService::setRole(Role role) {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, role] {
                worker->setRole(role);
            },
            Qt::BlockingQueuedConnection);
}

Role ControlService::role() const {
    Role result = Role::Unknown;

    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, &result] {
                result = worker->role();
            },
            Qt::BlockingQueuedConnection);

    return result;
}

void ControlService::start() {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_] {
                worker->start();
            },
            Qt::BlockingQueuedConnection);
}

void ControlService::stop() {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_] {
                worker->stop();
            },
            Qt::BlockingQueuedConnection);
}

bool ControlService::isRunning() const {
    bool result = false;

    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, &result] {
                result = worker->isRunning();
            },
            Qt::BlockingQueuedConnection);

    return result;
}