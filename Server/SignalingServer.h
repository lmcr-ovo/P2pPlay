//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_SIGNALINGSERVER_H
#define P2PPLAY_SIGNALINGSERVER_H

#include <QObject>
#include <QTcpServer>
#include "SignalingMessage.h"
#include "SignalingConnection.h"

class SignalingServer : public QObject {
    Q_OBJECT
public:
    SignalingServer();
    bool start(const QHostAddress& address, quint16 port);

signals:
    void messageReceived(SignalingConnection* connection,
            const SignalingMessage& message);

private slots:
    void onNewConnection();

private:
    QTcpServer server_;
};


#endif //P2PPLAY_SIGNALINGSERVER_H
