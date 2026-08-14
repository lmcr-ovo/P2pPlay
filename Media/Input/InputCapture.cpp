//
// Created by ASUS on 2026/8/14.
//

#include "InputCapture.h"

InputCapture::InputCapture(QObject* parent)
        : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new InputCaptureWorker;

    worker_->moveToThread(thread_);

    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);

    thread_->start();
}

InputCapture::~InputCapture() {
    if (worker_ != nullptr) {
        QMetaObject::invokeMethod(
                worker_,
                &InputCaptureWorker::stop,
                Qt::BlockingQueuedConnection);
    }

    if (thread_ != nullptr) {
        thread_->quit();
        thread_->wait();
    }

    worker_ = nullptr;
}

InputCaptureWorker* InputCapture::worker() const {
    return worker_;
}

void InputCapture::start() {
    InputCaptureWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker] {
                worker->start();
            },
            Qt::QueuedConnection);
}

void InputCapture::stop() {
    InputCaptureWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker] {
                worker->stop();
            },
            Qt::QueuedConnection);
}