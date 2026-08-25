//
// Created by ASUS on 2026/8/10.
//

#ifndef P2PPLAY_MEDIASERVICEWORKER_H
#define P2PPLAY_MEDIASERVICEWORKER_H

#include <QObject>
#include <QHostAddress>
#include "Role.h"
#include "UdpPacket.h"

class MediaServiceWorker : public QObject {
    Q_OBJECT

public:
    explicit MediaServiceWorker(QObject* parent = nullptr);

    Role role() const;
    bool isRunning() const;

public slots:
    void setRole(Role role);
    void start();
    void stop();

    void onP2pReady(const QHostAddress& address, quint16 port);
    void onUdpMediaFrameReceived(const UdpFrame& frame);

    bool sendVideoSampleBytes(const QByteArray& payload);
    bool sendAudioSampleBytes(const QByteArray& sampleBytes);

signals:
    void videoSampleBytesReceived(const QByteArray& sampleBytes);
    void audioSampleBytesReceived(const QByteArray& sampleBytes);

    void udpMediaFrameToSend(UdpFrameType type, const QByteArray& payload);

    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    bool canSend(UdpFrameType type) const;

private:
    Role role_ = Role::Unknown;
    bool running_ = false;
};

#endif //P2PPLAY_MEDIASERVICEWORKER_H
