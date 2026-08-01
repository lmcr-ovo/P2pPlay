#ifndef P2PPLAY_UDPPACKET_H
#define P2PPLAY_UDPPACKET_H

#include <QtCore>

enum class UdpChannelType : quint16 {
    Unknown = 0,
    Control = 1,
    Media = 2
};

enum class UdpFrameType : quint16 {
    Unknown = 0,
    // Control
    Probe = 1,
    ProbeAck = 2,
    Punch = 3,
    PunchAck = 4,

    // Media
    VideoFrame = 5,
    AudioFrame = 6,
    InputEvent = 7,
    KeyFrameRequest = 8
};

struct UdpPacket {
    static constexpr quint32 Magic = 0x50325031;
    static constexpr quint16 Version = 1;

    static constexpr int MaxDatagramSize = 1200;

    static constexpr int FixedHeaderSize =
            sizeof(quint32) + // magic
            sizeof(quint16) + // version
            sizeof(UdpChannelType) + // channel
            sizeof(UdpFrameType) + // type
            sizeof(quint32) + // frameSeq
            sizeof(quint16) + // fragmentSeq
            sizeof(quint16); // fragmentCount

    static constexpr int MaxPayloadSize = MaxDatagramSize - FixedHeaderSize;

    quint32 magic = Magic;
    quint16 version = Version;

    UdpChannelType channel = UdpChannelType::Unknown;
    UdpFrameType type = UdpFrameType::Unknown;

    quint32 frameSeq = 0;
    quint16 fragmentSeq = 0;
    quint16 fragmentCount = 0;

    QByteArray payload;
};

struct UdpFrame {
    UdpChannelType channelType = UdpChannelType::Unknown;
    UdpFrameType frameType = UdpFrameType::Unknown;
    QHostAddress senderAddress;
    quint16 senderPort = 0;

    QByteArray payload;
};

#endif //P2PPLAY_UDPPACKET_H