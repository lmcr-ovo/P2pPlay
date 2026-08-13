//
// Created by ASUS on 2026/8/13.
//

#include "InputSender.h"

InputSender::InputSender(QObject* parent)
    : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new InputSenderWorker;

    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    thread_->start();
}

InputSender::~InputSender() {
    disconnect(worker_, nullptr, nullptr, nullptr);
    thread_->quit();
    thread_->wait();
    worker_ = nullptr;
}

InputSenderWorker* InputSender::worker() const {
    return worker_;
}