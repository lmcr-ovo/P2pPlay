//
// Created by ASUS on 2026/8/29.
//

#include "AudioService.h"

AudioService::AudioService(QObject* parent)
        : QObject(parent),
          worker_(new AudioServiceWorker()) {
    worker_->moveToThread(&workerThread_);

    connect(&workerThread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    connect(worker_, &AudioServiceWorker::logReceived,
            this, &AudioService::logReceived);
    connect(worker_, &AudioServiceWorker::errorOccurred,
            this, &AudioService::errorOccurred);

    workerThread_.start();
}

AudioService::~AudioService() {
    if (workerThread_.isRunning()) {
        QMetaObject::invokeMethod(
                worker_, "stop", Qt::BlockingQueuedConnection);
        workerThread_.quit();
        workerThread_.wait();
    }
    worker_ = nullptr;
}

AudioServiceWorker* AudioService::worker() const {
    return worker_;
}

void AudioService::applyConfig(const AppConfig& config) {
    AudioServiceWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, config] { worker->applyConfig(config); },
            Qt::QueuedConnection);
}

void AudioService::setRole(Role role) {
    AudioServiceWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, role] { worker->setRole(role); },
            Qt::QueuedConnection);
}

void AudioService::start() {
    QMetaObject::invokeMethod(worker_, "start", Qt::QueuedConnection);
}

void AudioService::stop() {
    QMetaObject::invokeMethod(worker_, "stop", Qt::QueuedConnection);
}

void AudioService::setMicrophoneEnabled(bool enabled) {
    AudioServiceWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, enabled] { worker->setMicrophoneEnabled(enabled); },
            Qt::QueuedConnection);
}

void AudioService::setDesktopAudioEnabled(bool enabled) {
    AudioServiceWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, enabled] { worker->setDesktopAudioEnabled(enabled); },
            Qt::QueuedConnection);
}

void AudioService::setPlaybackEnabled(bool enabled) {
    AudioServiceWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, enabled] { worker->setPlaybackEnabled(enabled); },
            Qt::QueuedConnection);
}
