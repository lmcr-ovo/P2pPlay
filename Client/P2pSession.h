//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_P2PSESSION_H
#define P2PPLAY_P2PSESSION_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QUdpSocket>
#include <QTimer>
#include <QThread>
#include "AppConfig.h"
#include "SignalingMessage.h"
#include "UdpPacket.h"
#include "P2pUdpTransport.h"
#include "P2pSessionWorker.h"

class P2pSession : public QObject {
    Q_OBJECT

public:
    explicit P2pSession(QObject* parent);
    ~P2pSession() override;
    void applyConfig(const P2pConfig& config);
    bool bind(quint16 localPort);
    void setClientInfo(const QString& roomId, const QString& clientId);
    void setServerUdpEndpoint(const QHostAddress& address, quint16 port);
    void setPunchPortRange(quint16 range);
    P2pSessionWorker* worker() const;

signals:
    void p2pReady(const QHostAddress& address, quint16 port);
    void mediaFrameReceived(const UdpFrame& frame);
    void errorOccurred(const QString& reason);
    void logReceived(const QString& message);

public slots:
    void onProbePermitted(const SignalingMessage& message);
    void onPeerEndpoint(const SignalingMessage& message);
    void sendMediaFrame(UdpFrameType type, const QByteArray& payload);

private:
    QThread* thread_ = nullptr;
    P2pSessionWorker* worker_ = nullptr;
};


#endif //P2PPLAY_P2PSESSION_H
