#ifndef P2PPLAY_SIGNALINGMESSAGE_H
#define P2PPLAY_SIGNALINGMESSAGE_H

#include <QHostAddress>
#include "TcpFrame.h"

struct SignalingMessage {
    SignalingType type = SignalingType::Unknown;
    QString roomId;
    QString clientId;
    QString reason;
    QHostAddress endpointAddress;
    quint16 endpointPort = 0;
    bool success = false;
};

Q_DECLARE_METATYPE(SignalingMessage)

#endif //P2PPLAY_SIGNALINGMESSAGE_H