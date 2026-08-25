//
// Created by ASUS on 2026/8/25.
//

#ifndef P2PPLAY_CONTROLSERVICEWORKER_H
#define P2PPLAY_CONTROLSERVICEWORKER_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>

#include "Role.h"
#include "UdpPacket.h"

class ControlServiceWorker : public QObject {
Q_OBJECT

public:
    explicit ControlServiceWorker(QObject* parent = nullptr);

    Role role() const;
    bool isRunning() const;

public slots:
    void setRole(Role role);
    void start();
    void stop();

    void onP2pReady(
            const QHostAddress& address,
            quint16 port);

    void onControlFrameReceived(
            const UdpFrame& frame);

    bool sendInputEvent(
            const QByteArray& payload);

    bool sendKeyFrameRequest();

    bool sendReceiverReport(
            const QByteArray& payload);

signals:
    void inputEventReceived(
            const QByteArray& payload);

    void keyFrameRequestReceived(
            const QByteArray& payload);

    void receiverReportReceived(
            const QByteArray& payload);

    void controlFrameToSend(
            UdpFrameType type,
            const QByteArray& payload);

    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    bool canSend(UdpFrameType type) const;

private:
    Role role_ = Role::Unknown;
    bool running_ = false;
};

#endif
