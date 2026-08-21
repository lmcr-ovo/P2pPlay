//
// Created by ASUS on 2026/7/29.
//

#ifndef P2PPLAY_UDPPACKETQUEUE_H
#define P2PPLAY_UDPPACKETQUEUE_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QQueue>
#include "UdpPacket.h"

struct PendingUdpPacket {
    UdpPacket packet;
    QHostAddress address;
    quint16 port = 0;
};

class UdpPacketQueue : public QObject {
    Q_OBJECT

public:
    explicit UdpPacketQueue(QObject* parent, QUdpSocket* sock);
    void setTick(int packetPerTick, int flushIntervalMs);

signals:
    void sendBlock();

public slots:
    void onPacketsReadyToSend(QQueue<UdpPacket> packets,
            const QHostAddress& peerAddress, quint16 PeerPort);

private slots:
    void sendPacketPerTick();

private:
    QQueue<PendingUdpPacket> buildPendingFrame(QQueue<UdpPacket>& packets,
                                               const QHostAddress& peerAddress,
                                               quint16 peerPort) const;
    bool writePacket(const PendingUdpPacket& pending);
    void sendControlPackets();
    void sendMediaPackets();

private:
    QUdpSocket* sock_;
    QTimer timer_;
    // 媒体：所有帧排队，逐个发送（不丢帧）
    QQueue<PendingUdpPacket> mediaQueue_;
    // 控制：排队逐个发送（不丢）
    QQueue<PendingUdpPacket> controlQueue_;
    int packetsPerTick_ = 32;
    int flushIntervalMs_ = 1;
};


#endif //P2PPLAY_UDPPACKETQUEUE_H
