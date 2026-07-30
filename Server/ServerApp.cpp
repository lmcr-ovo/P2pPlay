//
// Created by ASUS on 2026/7/30.
//

#include "ServerApp.h"

ServerApp::ServerApp()
    : roomService_(this) {
    connect(&signalingServer_, &SignalingServer::messageReceived,
            &signalingDispatcher_, &SignalingDispatcher::onMessageReceived);
    connect(&signalingDispatcher_, &SignalingDispatcher::registerRequest,
            &roomService_, &RoomService::onRegister);
    connect(&signalingDispatcher_, &SignalingDispatcher::createRoomRequest,
            &roomService_, &RoomService::onCreateRoom);
    connect(&signalingDispatcher_, &SignalingDispatcher::joinRoomRequest,
            &roomService_, &RoomService::onJoinRoom);
    connect(&signalingDispatcher_, &SignalingDispatcher::probeRequest,
            &roomService_, &RoomService::onProbeRequest);
}