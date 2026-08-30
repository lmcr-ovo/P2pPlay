//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_AUDIOSERVICE_H
#define P2PPLAY_AUDIOSERVICE_H


#include <QObject>
#include <QThread>

#include "AppConfig.h"
#include "AvSync/AvSyncFrame.h"
#include "AudioServiceWorker.h"

class AudioService : public QObject {
Q_OBJECT
public:
    explicit AudioService(QObject* parent = nullptr);
    ~AudioService() override;

    AudioServiceWorker* worker() const;

public slots:
    void applyConfig(const AppConfig& config);
    void setRole(Role role);
    void start();
    void stop();
    void setMicrophoneEnabled(bool enabled);
    void setDesktopAudioEnabled(bool enabled);
    void setPlaybackEnabled(bool enabled);

signals:
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    QThread workerThread_;
    AudioServiceWorker* worker_ = nullptr;
};

#endif //P2PPLAY_AUDIOSERVICE_H
