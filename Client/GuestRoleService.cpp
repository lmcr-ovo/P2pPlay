//
// Created by ASUS on 2026/8/1.
//

#include "GuestRoleService.h"

GuestRoleService::GuestRoleService(QObject* parent)
    : QObject(parent) {}

void GuestRoleService::setClientInfo(const QString& roomId, const QString& clientId) {
    roomId_ = roomId;
    clientId_ = clientId;
}

QString GuestRoleService::clientId() const {
    return clientId_;
}

QString GuestRoleService::roomId() const {
    return roomId_;
}

void GuestRoleService::onConnected() {
    if (clientId_.isEmpty()) {
        emit errorOccurred("guest client id is empty");
        return;
    }

    if (roomId_.isEmpty()) {
        emit errorOccurred("guest room id is empty");
        return;
    }

    emit traversalContextReady(roomId_, clientId_);

    sendRegister();
    sendJoinRoom();
}

void GuestRoleService::onLogReceived(const SignalingMessage& message) {
    if (!message.reason.isEmpty()) {
        emit logReceived(message.reason);
    }
}

void GuestRoleService::onP2pReady(const QHostAddress& address, quint16 port) {
    emit logReceived(QString("p2p ready: %1:%2")
                             .arg(address.toString())
                             .arg(port));
}

void GuestRoleService::sendRegister() {
    SignalingMessage message;
    message.type = SignalingType::Register;
    message.clientId = clientId_;

    emit sendMessage(message);
}

void GuestRoleService::sendJoinRoom() {
    SignalingMessage message;
    message.type = SignalingType::JoinRoom;
    message.roomId = roomId_;
    message.clientId = clientId_;

    emit sendMessage(message);
}
