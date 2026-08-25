//
// Created by ASUS on 2026/8/10.
//

#include "MediaServiceWorker.h"
#include "Video/VideoSampleCodec.h"
#include "TraceManager.h"

MediaServiceWorker::MediaServiceWorker(QObject* parent)
    : QObject(parent) {
}

void MediaServiceWorker::setRole(Role role) {
    role_ = role;
}

Role MediaServiceWorker::role() const {
    return role_;
}

void MediaServiceWorker::start() {
    if (role_ == Role::Unknown) {
        emit errorOccurred("media role is unknown");
        return;
    }

    running_ = true;
    emit logReceived("media service started");
}

void MediaServiceWorker::stop() {
    running_ = false;
    emit logReceived("media service stopped");
}

bool MediaServiceWorker::isRunning() const {
    return running_;
}

void MediaServiceWorker::onP2pReady(const QHostAddress& address, quint16 port) {
    Q_UNUSED(address)
    Q_UNUSED(port)

    start();
}

void MediaServiceWorker::onUdpMediaFrameReceived(const UdpFrame& frame) {
    if (!running_) {
        return;
    }

    if (frame.channelType != UdpChannelType::Media) {
        return;
    }

    switch (frame.frameType) {
        case UdpFrameType::VideoFrame:
        {
            quint32 sampleSeq = 0;
            if (VideoSampleCodec::peekVideoSeq(frame.payload, sampleSeq)) {
                TraceManager::instance().record(sampleSeq,
                                                TraceStage::ReassembleEnd,
                                                TraceManager::nowUs());
            }
        }
            emit videoSampleBytesReceived(frame.payload);
            break;

        case UdpFrameType::AudioFrame:
            emit audioSampleBytesReceived(frame.payload);
            break;
        default:
            break;
    }
}


bool MediaServiceWorker::sendVideoSampleBytes(const QByteArray& payload) {
    if (!running_) {
        emit errorOccurred("media service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::VideoFrame)) {
        emit errorOccurred("current role cannot send video frame");
        return false;
    }

    quint32 sampleId = 0;
    if (VideoSampleCodec::peekVideoSeq(payload, sampleId)) {
        TraceManager::instance().record(sampleId,
                                        TraceStage::SendEnd,
                                        TraceManager::nowUs());
    }

    emit udpMediaFrameToSend(UdpFrameType::VideoFrame, payload);
    return true;
}

bool MediaServiceWorker::sendAudioSampleBytes(const QByteArray& sampleBytes) {
    if (!running_) {
        emit errorOccurred("media service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::AudioFrame)) {
        emit errorOccurred("current role cannot send audio frame");
        return false;
    }

    emit udpMediaFrameToSend(UdpFrameType::AudioFrame, sampleBytes);
    return true;
}


bool MediaServiceWorker::canSend(
        UdpFrameType type) const {
    if (role_ == Role::Host) {
        return type == UdpFrameType::VideoFrame
               || type == UdpFrameType::AudioFrame;
    }

    if (role_ == Role::Guest) {
        return type == UdpFrameType::VideoFrame
               || type == UdpFrameType::AudioFrame;
    }

    return false;
}
