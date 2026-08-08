//
// Created by ASUS on 2026/8/5.
//

#ifndef P2PPLAY_VIDEOSAMPLEPIPLINE_H
#define P2PPLAY_VIDEOSAMPLEPIPLINE_H

#include <QObject>
#include "VideoDecoder.h"

class VideoRecevierPipline : public QObject {
    Q_OBJECT
public:
    explicit VideoRecevierPipline(QObject* parent);

signals:
    void videoSampleBytesReady(const QByteArray& sampleBytes);
    void videoImageReady(const QImage& img, quint32 sampleId);

public slots:
    void onVideoSampleBytesReceived(const QByteArray& sampleBytes);

private:
    VideoDecoder decoder_;
};


#endif //P2PPLAY_VIDEOSAMPLEPIPLINE_H
