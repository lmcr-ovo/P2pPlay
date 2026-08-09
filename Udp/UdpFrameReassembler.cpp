//
// Created by ASUS on 2026/7/27.
//

#include "UdpFrameReassembler.h"

UdpFrameReassembler::UdpFrameReassembler(QObject* parent)
    : QObject(parent) {}

void UdpFrameReassembler::pushPacket(const UdpPacket& packet,
        const QHostAddress& senderAddress,
        quint16 senderPort) {
    if (packet.fragmentCount == 0) {
        return;
    }

    if (packet.fragmentSeq >= packet.fragmentCount) {
        return;
    }

    PeerKey key = PeerKey::makePeerKey(senderAddress, senderPort);
    if (!peers_.contains(key)) {
        PeerState state(key);
        peers_.insert(key, state);
    }

    auto it = peers_.find(key);
    if (it == peers_.end()) {
        it = peers_.insert(key, PeerState(key));
    }
    PeerState& peerState = it.value();

    // 如果packet是最新的
    if (!peerState.hasLatestSeenSeq || packet.frameSeq > peerState.latestSeenSeq) {
        peerState.latestSeenSeq = packet.frameSeq;
        peerState.hasLatestSeenSeq = true;
    }

    // 当前packet太旧直接丢弃
    if (peerState.hasLatestSeenSeq
        && peerState.latestSeenSeq > PeerState::MaxFrameLag
        && packet.frameSeq < peerState.latestSeenSeq - PeerState::MaxFrameLag) {
        return;
    }

    // 清理旧帧
    const quint32 minAllowedSeq = peerState.latestSeenSeq > PeerState::MaxFrameLag
                                  ? peerState.latestSeenSeq - PeerState::MaxFrameLag
                                  : 0;

    while (!peerState.pendingFrame_.isEmpty()) {
        auto it = peerState.pendingFrame_.begin();

        if (it.key() >= minAllowedSeq) {
            break;
        }

        peerState.pendingFrame_.erase(it);
    }

    // 数量上限
    while (peerState.pendingFrame_.size() > PeerState::MaxPendingFramesPerPeer) {
        auto it = peerState.pendingFrame_.begin();
        peerState.pendingFrame_.erase(it);
    }

    // 没有收过该帧
    if (!peerState.pendingFrame_.contains(packet.frameSeq)) {
        FrameBuffer buffer;
        buffer.channelType = packet.channel;
        buffer.frameType = packet.type;
        buffer.senderAddress = senderAddress;
        buffer.senderPort = senderPort;
        buffer.frameSeq = packet.frameSeq;
        buffer.fragmentCount = packet.fragmentCount;
        buffer.payload.resize(buffer.fragmentCount
        * static_cast<int>(UdpPacket::MaxPayloadSize));
        buffer.received.resize(packet.fragmentCount);
        peerState.pendingFrame_.insert(packet.frameSeq, buffer);
    }

    FrameBuffer& buffer = peerState.pendingFrame_[packet.frameSeq];

    if (buffer.channelType != packet.channel
        || buffer.frameType != packet.type
        || buffer.fragmentCount != packet.fragmentCount
        || buffer.senderAddress != senderAddress
        || buffer.senderPort != senderPort) {
        return;
    }

    if (buffer.received.testBit(packet.fragmentSeq)) {
        return;
    }
    const int offset = packet.fragmentSeq * UdpPacket::MaxPayloadSize;
    memcpy(buffer.payload.data() + offset,
            packet.payload.constData(),
            packet.payload.size());
    buffer.received.setBit(packet.fragmentSeq);
    buffer.receivedCount += 1;

    // 收到最后一个分片
    if (packet.fragmentSeq == packet.fragmentCount - 1) {
        buffer.totalSize = offset + packet.payload.size();
    }

    // 收齐
    if (buffer.isComplete() && buffer.totalSize > 0) {

        buffer.payload.resize(buffer.totalSize);

        UdpFrame frame;
        frame.senderAddress = buffer.senderAddress;
        frame.senderPort = buffer.senderPort;
        frame.channelType = buffer.channelType;
        frame.frameType = buffer.frameType;
        frame.payload = buffer.payload;

        emit frameReady(frame);
        peerState.pendingFrame_.remove(packet.frameSeq);
    }
}

