//
// Created by ASUS on 2026/7/27.
//
#include <QNetworkDatagram>
#include "P2pUdpTransport.h"
#include "UdpPacket.h"
#include "UdpPacketCodec.h"
#include "UdpFragmenter.h"

P2pUdpTransport::P2pUdpTransport(QUdpSocket* sock, QObject* parent)
    : QObject(parent), sock_(sock), reassembler_(),
    packetQueue_(this, sock) {
    if (sock == nullptr) {
        emit errorOccurred("null udp socket");
        return;
    }

    connect(sock_, &QUdpSocket::readyRead, this, &P2pUdpTransport::onReadyRead);
    connect(&reassembler_, &UdpFrameReassembler::frameReady,
            this, &P2pUdpTransport::frameReady);

    connect(&reassembler_, &UdpFrameReassembler::frameDropped,
            this, &P2pUdpTransport::frameDropped);
    connect(sock_,
            QOverload<QAbstractSocket::SocketError>::of(&QUdpSocket::error),
            this, [this](QAbstractSocket::SocketError err) {
        emit errorOccurred(sock_->errorString());
    });
    connect(this, &P2pUdpTransport::packetsReadyToSend,
            &packetQueue_, &UdpPacketQueue::onPacketsReadyToSend);
}

void P2pUdpTransport::setPeerEndpoint(const QHostAddress& address, quint16 port) {
    peerAddress_ = address;
    peerPort_ = port;
}

void P2pUdpTransport::setTick(int packetPerTick, int flushIntervalMs) {
    packetQueue_.setTick(packetPerTick, flushIntervalMs);
}

bool P2pUdpTransport::sendFrame(UdpChannelType channel, UdpFrameType type, const QByteArray& payload) {
    if (sock_ == nullptr) {
        emit errorOccurred("udp socket is null");
        return false;
    }

    if (peerAddress_.isNull() || peerPort_ == 0) {
        emit errorOccurred("peer endpoint is not set");
        return false;
    }

    quint32 frameSeq = nextFrameSeq_++;
    UdpFrame frame;
    frame.channelType = channel;
    frame.frameType = type;
    frame.payload = payload;

    QQueue<UdpPacket> packets = UdpFragmenter::fragment(frame, frameSeq);
    emit packetsReadyToSend(packets, peerAddress_, peerPort_);
    return true;
}

bool P2pUdpTransport::sendFrameTo(const QHostAddress& address,
        quint16 port,
        UdpChannelType channel,
        UdpFrameType type,
        const QByteArray& payload) {
    if (sock_ == nullptr) {
        emit errorOccurred("udp socket is null");
        return false;
    }

    if (address.isNull() || port == 0) {
        emit errorOccurred("invalid udp endpoint");
        return false;
    }

    UdpFrame frame;
    frame.channelType = channel;
    frame.frameType = type;
    frame.payload = payload;

    const quint32 frameSeq = allocFrameSeq();
    QQueue<UdpPacket> packets = UdpFragmenter::fragment(frame, frameSeq);

    if (packets.isEmpty()) {
        emit errorOccurred("udp fragment failed");
        return false;
    }

    emit packetsReadyToSend(packets, address, port);
    return true;
}

quint32 P2pUdpTransport::allocFrameSeq() {
    return nextFrameSeq_++;
}

void P2pUdpTransport::clearPeerEndpoint() {
    peerAddress_ = QHostAddress();
    peerPort_ = 0;
}

void P2pUdpTransport::setPeerFilterEnabled(bool enabled) {
    peerFilterEnabled_ = enabled;
}

bool P2pUdpTransport::shouldAcceptDatagram(
        const QHostAddress& senderAddress,
        quint16 senderPort) const {
    if (!peerFilterEnabled_) {
        return true;
    }

    if (peerAddress_.isNull() || peerPort_ == 0) {
        return true;
    }

    return senderAddress == peerAddress_
           && senderPort == peerPort_;
}

void P2pUdpTransport::onReadyRead() {
    while (sock_->hasPendingDatagrams()) {
        QNetworkDatagram datagram = sock_->receiveDatagram();

        const QByteArray bytes = datagram.data();
        const QHostAddress senderAddress = datagram.senderAddress();
        const quint16 senderPort = datagram.senderPort();

        if (!shouldAcceptDatagram(senderAddress, senderPort)) {
            continue;
        }

        UdpPacket packet;
        if (!UdpPacketCodec::decode(bytes, packet)) {
            emit errorOccurred("invalid udp packet");
            continue;
        }

        reassembler_.pushPacket(packet, senderAddress, senderPort);
    }
}