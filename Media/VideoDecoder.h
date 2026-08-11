//
// Created by ASUS on 2026/8/5.
//

#ifndef P2PPLAY_VIDEODECODER_H
#define P2PPLAY_VIDEODECODER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include "AppConfig.h"
#include "VideoSample.h"
#include "VideoDecoderWorker.h"

class VideoDecoder : public QObject {
    Q_OBJECT
public:
    explicit VideoDecoder(QObject* parent);
    ~VideoDecoder() override;
    void applyConfig(const AppConfig& config);
    VideoDecoderWorker* worker() const;

private:
    QThread* thread_ = nullptr;
    VideoDecoderWorker* worker_ = nullptr;
};


#endif //P2PPLAY_VIDEODECODER_H
