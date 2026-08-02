//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_GUESTROLESERVICE_H
#define P2PPLAY_GUESTROLESERVICE_H

#include <QObject>
#include <QHostAddress>

#include "SignalingMessage.h"

class GuestRoleService : public QObject {
    Q_OBJECT

public:
    explicit GuestRoleService(QObject* parent = nullptr);

    void setClientInfo(const QString& roomId, const QString& clientId);
    QString clientId() const;
    QString roomId() const;

signals:
    void sendMessage(const SignalingMessage& message);
    void traversalContextReady(const QString& roomId, const QString& clientId);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

public slots:
    void onConnected();
    void onLogReceived(const SignalingMessage& message);
    void onP2pReady(const QHostAddress& address, quint16 port);

private:
    void sendRegister();
    void sendJoinRoom();

private:
    QString roomId_;
    QString clientId_;
};


#endif //P2PPLAY_GUESTROLESERVICE_H
