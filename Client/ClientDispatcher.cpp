#include "ClientDispatcher.h"

ClientDispatcher::ClientDispatcher(QObject* parent)
        : QObject(parent)
{
}

void ClientDispatcher::onMessageReceived(const SignalingMessage& message)
{
    switch (message.type) {
        case SignalingType::RoomCreated:
            emit roomCreated(message);
            break;

        case SignalingType::PeerJoined:
            emit peerJoined(message);
            break;

        case SignalingType::ProbePermitted:
            emit probePermitted(message);
            break;

        case SignalingType::PeerEndpoint:
            emit peerEndpoint(message);
            break;

        case SignalingType::Log:
            emit logReceived(message);
            break;

        case SignalingType::Error:
            emit errorReceived(message);
            break;
        default:
            emit unknownMessage(message);
            break;
    }
}