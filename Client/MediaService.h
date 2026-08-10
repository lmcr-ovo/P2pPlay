#ifndef P2PPLAY_MEDIASERVICE_H
#define P2PPLAY_MEDIASERVICE_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>

#include "UdpPacket.h"

enum class MediaRole {
    Unknown,
    Host,
    Guest
};

class MediaService : public QObject {
Q_OBJECT

public:
    explicit MediaService(QObject* parent = nullptr);

    void setRole(MediaRole role);
    MediaRole role() const;

    void start();
    void stop();

    bool isRunning() const;

signals:
    void udpMediaFrameToSend(UdpFrameType type, const QByteArray& payload);

    void videoSampleBytesReceived(const QByteArray& sampleBytes);
    void audioSampleBytesReceived(const QByteArray& sampleBytes);
    void inputCommandReceived(const QByteArray& commandBytes);
    void keyFrameRequestReceived(const QByteArray& requestByes);

    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

public slots:
    void onP2pReady(const QHostAddress& address, quint16 port);
    void onUdpMediaFrameReceived(const UdpFrame& frame);

    bool sendVideoSampleBytes(const QByteArray& sampleBytes);
    bool sendAudioSampleBytes(const QByteArray& sampleBytes);
    bool sendInputCommand(const QByteArray& commandBytes);
    bool sendKeyFrameRequest(const QByteArray& requestBytes);

private:
    bool canSend(UdpFrameType type) const;

private:
    MediaRole role_ = MediaRole::Unknown;
    bool running_ = false;
};

#endif // P2PPLAY_MEDIASERVICE_H