//
// Created by ASUS on 2026/8/25.
//

#include "ControlServiceWorker.h"

ControlServiceWorker::ControlServiceWorker(QObject* parent)
        : QObject(parent) {
}

void ControlServiceWorker::setRole(Role role) {
    role_ = role;
}

Role ControlServiceWorker::role() const {
    return role_;
}

bool ControlServiceWorker::isRunning() const {
    return running_;
}

void ControlServiceWorker::start() {
    if (role_ == Role::Unknown) {
        emit errorOccurred("control role is unknown");
        return;
    }

    running_ = true;
    emit logReceived("control service started");
}

void ControlServiceWorker::stop() {
    running_ = false;
    emit logReceived("control service stopped");
}

void ControlServiceWorker::onP2pReady(
        const QHostAddress& address,
        quint16 port) {
    Q_UNUSED(address)
    Q_UNUSED(port)

    start();
}

void ControlServiceWorker::onControlFrameReceived(
        const UdpFrame& frame) {
    if (!running_) {
        return;
    }

    if (frame.channelType != UdpChannelType::Control) {
        return;
    }

    switch (frame.frameType) {
        case UdpFrameType::InputEvent:
            emit inputEventReceived(frame.payload);
            break;

        case UdpFrameType::KeyFrameRequest:
            qDebug() << "[关键帧][host] 收到关键帧请求";
            emit keyFrameRequestReceived(frame.payload);
            break;

        case UdpFrameType::ReceiverReport:
            emit receiverReportReceived(frame.payload);
            break;

        default:
            break;
    }
}

bool ControlServiceWorker::sendInputEvent(
        const QByteArray& payload) {
    if (!running_) {
        emit errorOccurred(
                "control service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::InputEvent)) {
        emit errorOccurred(
                "current role cannot send input event");
        return false;
    }

    emit controlFrameToSend(
            UdpFrameType::InputEvent,
            payload);

    return true;
}

bool ControlServiceWorker::sendKeyFrameRequest() {
    if (!running_) {
        emit errorOccurred(
                "control service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::KeyFrameRequest)) {
        emit errorOccurred(
                "current role cannot send key frame request");
        return false;
    }

    // 当前协议要求帧payload非空
    const QByteArray payload(1, 0);

    emit controlFrameToSend(
            UdpFrameType::KeyFrameRequest,
            payload);

    return true;
}

bool ControlServiceWorker::sendReceiverReport(
        const QByteArray& payload) {
    if (!running_) {
        emit errorOccurred(
                "control service is not running");
        return false;
    }

    if (!canSend(UdpFrameType::ReceiverReport)) {
        emit errorOccurred(
                "current role cannot send receiver report");
        return false;
    }

    emit controlFrameToSend(
            UdpFrameType::ReceiverReport,
            payload);

    return true;
}

bool ControlServiceWorker::canSend(
        UdpFrameType type) const {
    if (role_ == Role::Host) {
        return type == UdpFrameType::InputEvent
               || type == UdpFrameType::ReceiverReport;
    }

    if (role_ == Role::Guest) {
        return type == UdpFrameType::InputEvent
               || type == UdpFrameType::KeyFrameRequest
               || type == UdpFrameType::ReceiverReport;
    }

    return false;
}