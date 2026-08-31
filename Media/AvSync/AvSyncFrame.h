//
// Created by ASUS on 2026/8/27.
//

#ifndef P2PPLAY_AVSYNCFRAME_H
#define P2PPLAY_AVSYNCFRAME_H

#include <QImage>
#include "Audio/AudioSample.h"

struct DecodedAudioFrame {
    quint32 seq = 0;
    AudioStreamKind streamKind = AudioStreamKind::Unknown;
    quint64 ptsMs = 0;
    int sampleRate = 48000;
    int channels = 1;
    QByteArray pcm;
};

struct DecodedVideoFrame {
    quint64 ptsMs = 0;
    int width = 0;
    int height = 0;
    QImage image;
};

Q_DECLARE_METATYPE(DecodedVideoFrame)
Q_DECLARE_METATYPE(DecodedAudioFrame)

#endif //P2PPLAY_AVSYNCFRAME_H
