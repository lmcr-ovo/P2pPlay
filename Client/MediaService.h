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
    void mediaFrameToSend(UdpFrameType type, const QByteArray& payload);

    void videoFrameReceived(const QByteArray& payload);
    void audioFrameReceived(const QByteArray& payload);
    void inputEventReceived(const QByteArray& payload);
    void keyFrameRequestReceived(const QByteArray& payload);

    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

public slots:
    void onP2pReady(const QHostAddress& address, quint16 port);
    void onMediaFrameReceived(const UdpFrame& frame);

    bool sendVideoFrame(const QByteArray& payload);
    bool sendAudioFrame(const QByteArray& payload);
    bool sendInputEvent(const QByteArray& payload);
    bool sendKeyFrameRequest(const QByteArray& payload);

private:
    bool canSend(UdpFrameType type) const;

private:
    MediaRole role_ = MediaRole::Unknown;
    bool running_ = false;
};

#endif // P2PPLAY_MEDIASERVICE_H