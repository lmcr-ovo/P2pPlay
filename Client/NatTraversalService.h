//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_NATTRAVERSALSERVICE_H
#define P2PPLAY_NATTRAVERSALSERVICE_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QUdpSocket>
#include <QTimer>
#include "SignalingMessage.h"
#include "UdpPacket.h"
#include "P2pUdpTransport.h"

class NatTraversalService : public QObject {
    Q_OBJECT

public:
    explicit NatTraversalService(QObject* parent);

    bool bind(quint16 localPort);
    void setClientInfo(const QString& roomId, const QString& clientId);
    void setServerUdpEndpoint(const QHostAddress& address, quint16 port);
    void setPunchPortRange(quint16 range);

signals:
    void p2pReady(const QHostAddress& address, quint16 port);
    void errorOccurred(const QString& reason);
    void logReceived(const QString& message);

public slots:
    void onProbePermitted(const SignalingMessage& message);
    void onPeerEndpoint(const SignalingMessage& message);

private slots:
    void onFrameReady(const UdpFrame& frame);
    void sendPunch();

private:
    void sendProbe();
    QList<quint16> punchCandidatePorts() const;
    void handlePunch(const UdpFrame& frame);
    void handlePunchAck(const UdpFrame& frame);
    void markP2pReady(const QHostAddress& address, quint16 port);

private:
    QUdpSocket sock_;
    P2pUdpTransport transport_;

    QString roomId_;
    QString clientId_;

    QHostAddress serverUdpAddress_;
    quint16 serverUdpPort_ = 0;

    QHostAddress peerAddress_;
    quint16 peerPort_ = 0;

    QTimer punchTimer_;
    quint16 punchPortRange_ = 32;
    bool p2pReady_ = false;
};


#endif //P2PPLAY_NATTRAVERSALSERVICE_H
