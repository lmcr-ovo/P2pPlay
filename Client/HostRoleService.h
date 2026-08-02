//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_HOSTROLESERVICE_H
#define P2PPLAY_HOSTROLESERVICE_H

#include <QObject>
#include <QHostAddress>

#include "SignalingMessage.h"

class HostRoleService : public QObject {
    Q_OBJECT

public:
    explicit HostRoleService(QObject* parent = nullptr);

    void setClientId(const QString& clientId);
    QString clientId() const;
    QString roomId() const;

signals:
    void sendMessage(const SignalingMessage& message);
    void traversalContextReady(const QString& roomId, const QString& clientId);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

public slots:
    void onConnected();
    void onRoomCreated(const SignalingMessage& message);
    void onPeerJoined(const SignalingMessage& message);
    void onP2pReady(const QHostAddress& address, quint16 port);

private:
    void sendRegister();
    void sendCreateRoom();
    void sendProbeRequest();

private:
    QString clientId_;
    QString roomId_;
};


#endif //P2PPLAY_HOSTROLESERVICE_H
