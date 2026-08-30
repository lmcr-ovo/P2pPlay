//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_AVSYNCSERVICE_H
#define P2PPLAY_AVSYNCSERVICE_H


#include <QObject>
#include <QThread>

#include "AvSyncWorker.h"

class AvSyncService : public QObject {
Q_OBJECT
public:
    explicit AvSyncService(QObject* parent = nullptr);
    ~AvSyncService() override;

    AvSyncWorker* worker() const;

public slots:
    void setRole(Role role);
    void setVideoEnabled(bool enabled);
    void setAudioEnabled(bool enabled);
    void setAvSyncEnabled(bool enabled);

signals:
    void videoFrameToRender(const DecodedVideoFrame& frame);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    QThread workerThread_;
    AvSyncWorker* worker_ = nullptr;
};

#endif //P2PPLAY_AVSYNCSERVICE_H
