//
// Created by ASUS on 2026/8/4.
//

#ifndef P2PPLAY_VIDEOENCODER_H
#define P2PPLAY_VIDEOENCODER_H

#include <QObject>
#include <QImage>
#include "AppConfig.h"
#include "VideoSample.h"
#include "TraceManager.h"

class VideoEncoderWorker : public QObject {
Q_OBJECT
public:
    VideoEncoderWorker() = default;
    void applyConfig(const AppConfig& config);

signals:
    void videoSampleBytesReady(const QByteArray& sampleBytes);

public slots:
    void onVideoImageReady(const QImage &img,
                           quint32 sampleSeq);
    QByteArray handleJpeg(const QImage& img, quint32 sampleSeq);

private:
    VideoSampleCodecType codecType_ = VideoSampleCodecType::Jpeg;
    quint16 jpegQuality_ = 50;
    quint32 nextVideoSeq_ = 0;
};

class VideoEncoder : public QObject {
    Q_OBJECT
public:
    explicit VideoEncoder(QObject* parent);
    ~VideoEncoder() override ;
    void applyConfig(const AppConfig& config);

signals:
    void videoSampleBytesReady(const QByteArray& sampleBytes);

public slots:
    void onVideoImageReady(const QImage &img,
            quint32 sampleSeq);
    //QByteArray handleJpeg(const QImage& img, quint32 sampleSeq);

private:
    //VideoSampleCodecType codecType_ = VideoSampleCodecType::Jpeg;
    //quint16 jpegQuality_ = 50;
    //quint32 nextVideoSeq_ = 0;
    QThread* thread_;
    VideoEncoderWorker* worker_;
};


#endif //P2PPLAY_VIDEOENCODER_H
