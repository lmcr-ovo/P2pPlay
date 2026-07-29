//
// Created by ASUS on 2026/7/27.
//
#include <QNetworkDatagram>
#include "P2pUdpTransport.h"
#include "UdpPacket.h"
#include "UdpPacketCodec.h"
#include "UdpFragmenter.h"

P2pUdpTransport::P2pUdpTransport(QUdpSocket* sock, QObject* parent)
    : QObject(parent), sock_(sock), reassembler_() {
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
}

void P2pUdpTransport::setPeerEndpoint(const QHostAddress& address, quint16 port) {
    peerAddress_ = address;
    peerPort_ = port;
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

    quint32 frameSeq = nextFrameSeq++;
    UdpFrame frame;
    frame.channelType = channel;
    frame.frameType = type;
    frame.payload = payload;

    int sentCount = 0;

    QList<UdpPacket> packets = UdpFragmenter::fragment(frame, frameSeq);
    for (UdpPacket& packet : packets) {
        QByteArray bytes = UdpPacketCodec::encode(packet);
        qint64 written = sock_->writeDatagram(bytes, peerAddress_, peerPort_);

        if (written != bytes.size()) {
            emit errorOccurred("udp write datagram failed");
            return false;
        }
        ++sentCount;
        if (sentCount % 24 == 0) {
            QThread::usleep(500);
        }
    }
    return true;
}

void P2pUdpTransport::onReadyRead() {
    while (sock_->hasPendingDatagrams()) {
        QNetworkDatagram datagram = sock_->receiveDatagram();

        const QByteArray bytes = datagram.data();
        const QHostAddress senderAddress = datagram.senderAddress();
        const quint16 senderPort = datagram.senderPort();

        if (!peerAddress_.isNull() && peerPort_ != 0) {
            if (senderAddress != peerAddress_ || senderPort != peerPort_) {
                continue;
            }
        }

        UdpPacket packet;
        if (!UdpPacketCodec::decode(bytes, packet)) {
            emit errorOccurred("invalid udp packet");
            continue;
        }

        reassembler_.pushPacket(packet);
    }
}