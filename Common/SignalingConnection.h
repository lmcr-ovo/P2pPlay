//
// Created by ASUS on 2026/7/26.
//

#ifndef P2PPLAY_SIGNALINGCONNECTION_H
#define P2PPLAY_SIGNALINGCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include "SignalingMessage.h"

class SignalingConnection : public QObject {
    Q_OBJECT

public:
    explicit SignalingConnection(QTcpSocket* sock, QObject* parent = nullptr);

    bool sendMessage(const SignalingMessage& message);
    QTcpSocket* tcpSocket() const;

signals:
    void messageReceived(const SignalingMessage& message);
    void disconnected();
    void errorOccurred(const QString& reason);

private slots:
    void onReadyRead();

private:
    QTcpSocket* sock_ = nullptr;
    QByteArray buffer_;
};


#endif //P2PPLAY_SIGNALINGCONNECTION_H
