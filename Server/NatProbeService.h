//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_NATPROBESERVICE_H
#define P2PPLAY_NATPROBESERVICE_H

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include "P2pUdpTransport.h"

class NatProbeService : public QObject {
Q_OBJECT

public:
    NatProbeService(QObject* parent);
    bool start(const QHostAddress& address, quint16 port);

signals:
    void probeReceived(
            const QString& roomId,
            const QString& clientId,
            const QHostAddress& address,
            quint16 port
            );

    void errorOccurred(const QString& reason);

public slots:
    void onFrameReady(const UdpFrame& frame);

private:
    QUdpSocket sock_;
    P2pUdpTransport transport_;
};
#endif //P2PPLAY_NATPROBESERVICE_H
