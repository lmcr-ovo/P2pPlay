#ifndef P2PPLAY_TCPFRAME_H
#define P2PPLAY_TCPFRAME_H

#include <QtCore>

enum class SignalingType : quint16 {
    Unknown = 0,
    Register = 1,
    CreateRoom = 2,
    RoomCreated = 3,
    JoinRoom = 4,
    PeerJoined = 6,
    UdpEndpointReady = 7,
    PeerEndpoint = 8,
    P2pResultReport = 9,
    Log = 12,
    ProbeRequest = 13,
    ProbePermitted = 14,
    Error = 10
};

struct TcpFrame {
    SignalingType type = SignalingType::Unknown;
    QByteArray payload;
};

#endif //P2PPLAY_TCPFRAME_H