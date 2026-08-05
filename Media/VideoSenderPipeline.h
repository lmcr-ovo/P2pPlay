//
// Created by ASUS on 2026/8/4.
//

#ifndef P2PPLAY_VIDEOSENDERPIPELINE_H
#define P2PPLAY_VIDEOSENDERPIPELINE_H

#include <QObject>
#include "AppConfig.h"
#include "ScreenVideoSource.h"
#include "VideoEncoder.h"

class VideoSenderPipeline : public QObject {
    Q_OBJECT
public:
    VideoSenderPipeline(QObject* parent);
    void applyConfig(const AppConfig& config);
    void start();
    void stop();
signals:
    void videoSampleBytesReady(const QByteArray& bytes);

private:
    ScreenVideoSource videoSource_;
    VideoEncoder encoder_;
};


#endif //P2PPLAY_VIDEOSENDERPIPELINE_H
