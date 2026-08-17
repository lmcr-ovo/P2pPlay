//
// Created by ASUS on 2026/7/29.
//

#include "UdpPacketQueue.h"
#include "UdpPacketCodec.h"
#include "Video/VideoSampleCodec.h"

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
    } else if (mediaCurrent_.isEmpty()) {
        mediaCurrent_ = std::move(framePackets);
    } else {
        if (!mediaPendingLatest_.isEmpty()) {
            quint32 droppedSeq = 0;
            if (VideoSampleCodec::peekVideoSeq(
                    mediaPendingLatest_.front().packet.payload,
                    droppedSeq)) {
                qDebug() << "丢弃媒体帧(被覆盖), sampleSeq =" << droppedSeq;
            }
        }

        mediaPendingLatest_ = std::move(framePackets);
        hasMediaPending_ = true;
    }

    if (!timer_.isActive()) {
        timer_.start(flushIntervalMs_);
    }
}

void UdpPacketQueue::promoteMediaPending() {
    if (!hasMediaPending_) {
        return;
    }

    mediaCurrent_ = std::move(mediaPendingLatest_);
    mediaPendingLatest_.clear();
    hasMediaPending_ = false;
}

void UdpPacketQueue::sendPacketPerTick() {
    sendControlPackets();
    sendMediaPackets();

    if (controlQueue_.isEmpty()
        && mediaCurrent_.isEmpty()
        && !hasMediaPending_) {
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

    while (!mediaCurrent_.isEmpty() && sent < packetsPerTick_) {
        const PendingUdpPacket pending = mediaCurrent_.dequeue();

        if (!writePacket(pending)) {
            continue;
        }

        ++sent;
    }

    if (mediaCurrent_.isEmpty()) {
        promoteMediaPending();
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
