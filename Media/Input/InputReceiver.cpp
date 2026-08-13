//
// Created by ASUS on 2026/8/13.
//

#include "InputReceiver.h"

InputReceiver::InputReceiver(QObject* parent)
    : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new InputReceiverWorker;

    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    thread_->start();
}

InputReceiver::~InputReceiver() {
    disconnect(worker_, nullptr, nullptr, nullptr);
    thread_->quit();
    thread_->wait();
    worker_ = nullptr;
}

InputReceiverWorker* InputReceiver::worker() const {
    return worker_;
}