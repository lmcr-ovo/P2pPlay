//
// Created by ASUS on 2026/8/4.
//

#ifndef P2PPLAY_VIDEOENCODER_H
#define P2PPLAY_VIDEOENCODER_H

#include <QByteArray>
#include <QObject>
#include <QImage>
#include <QThread>
#include "AppConfig.h"
#include "VideoSample.h"
#include "TraceManager.h"

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

class VideoEncoderWorker : public QObject {
Q_OBJECT
public:
    explicit VideoEncoderWorker(QObject* parent = nullptr);
    ~VideoEncoderWorker() override;
    void applyConfig(const AppConfig& config);

signals:
    void videoSampleBytesReady(const QByteArray& sampleBytes);

public slots:
    void onVideoImageReady(const QImage &img,
                           quint32 sampleSeq);
    void requestKeyFrame();

private:
    QByteArray handleJpeg(const QImage& img, quint32 sampleSeq);
    QByteArray handleH264(const QImage& img, quint32 sampleSeq);

    bool ensureH264Encoder(int width, int height);
    void stopH264Encoder();
    QByteArray drainH264Packets(quint32& sampleFlags, quint32 sampleSeq);

    VideoSampleCodecType codecType_ = VideoSampleCodecType::Jpeg;
    quint16 jpegQuality_ = 50;

    quint16 fps_ = 60;
    int h264BitrateKbps_ = 4000;
    int h264GopFrames_ = 60;
    int h264EncoderThreads_ = 1;
    QString h264Preset_ = "ultrafast";
    QString h264Tune_ = "zerolatency";
    QString h264Profile_ = "baseline";
    bool h264RepeatHeaders_ = true;
    bool h264ForceIdr_ = true;
    bool forceNextKeyFrame_ = true;

    // 编码器
    AVCodecContext* h264CodecContext_ = nullptr;
    // 装YUV
    AVFrame* h264Frame_ = nullptr;
    // 编码器输出结果
    AVPacket* h264Packet_ = nullptr;
    // 将QImage的ARGB转为YUV
    SwsContext* h264SwsContext_ = nullptr;
    int h264Width_ = 0;
    int h264Height_ = 0;
    qint64 nextPts_ = 0;
};

class VideoEncoder : public QObject {
    Q_OBJECT
public:
    explicit VideoEncoder(QObject* parent);
    ~VideoEncoder() override ;
    void applyConfig(const AppConfig& config);
    VideoEncoderWorker* worker() const;

private:
    QThread* thread_;
    VideoEncoderWorker* worker_;
};


#endif //P2PPLAY_VIDEOENCODER_H
