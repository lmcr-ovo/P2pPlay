//
// Created by ASUS on 2026/7/26.
//

#include "SignalingConnection.h"
#include "SignalingCodec.h"
#include "TcpFrame.h"
#include "TcpFrameCodec.h"

SignalingConnection::SignalingConnection(QTcpSocket* sock, QObject* parent)
    : QObject(parent), sock_(sock) {
    if (sock_ == nullptr) {
        emit errorOccurred("null tcp socket");
        return;
    }

    if (sock_->parent() == nullptr) {
        sock_->setParent(this);
    }

    connect(sock_, &QTcpSocket::readyRead, this, &SignalingConnection::onReadyRead);
    connect(sock_, &QTcpSocket::disconnected, this, &SignalingConnection::disconnected);
    connect(sock_,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this,
            [this](QAbstractSocket::SocketError) {
                emit errorOccurred(sock_->errorString());
            });
}

bool SignalingConnection::sendMessage(const SignalingMessage& message) {
    if (sock_ == nullptr || sock_->state() != QAbstractSocket::ConnectedState) {
        return false;
    }
    TcpFrame frame;
    frame.type = message.type;
    frame.payload = SignalingCodec::encodePayload(message);

    const QByteArray bytes = TcpFrameCodec::encode(frame);
    return sock_->write(bytes) == bytes.size();
}

QTcpSocket* SignalingConnection::tcpSocket() const {
    return sock_;
}


void SignalingConnection::onReadyRead() {
    if (sock_ == nullptr) {
        return;
    }
    buffer_.append(sock_->readAll());

    while (true) {
        TcpFrame frame;
        if (!TcpFrameCodec::tryDecode(buffer_, frame)) {
            break;
        }

        SignalingMessage message;
        if (!SignalingCodec::decodePayload(frame.type, frame.payload, message)) {
            emit errorOccurred("invalid signaling message");
            continue;
        }

        emit messageReceived(message);
    }
}
