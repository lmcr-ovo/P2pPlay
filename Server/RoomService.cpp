//
// Created by ASUS on 2026/7/30.
//
#include <QRandomGenerator>
#include "RoomService.h"

RoomService::RoomService(QObject* parent) : QObject(parent) {}

void RoomService::onRegister(SignalingConnection* connection,
        const SignalingMessage& message) {
    if (connection == nullptr) {
        return;
    }

    SignalingMessage response;

    if (message.clientId.isEmpty()) {
        response.type = SignalingType::Error;
        response.reason = "用户名不能为空";
        connection->sendMessage(response);
        return;
    }

    if (idTable_.contains(message.clientId)) {
        response.type = SignalingType::Error;
        response.reason = "该用户名已被占用";
        connection->sendMessage(response);
        return;
    }

    idTable_.insert(message.clientId);
    clientIdByConnection_.insert(connection, message.clientId);

    response.type = SignalingType::Log;
    response.reason = "注册成功";
    connection->sendMessage(response);
}

void RoomService::onCreateRoom(SignalingConnection* connection,
        const SignalingMessage& message) {
    const QString clientId = clientIdByConnection_.value(connection);
    SignalingMessage response;
    response.type = SignalingType::RoomCreated;
    response.clientId = clientId;

    if (roomIdByClientId_.contains(clientId)) {
        response.success = false;
        response.reason = "已进入房间";
        connection->sendMessage(response);
        return;
    }

    const QString roomId = generateRoomId();
    Room room;
    room.host.clientId = clientId;
    room.host.connection = connection;

    roomsById_.insert(roomId, room);
    roomIdByClientId_.insert(clientId, roomId);

    response.success = true;
    response.roomId = roomId;
    connection->sendMessage(response);
}

QString RoomService::generateRoomId() const {
    QString roomId;

    do {
        roomId = QString::number(
                QRandomGenerator::global()->bounded(100000, 1000000)
        );
    } while (roomsById_.contains(roomId));

    return roomId;
}

void RoomService::onJoinRoom(SignalingConnection* connection,
                             const SignalingMessage& message) {
    if (connection == nullptr) {
        return;
    }

    SignalingMessage response;

    const QString clientId = clientIdByConnection_.value(connection);
    if (clientId.isEmpty()) {
        response.type = SignalingType::Error;
        response.reason = "请先注册";
        connection->sendMessage(response);
        return;
    }

    if (message.roomId.isEmpty()) {
        response.type = SignalingType::Error;
        response.reason = "房间号不能为空";
        connection->sendMessage(response);
        return;
    }

    auto roomIt = roomsById_.find(message.roomId);
    if (roomIt == roomsById_.end()) {
        response.type = SignalingType::Error;
        response.reason = "房间不存在";
        connection->sendMessage(response);
        return;
    }

    Room& room = roomIt.value();

    if (room.host.connection == connection || room.host.clientId == clientId) {
        response.type = SignalingType::Error;
        response.reason = "不能加入自己创建的房间";
        connection->sendMessage(response);
        return;
    }

    if (roomIdByClientId_.contains(clientId)) {
        response.type = SignalingType::Error;
        response.reason = "已进入房间";
        connection->sendMessage(response);
        return;
    }

    if (room.hasClient()) {
        response.type = SignalingType::Error;
        response.reason = "房间已满";
        connection->sendMessage(response);
        return;
    }

    room.client.clientId = clientId;
    room.client.connection = connection;
    roomIdByClientId_.insert(clientId, room.roomId);

    response.type = SignalingType::Log;
    response.roomId = room.roomId;
    response.clientId = clientId;
    response.success = true;
    response.reason = "加入房间成功";
    connection->sendMessage(response);

    if (room.host.connection != nullptr) {
        SignalingMessage notify;
        notify.type = SignalingType::PeerJoined;
        notify.roomId = room.roomId;
        notify.clientId = clientId;
        notify.success = true;
        notify.reason = "对端已加入房间";

        room.host.connection->sendMessage(notify);
    }
}

void RoomService::onProbeRequest(SignalingConnection* connection,
        const SignalingMessage& message) {

}