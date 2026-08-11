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
    connect(worker_, &MediaServiceWorker::videoSampleBytesReceived,
            this, &MediaService::videoSampleBytesReceived);
    connect(worker_, &MediaServiceWorker::udpMediaFrameToSend,
            this, &MediaService::udpMediaFrameToSend);
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

void MediaService::onP2pReady(const QHostAddress& address, quint16 port) {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, address, port] {
                worker->onP2pReady(address, port);
            },
            Qt::BlockingQueuedConnection
    );
}

void MediaService::onUdpMediaFrameReceived(const UdpFrame& frame) {
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, frame] {
                worker->onUdpMediaFrameReceived(frame);
            },
            Qt::QueuedConnection
    );
}

bool MediaService::sendVideoSampleBytes(const QByteArray& payload) {
    return QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, payload] {
                worker->sendVideoSampleBytes(payload);
            },
            Qt::QueuedConnection
    );
}

bool MediaService::sendAudioSampleBytes(const QByteArray& sampleBytes) {
    return QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, sampleBytes] {
                worker->sendAudioSampleBytes(sampleBytes);
            },
            Qt::QueuedConnection
    );
}

bool MediaService::sendInputCommand(const QByteArray& commandBytes) {
    return QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, commandBytes] {
                worker->sendInputCommand(commandBytes);
            },
            Qt::QueuedConnection
    );
}

bool MediaService::sendKeyFrameRequest(const QByteArray& payload) {
    return QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, payload] {
                worker->sendKeyFrameRequest(payload);
            },
            Qt::QueuedConnection
    );
}
