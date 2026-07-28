//
// Created by ASUS on 2026/7/27.
//

#include "UdpPacketCodec.h"

QByteArray UdpPacketCodec::encode(UdpPacket& packet) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << packet.magic;
    stream << packet.version;
    stream << packet.channel;
    stream << packet.type;
    stream << packet.frameSeq;
    stream << packet.fragmentSeq;
    stream << packet.fragmentCount;

    bytes.append(packet.payload);
    return bytes;
}

bool UdpPacketCodec::decode(const QByteArray& bytes, UdpPacket& packet) {
    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    quint16 version = 0;
    quint16 channel = 0;
    quint16 type = 0;
    quint32 frameSeq = 0;
    quint16 fragmentSeq = 0;
    quint16 fragmentCount = 0;
    QByteArray payload;

    stream >> magic
           >> version
           >> channel
           >> type
           >> frameSeq
           >> fragmentSeq
           >> fragmentCount
           >> payload;

    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    packet.magic = magic;
    packet.version = version;
    packet.channel = static_cast<UdpChannelType>(channel);
    packet.type = static_cast<UdpFrameType>(type);
    packet.frameSeq = frameSeq;
    packet.fragmentSeq = fragmentSeq;
    packet.fragmentCount = fragmentCount;
    packet.payload = payload;

    return true;
}