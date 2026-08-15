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

QQueue<PendingUdpPacket> UdpPacketQueue::buildPendingFrame(
        QQueue<UdpPacket>& packets,
        const QHostAddress& peerAddress,
        quint16 peerPort) const {
    QQueue<PendingUdpPacket> framePackets;
    framePackets.reserve(packets.size());

    while (!packets.isEmpty()) {
        PendingUdpPacket pending;
        pending.packet = packets.dequeue();
        pending.address = peerAddress;
        pending.port = peerPort;
        framePackets.enqueue(pending);
    }

    return framePackets;
}

void UdpPacketQueue::onPacketsReadyToSend(QQueue<UdpPacket> packets,
        const QHostAddress& peerAddress, quint16 PeerPort) {
    QQueue<PendingUdpPacket> framePackets = buildPendingFrame(
            packets,
            peerAddress,
            PeerPort);

    if (framePackets.isEmpty()) {
        return;
    }

    if (currentPackets_.isEmpty()) {
        currentPackets_ = std::move(framePackets);
    } else {
        pendingLatestPackets_ = std::move(framePackets);
        hasPendingLatestPackets_ = true;
    }

    if (!timer_.isActive()) {
        timer_.start(flushIntervalMs_);
    }
}

void UdpPacketQueue::promotePendingFrame() {
    if (!hasPendingLatestPackets_) {
        return;
    }

    currentPackets_ = std::move(pendingLatestPackets_);
    pendingLatestPackets_.clear();
    hasPendingLatestPackets_ = false;
}

void UdpPacketQueue::sendPacketPerTick() {
    int sentCount = 0;

    while (!currentPackets_.isEmpty() && sentCount < packetsPerTick_) {
        const PendingUdpPacket pending = currentPackets_.dequeue();
/*
        if (pending.packet.channel == UdpChannelType::Control
            && pending.packet.type == UdpFrameType::Punch) {
            qDebug() << "发送punch";
            qDebug() << pending.address.toString() << pending.port;
            //UdpPunchPayload payload;
            //UdpControlPayloadCodec::decodePunch(pending.packet.payload, &payload);

        }
        */

        const QByteArray bytes = UdpPacketCodec::encode(pending.packet);

        const qint64 written = sock_->writeDatagram(
                bytes, pending.address, pending.port);

        if (written != bytes.size()) {
            qDebug() << "udp send failed"
                     << "target=" << pending.address.toString()
                     << pending.port
                     << "size=" << bytes.size()
                     << "written=" << written
                     << "error=" << sock_->error()
                     << "errorString=" << sock_->errorString();
            continue;
        }

        ++sentCount;
    }

    if (!currentPackets_.isEmpty()) {
        return;
    }

    promotePendingFrame();

    if (currentPackets_.isEmpty()) {
        timer_.stop();
    }
}
