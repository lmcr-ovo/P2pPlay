//
// Created by ASUS on 2026/8/3.
//

#ifndef P2PPLAY_VIDEOSAMPLECODEC_H
#define P2PPLAY_VIDEOSAMPLECODEC_H

#include <QByteArray>
#include "VideoSample.h"

class VideoSampleCodec {
public:
    static QByteArray encode(const VideoSample& vSample);
    static bool decode(const QByteArray& bytes, VideoSample& vSample);
    static bool peekVideoSeq(const QByteArray& bytes, quint32& videoSeq);

private:
    static constexpr int HeaderSize =
            sizeof(quint32) + // videoSeq
            sizeof(quint64) + // captureTimeStampMs
            sizeof(quint16) + // width
            sizeof(quint16) + // height
            sizeof(quint8) +  // codec
            sizeof(quint32); // flags
};


#endif //P2PPLAY_VIDEOSAMPLE_H
