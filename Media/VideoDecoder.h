//
// Created by ASUS on 2026/8/5.
//

#ifndef P2PPLAY_VIDEODECODER_H
#define P2PPLAY_VIDEODECODER_H

#include <QObject>
#include <QImage>
#include "AppConfig.h"
#include "VideoSample.h"
#include "TraceManager.h"

struct AVCodecContext;
struct AVCodecParserContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

class VideoDecoder : public QObject {
    Q_OBJECT
public:
    explicit VideoDecoder(QObject* parent);
    ~VideoDecoder() override;
    void applyConfig(const AppConfig& config);

signals:
    void videoImageReady(const QImage& img, quint32 sampleId);
    void keyFrameRequestNeeded();

public slots:
    void onVideoSampleBytesReceived(const QByteArray& sampleBytes);

private:
    void handleJpeg(VideoSample& sample);
    void handleH264(VideoSample& sample);
    bool ensureH264Decoder();
    void stopH264Decoder();
    void drainH264Frames(quint32 sampleSeq);
    void requestKeyFrameIfNeeded();

    bool hasLastH264Seq_ = false;
    quint32 lastH264Seq_ = 0;
    bool waitingForKeyFrame_ = true;
    qint64 lastKeyFrameRequestMs_ = 0;
    int keyFrameRequestIntervalMs_ = 200;

    AVCodecContext* h264CodecContext_ = nullptr;
    AVCodecParserContext* h264Parser_ = nullptr;
    AVFrame* h264Frame_ = nullptr;
    AVPacket* h264Packet_ = nullptr;
    SwsContext* h264SwsContext_ = nullptr;
};


#endif //P2PPLAY_VIDEODECODER_H
