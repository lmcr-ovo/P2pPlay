//
// Created by ASUS on 2026/8/20.
//

#include "ReceiverMonitor.h"

ReceiverMonitor::ReceiverMonitor(QObject* parent)
        : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new ReceiverMonitorWorker;

    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_->start();
}

ReceiverMonitor::~ReceiverMonitor() {
    if (thread_ != nullptr) {
        thread_->quit();
        thread_->wait();
    }
    worker_ = nullptr;
}

void ReceiverMonitor::applyConfig(const AppConfig& config) {
    ReceiverMonitorWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, config] {
                worker->applyConfig(config);
            },
            Qt::QueuedConnection
    );
}

ReceiverMonitorWorker* ReceiverMonitor::worker() const {
    return worker_;
}