//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_SIGNALINGDISPATCHER_H
#define P2PPLAY_SIGNALINGDISPATCHER_H

#include <QObject>
#include "SignalingConnection.h"

class SignalingDispatcher : QObject {
    Q_OBJECT

signals:
    void registerRequest(SignalingConnection* connection,
            const SignalingMessage& message);

    void createRoomRequest(SignalingConnection* connection,
            const SignalingMessage& message);

    void joinRoomRequest(SignalingConnection* connection,
            const SignalingMessage& message);

    void probeRequest(SignalingConnection* connection,
                         const SignalingMessage& message);

public slots:
    void onMessageReceived(SignalingConnection* connection,
            const SignalingMessage& message);
};


#endif //P2PPLAY_SIGNALINGDISPATCHER_H
