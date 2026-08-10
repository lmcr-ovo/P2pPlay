//
// Created by ASUS on 2026/8/10.
//

#ifndef P2PPLAY_P2PSESSIONWORKER_H
#define P2PPLAY_P2PSESSIONWORKER_H


#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QUdpSocket>
#include <QTimer>
#include "AppConfig.h"
#include "SignalingMessage.h"
#include "UdpPacket.h"
#include "P2pUdpTransport.h"

class P2pSessionWorker : public QObject {
Q_OBJECT

public:
    explicit P2pSessionWorker(QObject* parent = nullptr);

    void applyConfig(const P2pConfig& config);
    bool bind(quint16 localPort);
    void setClientInfo(const QString& roomId, const QString& clientId);
    void setServerUdpEndpoint(const QHostAddress& address, quint16 port);
    void setPunchPortRange(quint16 range);

signals:
    void p2pReady(const QHostAddress& address, quint16 port);
    void mediaFrameReceived(const UdpFrame& frame);
    void errorOccurred(const QString& reason);
    void logReceived(const QString& message);

public slots:
    void onProbePermitted(const SignalingMessage& message);
    void onPeerEndpoint(const SignalingMessage& message);
    void sendMediaFrame(UdpFrameType type, const QByteArray& payload);

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


    QString peerClientId_;
};

#endif //P2PPLAY_P2PSESSIONWORKER_H
