//
// Created by ASUS on 2026/8/1.
//

#include "P2pSession.h"
#include "UdpControlPayload.h"
#include "UdpControlPayloadCodec.h"

P2pSession::P2pSession(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<QHostAddress>("QHostAddress");
    qRegisterMetaType<UdpFrame>("UdpFrame");
    qRegisterMetaType<UdpFrameType>("UdpFrameType");
    qRegisterMetaType<SignalingMessage>("SignalingMessage");

    thread_ = new QThread(this);
    worker_ = new P2pSessionWorker;
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    connect(worker_, &P2pSessionWorker::p2pReady,
            this, &P2pSession::p2pReady);
    connect(worker_, &P2pSessionWorker::mediaFrameReceived,
            this, &P2pSession::mediaFrameReceived);
    connect(worker_, &P2pSessionWorker::errorOccurred,
            this, &P2pSession::errorOccurred);
    connect(worker_, &P2pSessionWorker::logReceived,
            this, &P2pSession::logReceived);

    thread_->start();
}

P2pSession::~P2pSession() {
    thread_->quit();
    thread_->wait();
    worker_ = nullptr;
}

void P2pSession::applyConfig(const P2pConfig &config) {
    P2pSessionWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, config] {
                worker->applyConfig(config);
            },
            Qt::QueuedConnection
            );
}

bool P2pSession::bind(quint16 localPort) {
    bool result = false;
    P2pSessionWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, localPort, &result] {
                result = worker->bind(localPort);
            },
            Qt::BlockingQueuedConnection
            );
    return result;
}

void P2pSession::setClientInfo(const QString &roomId, const QString &clientId) {
    P2pSessionWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, roomId, clientId] {
                worker->setClientInfo(roomId, clientId);
            },
            Qt::QueuedConnection
            );
}

void P2pSession::setServerUdpEndpoint(const QHostAddress &address, quint16 port) {
    P2pSessionWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, address, port] {
                worker->setServerUdpEndpoint(address, port);
            },
            Qt::QueuedConnection
            );
}

void P2pSession::setPunchPortRange(quint16 range) {
    P2pSessionWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, range] {
                worker->setPunchPortRange(range);
            },
            Qt::QueuedConnection
            );
}

P2pSessionWorker* P2pSession::worker() const {
    return worker_;
}

void P2pSession::onProbePermitted(const SignalingMessage &message) {
    P2pSessionWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, message] {
                worker->onProbePermitted(message);
            },
            Qt::QueuedConnection
            );
}

void P2pSession::onPeerEndpoint(const SignalingMessage &message) {
    P2pSessionWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, message] {
                worker->onPeerEndpoint(message);
            },
            Qt::QueuedConnection
            );
}

void P2pSession::sendMediaFrame(UdpFrameType type, const QByteArray &payload) {
    P2pSessionWorker* worker = worker_;

    QMetaObject::invokeMethod(
            worker,
            [worker, type, payload] {
                worker->sendMediaFrame(type, payload);
            },
            Qt::QueuedConnection
    );
}