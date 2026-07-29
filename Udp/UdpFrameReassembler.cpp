//
// Created by ASUS on 2026/7/27.
//

#include "UdpFrameReassembler.h"

UdpFrameReassembler::UdpFrameReassembler(QObject* parent)
    : QObject(parent) {}

void UdpFrameReassembler::pushPacket(const UdpPacket &packet) {
    if (packet.fragmentCount == 0) {
        return;
    }

    if (packet.fragmentSeq >= packet.fragmentCount) {
        return;
    }

    FrameBuffer &buffer = pendingFrame_[packet.frameSeq];
    if (buffer.fragments.isEmpty()) {
        buffer.channelType = packet.channel;
        buffer.frameType = packet.type;
        buffer.fragmentCount = packet.fragmentCount;
    }

    if (buffer.fragmentCount != packet.fragmentCount) {
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
    pendingFrame_.remove(packet.frameSeq);
    dropFramesOlderThan(packet.frameSeq);
}

void UdpFrameReassembler::emitCompleteFrame(const FrameBuffer& buffer) {
    QByteArray payload;
    for (auto it = buffer.fragments.cbegin(); it != buffer.fragments.cend(); ++it) {
        payload.append(it.value());
    }
    UdpFrame frame;
    frame.channelType = buffer.channelType;
    frame.frameType = buffer.frameType;
    frame.payload = payload;

    emit frameReady(frame);
}

void UdpFrameReassembler::dropFramesOlderThan(quint32 frameSeq) {
    auto it = pendingFrame_.begin();

    while (it != pendingFrame_.end() && it.key() < frameSeq) {
        const quint32 droppedSeq = it.key();
        it = pendingFrame_.erase(it);
        emit frameDropped(droppedSeq);
    }
}