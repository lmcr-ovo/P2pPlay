#ifndef P2PPLAY_MEDIASERVICE_H
#define P2PPLAY_MEDIASERVICE_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include "Role.h"
#include "UdpPacket.h"
#include "MediaServiceWorker.h"

class MediaService : public QObject {
Q_OBJECT

public:
    explicit MediaService(QObject* parent = nullptr);
    ~MediaService() override;
    MediaServiceWorker* worker() const;
    void setRole(Role role);
    Role role() const;

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
    QThread* thread_ = nullptr;
    MediaServiceWorker* worker_ = nullptr;
};

#endif // P2PPLAY_MEDIASERVICE_H