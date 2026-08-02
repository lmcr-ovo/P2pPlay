//
// Created by ASUS on 2026/8/1.
//

#include "SignalingClient.h"

SignalingClient::SignalingClient(QObject* parent)
    : QObject(parent),
      sock_(this),
      connection_(&sock_, this) {
    connect(&sock_, &QTcpSocket::connected,
            this, &SignalingClient::connected);
    connect(&connection_, &SignalingConnection::disconnected,
            this, &SignalingClient::disconnected);
    connect(&connection_, &SignalingConnection::messageReceived,
            this, &SignalingClient::messageReceived);
    connect(&connection_, &SignalingConnection::errorOccurred,
            this, &SignalingClient::errorOccurred);
}


void SignalingClient::connectToServer(const QHostAddress& address, quint16 port) {
    if (address.isNull() || port == 0) {
        emit errorOccurred("invalid signaling server endpoint");
        return;
    }

    if (sock_.state() != QAbstractSocket::UnconnectedState) {
        sock_.abort();
    }

    sock_.connectToHost(address, port);
}

bool SignalingClient::sendMessage(const SignalingMessage &message) {
    return connection_.sendMessage(message);
}

bool SignalingClient::isConnected() {
    return sock_.state() == QAbstractSocket::ConnectedState;
}
