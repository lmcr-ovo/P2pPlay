//
// Created by ASUS on 2026/8/5.
//

#ifndef P2PPLAY_VIDEOSAMPLEPIPLINE_H
#define P2PPLAY_VIDEOSAMPLEPIPLINE_H

#include <QObject>
#include "AppConfig.h"
#include "VideoDecoder.h"
#include "VideoDecoderWorker.h"

class VideoRecevierPipline : public QObject {
    Q_OBJECT
public:
    explicit VideoRecevierPipline(QObject* parent);
    void applyConfig(const AppConfig& config);
    VideoDecoderWorker* decoderWorker() const;

signals:
    //void videoSampleBytesReady(const QByteArray& sampleBytes);
    void keyFrameRequestNeeded();

private:
    VideoDecoder decoder_;
};


#endif //P2PPLAY_VIDEOSAMPLEPIPLINE_H
