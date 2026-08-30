//
// Created by ASUS on 2026/8/24.
//

#include "InputService.h"

InputService::InputService(QObject* parent)
    : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new InputServiceWorker;
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);

    thread_->start();
}

InputService::~InputService() {
    if (thread_ != nullptr) {
        thread_->quit();
        thread_->wait();
    }
    worker_ = nullptr;
}

void InputService::setRole(const Role &role) {
    role_ = role;
    InputServiceWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, role] {
                worker->setRole(role);
            },
            Qt::QueuedConnection
    );
}

InputServiceWorker * InputService::worker() const {
    return worker_;
}

void InputService::start() {
    InputServiceWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker] {
                worker->start();
            },
            Qt::QueuedConnection
    );
}

void InputService::setControlActive(bool active) {
    if (role_ == Role::Guest) {
        worker_->setControlActive(active);

        InputServiceWorker* worker = worker_;
        QMetaObject::invokeMethod(
                worker,
                [worker, active] {
                    worker->setControlActive(active);
                },
                Qt::QueuedConnection
        );
    }
}