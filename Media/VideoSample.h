//
// Created by ASUS on 2026/8/3.
//

#ifndef P2PPLAY_VIDEOSAMPLE_H
#define P2PPLAY_VIDEOSAMPLE_H

#include <QtCore>

enum class VideoSampleCodecType : quint8 {
    Unknown = 0,
    Jpeg = 1,
    H264 = 2
};

enum VideoSampleFlags : quint32 {
    VideoSampleFlag_None = 0,
    VideoSampleFlag_KeyFrame = 1u << 0
};

struct VideoSample {
    quint32 videoSeq = 0;
    quint64 captureTimeStampMs = 0;
    quint16 width = 0;
    quint16 height = 0;
    VideoSampleCodecType codec = VideoSampleCodecType::Unknown;
    quint32 flags = 0;
    QByteArray data;
};

#endif //P2PPLAY_VIDEOSAMPLE_H
