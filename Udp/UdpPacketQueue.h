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
    void promoteMediaPending();
    void sendControlPackets();
    void sendMediaPackets();

private:
    QUdpSocket* sock_;
    QTimer timer_;
    // 媒体：只保留最新帧（丢旧帧）
    QQueue<PendingUdpPacket> mediaCurrent_;
    QQueue<PendingUdpPacket> mediaPendingLatest_;
    bool hasMediaPending_ = false;
    // 控制：排队逐个发送（不丢）
    QQueue<PendingUdpPacket> controlQueue_;
    int packetsPerTick_ = 32;
    int flushIntervalMs_ = 1;
};


#endif //P2PPLAY_UDPPACKETQUEUE_H
