//
// Created by ASUS on 2026/7/28.
//

#include "UdpFragmenter.h"

QList<UdpPacket> UdpFragmenter::fragment(const UdpFrame& frame, quint32 frameSeq) {
    QList<UdpPacket> packets;

    int maxPayloadSize = UdpPacket::MaxPayloadSize;
    int totalSize = frame.payload.size();
    // 分片序号从为[0, maxFragmentSeq]
    int maxFragmentSeq = static_cast<int>(totalSize / maxPayloadSize);
    packets.reserve(maxFragmentSeq + 1);
    for (int fragmentSeq = 0; fragmentSeq <= maxFragmentSeq; ++fragmentSeq) {
        int offset = fragmentSeq * maxPayloadSize;
        int size = qMin(maxPayloadSize, totalSize - offset);

        UdpPacket packet;
        packet.magic = UdpPacket::Magic;
        packet.version = UdpPacket::Version;
        packet.channel = frame.channelType;
        packet.type = frame.frameType;
        packet.frameSeq = frameSeq;
        packet.fragmentSeq = fragmentSeq;
        packet.fragmentCount = maxFragmentSeq;
        packet.payload = frame.payload.mid(offset, size);

        packets.append(packet);
    }
    return packets;
}