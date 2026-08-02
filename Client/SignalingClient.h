//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_SIGNALINGCLIENT_H
#define P2PPLAY_SIGNALINGCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include "SignalingMessage.h"
#include "SignalingConnection.h"

class SignalingClient : public QObject {
    Q_OBJECT
public:
    explicit SignalingClient(QObject* parent);
    void connectToServer(const QHostAddress& address, quint16 port);
    bool sendMessage(const SignalingMessage& message);
    bool isConnected();

signals:
    void connected();
    void disconnected();
    void messageReceived(const SignalingMessage& message);
    void errorOccurred(const QString& reason);

private:
    QTcpSocket sock_;
    SignalingConnection connection_;
};


#endif //P2PPLAY_SIGNALINGCLIENT_H
