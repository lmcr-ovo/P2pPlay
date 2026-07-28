#ifndef P2PPLAY_TCPFRAME_H
#define P2PPLAY_TCPFRAME_H

#include <QtCore>

enum class SignalingType : quint16 {
    Unknown = 0,
    CreateRoom = 1,
    RoomCreated = 2,
    JoinRoom = 3,
    JoinResult = 4,
    PeerJoined = 5,
    UdpEndPointReady = 6,
    PeerEndPoint = 7,
    P2pResultReport = 8,
    Error = 9
};

struct TcpFrame {
    SignalingType type = SignalingType::Unknown;
    QByteArray payload;
};

#endif //P2PPLAY_TCPFRAME_H