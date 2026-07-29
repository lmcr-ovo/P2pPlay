//
// Created by ASUS on 2026/7/27.
//

#ifndef P2PPLAY_P2PUDPTRANSPORT_H
#define P2PPLAY_P2PUDPTRANSPORT_H

#include <QObject>
#include <QUdpSocket>
#include "UdpFrameReassembler.h"
#include "UdpPacketQueue.h"

class P2pUdpTransport : public QObject {
    Q_OBJECT

public:
    explicit P2pUdpTransport(QUdpSocket* sock, QObject* parent);
    bool sendFrame(UdpChannelType channel, UdpFrameType type, const QByteArray& payload);
    void setPeerEndpoint(const QHostAddress& address, quint16 port);
    void setTick(int packetPerTick, int flushIntervalMs);

signals:
    void errorOccurred(const QString& reason);
    void frameReady(const UdpFrame& frame);
    void frameDropped(quint32 frameSeq);
    void packetsReadyToSend(QQueue<UdpPacket> packets,
            const QHostAddress& peerAddress,
            quint16 peerPort);

private slots:
    void onReadyRead();

private:
    QUdpSocket* sock_;
    UdpFrameReassembler reassembler_;
    QHostAddress peerAddress_;
    quint16 peerPort_ = 0;
    quint32 nextFrameSeq_ = 0;
    UdpPacketQueue packetQueue_;
};


#endif //P2PPLAY_P2PUDPTRANSPORT_H
