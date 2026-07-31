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

    FrameKey key;
    key.senderAddress = senderAddress;
    key.senderPort = senderPort;
    key.frameSeq = packet.frameSeq;

    FrameBuffer &buffer = pendingFrame_[key];
    if (buffer.fragments.isEmpty()) {
        buffer.channelType = packet.channel;
        buffer.frameType = packet.type;
        buffer.senderAddress = senderAddress;
        buffer.senderPort = senderPort;
        buffer.fragmentCount = packet.fragmentCount;
    }

    if (buffer.channelType != packet.channel
        || buffer.frameType != packet.type
        || buffer.fragmentCount != packet.fragmentCount
        || buffer.senderAddress != senderAddress
        || buffer.senderPort != senderPort) {
        return;
    }

    if (buffer.fragments.contains(packet.fragmentSeq)) {
        return;
    }

    buffer.fragments.insert(packet.fragmentSeq, packet.payload);
    if (!buffer.isComplete()) {
        return;
    }

    emitCompleteFrame(buffer);
    pendingFrame_.remove(key);
    dropFramesOlderThan(senderAddress, senderPort, packet.frameSeq);
}

void UdpFrameReassembler::emitCompleteFrame(const FrameBuffer& buffer) {
    QByteArray payload;
    for (auto it = buffer.fragments.cbegin(); it != buffer.fragments.cend(); ++it) {
        payload.append(it.value());
    }
    UdpFrame frame;
    frame.channelType = buffer.channelType;
    frame.frameType = buffer.frameType;
    frame.senderAddress = buffer.senderAddress;
    frame.senderPort = buffer.senderPort;
    frame.payload = payload;

    emit frameReady(frame);
}

void UdpFrameReassembler::dropFramesOlderThan(const QHostAddress& senderAddress,
        quint16 senderPort,
        quint32 frameSeq
        ) {
    auto it = pendingFrame_.begin();

    while (it != pendingFrame_.end()) {
        const FrameKey key = it.key();

        if (key.senderAddress == senderAddress
            && key.senderPort == senderPort
            && key.frameSeq < frameSeq) {
            const quint32 droppedSeq = key.frameSeq;
            it = pendingFrame_.erase(it);
            emit frameDropped(droppedSeq);
            continue;
        }
        ++it;
    }
}