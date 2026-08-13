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
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    QThread* thread_ = nullptr;
    MediaServiceWorker* worker_ = nullptr;
};

#endif // P2PPLAY_MEDIASERVICE_H