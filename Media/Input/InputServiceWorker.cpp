//
// Created by ASUS on 2026/8/24.
//

#include "InputServiceWorker.h"

InputServiceWorker::InputServiceWorker(QObject* parent)
    : capture_(this),
    sender_(this),
    receiver_(this) {
}

InputServiceWorker::~InputServiceWorker() {
    clearRoleConnections();
}

void InputServiceWorker::setRole(const Role& role) {
    role_ = role;
    clearRoleConnections();
    connectRoleSignals();
}

void InputServiceWorker::connectRoleSignals() {
    if (role_ == Role::Host) {
        // 接收方回复Ack
        roleConnections_.append(connect(
                &receiver_, &InputReceiver::inputAckSampleBytesReady,
                this, &InputServiceWorker::inputAckSampleBytesReady));
    } else {
        // 接收capture发出的inputRawSample
        roleConnections_.append(connect(
                &capture_, &InputCapture::inputRawSampleReady,
                &sender_, &InputSender::onInputRawSampleReady
                ));
        // 接收sender发出的inputSampleBytesReady
        roleConnections_.append(connect(
                &sender_, &InputSender::inputSampleBytesReady,
                this, &InputServiceWorker::inputSampleBytesReady
                ));
    }
}

void InputServiceWorker::clearRoleConnections() {
    for (const auto& conn : roleConnections_) {
        disconnect(conn);
    }
    roleConnections_.clear();
}

void InputServiceWorker::start() {
    if (role_ == Role::Guest) {
        capture_.start();
    }
}

void InputServiceWorker::setControlActive(bool active) {
    capture_.setControlActive(active);
}

void InputServiceWorker::onInputSampleBytesReceived(const QByteArray &bytes) {
    if (role_ == Role::Host) {
        receiver_.onInputSampleBytesReady(bytes);
    }

    // guest只收Ack
    if (role_ == Role::Guest) {
        sender_.onInputAckSampleBytesReady(bytes);
    }
}