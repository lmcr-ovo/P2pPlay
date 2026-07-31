//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_NATPROBESERVICE_H
#define P2PPLAY_NATPROBESERVICE_H

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>

class NatProbeService : public QObject {
Q_OBJECT

public:
    explicit NatProbeService(QObject* parent = nullptr);

    bool start(const QHostAddress& address, quint16 port);
    void stop();

signals:
    void probeReceived(const QString& roomId,
                       const QString& clientId,
                       const QHostAddress& address,
                       quint16 port);

    void errorOccurred(const QString& reason);

private slots:
    void onReadyRead();

private:
    bool decodeProbePayload(const QByteArray& payload,
                            QString* roomId,
                            QString* clientId) const;

    QByteArray encodeProbeAckPayload(const QString& roomId,
                                     const QString& clientId) const;

private:
    QUdpSocket socket_;
};
#endif //P2PPLAY_NATPROBESERVICE_H
