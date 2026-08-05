//
// Created by ASUS on 2026/8/4.
//

#ifndef P2PPLAY_VIDEOENCODER_H
#define P2PPLAY_VIDEOENCODER_H

#include <QObject>
#include <QImage>
#include "AppConfig.h"
#include "VideoSample.h"

class VideoEncoder : public QObject {
    Q_OBJECT
public:
    explicit VideoEncoder(QObject* parent);
    void applyConfig(const AppConfig& config);

signals:
    void videoSampleBytesReady(const QByteArray& sampleBytes);

public slots:
    void onVideoImageReady(const QImage& img);
    QByteArray handleJpeg(const QImage& img);

private:
    VideoSampleCodecType codecType_ = VideoSampleCodecType::Jpeg;
    quint16 jpegQuality_ = 50;
    quint32 nextVideoSeq_ = 0;
};


#endif //P2PPLAY_VIDEOENCODER_H
