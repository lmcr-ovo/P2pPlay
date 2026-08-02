//
// Created by ASUS on 2026/8/1.
//

#include "HostRoleService.h"

HostRoleService::HostRoleService(QObject* parent)
    : QObject(parent) {}

void HostRoleService::setClientId(const QString& clientId) {
    clientId_ = clientId;
}

QString HostRoleService::clientId() const {
    return clientId_;
}

QString HostRoleService::roomId() const {
    return roomId_;
}

void HostRoleService::onConnected() {
    if (clientId_.isEmpty()) {
        emit errorOccurred("host client id is empty");
        return;
    }

    sendRegister();
    sendCreateRoom();
}

void HostRoleService::onRoomCreated(const SignalingMessage& message) {
    if (!message.success) {
        emit errorOccurred(message.reason);
        return;
    }

    if (message.roomId.isEmpty()) {
        emit errorOccurred("created room id is empty");
        return;
    }

    roomId_ = message.roomId;
    emit traversalContextReady(roomId_, clientId_);
    emit logReceived(QString("room created: %1").arg(roomId_));
}

void HostRoleService::onPeerJoined(const SignalingMessage& message) {
    if (!message.success) {
        emit errorOccurred(message.reason);
        return;
    }

    if (!message.roomId.isEmpty() && !roomId_.isEmpty()
        && message.roomId != roomId_) {
        return;
    }

    emit logReceived(QString("peer joined: %1").arg(message.clientId));
    sendProbeRequest();
}

void HostRoleService::onP2pReady(const QHostAddress& address, quint16 port) {
    emit logReceived(QString("p2p ready: %1:%2")
                             .arg(address.toString())
                             .arg(port));
}

void HostRoleService::sendRegister() {
    SignalingMessage message;
    message.type = SignalingType::Register;
    message.clientId = clientId_;

    emit sendMessage(message);
}

void HostRoleService::sendCreateRoom() {
    SignalingMessage message;
    message.type = SignalingType::CreateRoom;
    message.clientId = clientId_;

    emit sendMessage(message);
}

void HostRoleService::sendProbeRequest() {
    if (roomId_.isEmpty()) {
        emit errorOccurred("room id is empty when sending probe request");
        return;
    }

    SignalingMessage message;
    message.type = SignalingType::ProbeRequest;
    message.roomId = roomId_;
    message.clientId = clientId_;

    emit sendMessage(message);
}
