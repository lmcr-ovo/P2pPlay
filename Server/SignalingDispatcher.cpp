//
// Created by ASUS on 2026/7/30.
//

#include "SignalingDispatcher.h"

void SignalingDispatcher::onMessageReceived(SignalingConnection* connection,
        const SignalingMessage& message) {
    switch (message.type) {
        case SignalingType::Register : {
            emit registerRequest(connection, message);
            break;
        }
        case SignalingType::CreateRoom : {
            emit createRoomRequest(connection, message);
            break;
        }
        case SignalingType::JoinRoom : {
            emit joinRoomRequest(connection, message);
            break;
        }
        case SignalingType::ProbeRequest : {
            emit probeRequest(connection, message);
            break;
        }
    }
}