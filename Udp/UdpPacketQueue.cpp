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
    if (packets.isEmpty()) {
        return;
    }

    const bool isControl = packets.head().channel == UdpChannelType::Control;

    QQueue<PendingUdpPacket> framePackets = buildPendingFrame(
            packets,
            peerAddress,
            PeerPort);

    if (isControl) {
        while (!framePackets.isEmpty()) {
            controlQueue_.enqueue(framePackets.dequeue());
        }
    } else {
        while (!framePackets.isEmpty()) {
            mediaQueue_.enqueue(framePackets.dequeue());
        }
    }

    if (!timer_.isActive()) {
        timer_.start(flushIntervalMs_);
    }
}

void UdpPacketQueue::sendPacketPerTick() {
    sendControlPackets();
    sendMediaPackets();

    if (controlQueue_.isEmpty() && mediaQueue_.isEmpty()) {
        timer_.stop();
    }
}

void UdpPacketQueue::sendControlPackets() {
    int sent = 0;

    while (!controlQueue_.isEmpty() && sent < packetsPerTick_) {
        const PendingUdpPacket pending = controlQueue_.dequeue();

        if (pending.packet.type == UdpFrameType::Punch) {
            qDebug() << "发送punch";
            qDebug() << pending.address.toString() << pending.port;
        }

        if (!writePacket(pending)) {
            continue;
        }

        ++sent;
    }
}

void UdpPacketQueue::sendMediaPackets() {
    int sent = 0;

    while (!mediaQueue_.isEmpty() && sent < packetsPerTick_) {
        const PendingUdpPacket pending = mediaQueue_.dequeue();

        if (!writePacket(pending)) {
            continue;
        }

        ++sent;
    }
}

bool UdpPacketQueue::writePacket(const PendingUdpPacket& pending) {
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
        return false;
    }

    return true;
}
