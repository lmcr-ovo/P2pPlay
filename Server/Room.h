//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_ROOM_H
#define P2PPLAY_ROOM_H

#include <QHostAddress>
#include "SignalingConnection.h"

struct UdpEndpoint {
    QHostAddress address;
    quint16 port = 0;

    bool isValid() const {
        return !address.isNull() && port != 0;
    }
};

struct RoomPeer {
    QString clientId;
    SignalingConnection* connection = nullptr;
    UdpEndpoint udpEndpoint;

    bool isValid() const {
        return !clientId.isEmpty() && connection != nullptr;
    };
};


struct Room {
    QString roomId;
    RoomPeer host;
    RoomPeer client;

    bool hasClient() const {
        return client.isValid();
    }

    bool endpointsReady() {
        return host.udpEndpoint.isValid()
            && client.udpEndpoint.isValid();
    }
};


#endif //P2PPLAY_ROOM_H
