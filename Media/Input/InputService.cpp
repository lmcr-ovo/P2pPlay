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
    worker_->setRole(role_);
}

InputServiceWorker * InputService::worker() const {
    return worker_;
}

void InputService::start() {
    worker_->start();
}

void InputService::setControlActive(bool active) {
    if (role_ == Role::Guest) {
        worker_->setControlActive(active);
    }
}