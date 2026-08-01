//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_SERVERAPP_H
#define P2PPLAY_SERVERAPP_H

#include <QObject>
#include "SignalingServer.h"
#include "SignalingDispatcher.h"
#include "RoomService.h"
#include "NatProbeService.h"

class ServerApp : public QObject {
    Q_OBJECT
public:
    explicit ServerApp();


private:
    RoomService roomService_;
    SignalingServer signalingServer_;
    SignalingDispatcher signalingDispatcher_;
    NatProbeService natProbeService_;
};


#endif //P2PPLAY_SERVERAPP_H
