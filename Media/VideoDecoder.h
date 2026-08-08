//
// Created by ASUS on 2026/8/5.
//

#ifndef P2PPLAY_VIDEODECODER_H
#define P2PPLAY_VIDEODECODER_H

#include <QObject>
#include <QImage>
#include "VideoSample.h"
#include "TraceManager.h"

class VideoDecoder : public QObject {
    Q_OBJECT
public:
    explicit VideoDecoder(QObject* parent);

signals:
    void videoImageReady(const QImage& img, quint32 sampleId);

public slots:
    void onVideoSampleBytesReceived(const QByteArray& sampleBytes);

private:
    void handleJpeg(VideoSample& sample);
};


#endif //P2PPLAY_VIDEODECODER_H
