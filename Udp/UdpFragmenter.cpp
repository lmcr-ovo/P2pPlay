//
// Created by ASUS on 2026/7/28.
//
#include <limits>
#include "UdpFragmenter.h"

QList<UdpPacket> UdpFragmenter::fragment(const UdpFrame& frame, quint32 frameSeq) {
    QList<UdpPacket> packets;

    int maxPayloadSizeInt = UdpPacket::MaxPayloadSize;
    int totalSize = frame.payload.size();
    // 分片序号从为[0, maxFragmentSeq]
    int fragmentCount = qMax(
            1,
            (totalSize + maxPayloadSizeInt - 1) / maxPayloadSizeInt);
    if (fragmentCount > std::numeric_limits<quint16>::max()) {
        return packets;
    }
    packets.reserve(fragmentCount);
    for (int fragmentSeq = 0; fragmentSeq < fragmentCount; ++fragmentSeq) {
        int offset = fragmentSeq * maxPayloadSizeInt;
        int size = qMin(maxPayloadSizeInt, totalSize - offset);

        UdpPacket packet;
        packet.magic = UdpPacket::Magic;
        packet.version = UdpPacket::Version;
        packet.channel = frame.channelType;
        packet.type = frame.frameType;
        packet.frameSeq = frameSeq;
        packet.fragmentSeq = static_cast<quint16>(fragmentSeq);
        packet.fragmentCount = static_cast<quint16 >(fragmentCount);
        packet.payload = size > 0 ? frame.payload.mid(offset, size) : QByteArray();

        packets.append(packet);
    }
    return packets;
}