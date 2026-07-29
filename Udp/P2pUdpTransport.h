//
// Created by ASUS on 2026/7/27.
//

#ifndef P2PPLAY_P2PUDPTRANSPORT_H
#define P2PPLAY_P2PUDPTRANSPORT_H

#include <QObject>
#include <QUdpSocket>
#include "UdpFrameReassembler.h"

class P2pUdpTransport : public QObject {
    Q_OBJECT

public:
    explicit P2pUdpTransport(QUdpSocket* sock, QObject* parent);
    bool sendFrame(UdpChannelType channel, UdpFrameType type, const QByteArray& payload);
    void setPeerEndpoint(const QHostAddress& address, quint16 port);

signals:
    void errorOccurred(const QString& reason);
    void frameReady(const UdpFrame& frame);
    void frameDropped(quint32 frameSeq);

private slots:
    void onReadyRead();

private:
    QUdpSocket* sock_;
    UdpFrameReassembler reassembler_;
    QHostAddress peerAddress_;
    quint16 peerPort_ = 0;
    quint32 nextFrameSeq = 0;
};


#endif //P2PPLAY_P2PUDPTRANSPORT_H
