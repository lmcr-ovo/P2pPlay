//
// Created by ASUS on 2026/7/30.
//

#include <QtCore>
#include "SignalingServer.h"
#include "SignalingConnection.h"

SignalingServer::SignalingServer() {
    connect(&server_, &QTcpServer::newConnection, this, &SignalingServer::onNewConnection);
}

bool SignalingServer::start(const QHostAddress& address, quint16 port) {
    return server_.listen(address, port);
}

void SignalingServer::onNewConnection() {
    QTcpSocket* sock = server_.nextPendingConnection();
    SignalingConnection* connection = new SignalingConnection(sock, this);

    connect(connection, &SignalingConnection::messageReceived, this,
            [this, connection] (const SignalingMessage& message) {
        messageReceived(connection, message);
    });
}
