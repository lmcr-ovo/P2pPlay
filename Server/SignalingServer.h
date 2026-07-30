//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_SIGNALINGSERVER_H
#define P2PPLAY_SIGNALINGSERVER_H

#include <QObject>
#include <QTcpServer>
#include "SignalingMessage.h"
#include "SignalingConnection.h"

class SignalingServer : QObject {
    Q_OBJECT
public:
    SignalingServer();
    void start(const QHostAddress& address, quint16 port);

signals:
    void messageReceived(const SignalingConnection* connection,
            const SignalingMessage& message);

private slots:
    void onNewConnection();

private:
    QTcpServer server_;
};


#endif //P2PPLAY_SIGNALINGSERVER_H
