#include "MediaService.h"

#include "VideoSampleCodec.h"
#include "TraceManager.h"

MediaService::MediaService(QObject* parent)
        : QObject(parent) {
}

void MediaService::setRole(MediaRole role) {
    role_ = role;
}

MediaRole MediaService::role() const {
    return role_;
}

void MediaService::start() {
    if (role_ == MediaRole::Unknown) {
        emit errorOccurred("media role is unknown");
        return;
    }

    running_ = true;
    emit logReceived("media service started");
}

void MediaService::stop() {
    running_ = false;
    emit logReceived("media service stopped");
}

bool MediaService::isRunning() const {
    return running_;
}

void MediaService::onP2pReady(const QHostAddress& address, quint16 port) {
    Q_UNUSED(address)
    Q_UNUSED(port)

    start();
}

void MediaService::onUdpMediaFrameReceived(const UdpFrame& frame) {
    if (!running_) {
        return;
    }

    if (frame.channelType != UdpChannelType::Media) {
        return;
    }

    switch (frame.frameType) {
        case UdpFrameType::VideoFrame:
            {
                VideoSample sample;
                if (VideoSampleCodec::decode(frame.payload, sample)) {
                    TraceManager::instance().record(sample.videoSeq,
                                                    TraceStage::ReassembleEnd,
                                                    TraceManager::nowUs());
                }
            }
            emit videoSampleBytesReceived(frame.payload);
            break;

        case UdpFrameType::AudioFrame:
            emit audioSampleBytesReceived(frame.payload);
            break;

        case UdpFrameType::InputEvent:
            emit inputCommandReceived(frame.payload);
            break;

        case UdpFrameType::KeyFrameRequest:
            emit keyFrameRequestReceived(frame.payload);
            break;

        default:
            break;
    }
}

bool MediaService::sendVideoSampleBytes(const QByteArray& payload) {
    if (!running_) {
        emit errorOccurred("media service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::VideoFrame)) {
        emit errorOccurred("current role cannot send video frame");
        return false;
    }

    VideoSample sample;
    emit udpMediaFrameToSend(UdpFrameType::VideoFrame, payload);

    if (VideoSampleCodec::decode(payload, sample)) {
        TraceManager::instance().record(sample.videoSeq,
                                        TraceStage::SendEnd,
                                        TraceManager::nowUs());
    }
    return true;
}

bool MediaService::sendAudioSampleBytes(const QByteArray& sampleBytes) {
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

bool MediaService::sendInputCommand(const QByteArray& commandBytes) {
    if (!running_) {
        emit errorOccurred("media service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::InputEvent)) {
        emit errorOccurred("current role cannot send input event");
        return false;
    }

    emit udpMediaFrameToSend(UdpFrameType::InputEvent, commandBytes);
    return true;
}

bool MediaService::sendKeyFrameRequest(const QByteArray& payload) {
    if (!running_) {
        emit errorOccurred("media service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::KeyFrameRequest)) {
        emit errorOccurred("current role cannot send key frame request");
        return false;
    }

    emit udpMediaFrameToSend(UdpFrameType::KeyFrameRequest, payload);
    return true;
}

bool MediaService::canSend(UdpFrameType type) const {
    if (role_ == MediaRole::Host) {
        return type == UdpFrameType::VideoFrame
               || type == UdpFrameType::AudioFrame;
    }

    if (role_ == MediaRole::Guest) {
        return type == UdpFrameType::InputEvent
               || type == UdpFrameType::KeyFrameRequest;
    }

    return false;
}
