//
// Created by ASUS on 2026/7/29.
//

#include "UdpPacketQueue.h"
#include "UdpPacketCodec.h"

UdpPacketQueue::UdpPacketQueue(QObject* parent, QUdpSocket* sock)
    : QObject(parent), sock_(sock), timer_(this) {
    connect(&timer_, &QTimer::timeout, this, &UdpPacketQueue::sendPacketPerTick);
}

void UdpPacketQueue::setTick(int packetsPerTick, int flushIntervalMs) {
    packetsPerTick_ = packetsPerTick;
    flushIntervalMs_ = flushIntervalMs;
}

void UdpPacketQueue::onPacketsReadyToSend(QQueue<UdpPacket> packets,
        const QHostAddress& peerAddress, quint16 PeerPort) {
    //if (!packets_.isEmpty()) return;
    while (!packets.isEmpty()) {
        PendingUdpPacket pending;
        pending.packet = packets.dequeue();
        pending.address = peerAddress;
        pending.port = PeerPort;
        packets_.enqueue(pending);
    }

    if (!timer_.isActive()) {
        timer_.start(flushIntervalMs_);
    }
}

void UdpPacketQueue::sendPacketPerTick() {
    int sentCount = 0;

    while (!packets_.isEmpty() && sentCount < packetsPerTick_) {
        const PendingUdpPacket pending = packets_.dequeue();
        const QByteArray bytes = UdpPacketCodec::encode(pending.packet);

        const qint64 written = sock_->writeDatagram(
                bytes, pending.address, pending.port);

        if (written != bytes.size()) {
            // 后面建议 emit writeFailed(sock_->errorString());
            continue;
        }

        ++sentCount;
    }

    if (packets_.isEmpty()) {
        timer_.stop();
        return;
    }
}