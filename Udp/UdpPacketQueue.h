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
    QUdpSocket* sock_;
    QTimer timer_;
    QQueue<UdpPacket> packets_;
    QHostAddress peerAddress_;
    quint16 peerPort_ = 0;
    int packetsPerTick_ = 32;
    int flushIntervalMs_ = 1;
};


#endif //P2PPLAY_UDPPACKETQUEUE_H
