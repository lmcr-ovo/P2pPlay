//
// Created by ASUS on 2026/7/27.
//

#include "UdpPacketCodec.h"

QByteArray UdpPacketCodec::encode(const UdpPacket& packet) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << packet.magic;
    stream << packet.version;
    stream << static_cast<quint16>(packet.channel);
    stream << static_cast<quint16>(packet.type);
    stream << packet.frameSeq;
    stream << packet.fragmentSeq;
    stream << packet.fragmentCount;

    bytes.append(packet.payload);
    return bytes;
}

bool UdpPacketCodec::decode(const QByteArray& bytes, UdpPacket& packet) {
    if (bytes.size() < UdpPacket::FixedHeaderSize) {
        return false;
    }

    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    quint16 version = 0;
    quint16 channel = 0;
    quint16 type = 0;
    quint32 frameSeq = 0;
    quint16 fragmentSeq = 0;
    quint16 fragmentCount = 0;

    stream >> magic
           >> version
           >> channel
           >> type
           >> frameSeq
           >> fragmentSeq
           >> fragmentCount;

    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    if (magic != UdpPacket::Magic || version != UdpPacket::Version) {
        return false;
    }

    packet.magic = magic;
    packet.version = version;
    packet.channel = static_cast<UdpChannelType>(channel);
    packet.type = static_cast<UdpFrameType>(type);
    packet.frameSeq = frameSeq;
    packet.fragmentSeq = fragmentSeq;
    packet.fragmentCount = fragmentCount;
    packet.payload = bytes.mid(UdpPacket::FixedHeaderSize);
    return true;
}