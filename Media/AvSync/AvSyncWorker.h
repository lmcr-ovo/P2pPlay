//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_AVSYNCWORKER_H
#define P2PPLAY_AVSYNCWORKER_H


#include <QObject>

#include "Role.h"
#include "AvSyncFrame.h"

class AvSyncWorker : public QObject {
Q_OBJECT
public:
    explicit AvSyncWorker(QObject* parent = nullptr);

public slots:
    void setRole(Role role);
    void setVideoEnabled(bool enabled);
    void setAudioEnabled(bool enabled);
    void setAvSyncEnabled(bool enabled);

    void onVideoFrameReady(const DecodedVideoFrame& frame);
    void onAudioFrameReady(const DecodedAudioFrame& frame);

signals:
    void videoFrameToRender(const DecodedVideoFrame& frame);
    void audioFrameToPlay(const DecodedAudioFrame& frame);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    Role role_ = Role::Unknown;
    bool videoEnabled_ = true;
    bool audioEnabled_ = true;
    bool avSyncEnabled_ = true;
    quint64 lastDesktopAudioPtsMs_ = 0;
    bool desktopClockValid_ = false;
};

#endif //P2PPLAY_AVSYNCWORKER_H
