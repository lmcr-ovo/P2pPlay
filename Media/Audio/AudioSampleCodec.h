//
// Created by ASUS on 2026/8/27.
//

#ifndef P2PPLAY_AUDIOSAMPLECODEC_H
#define P2PPLAY_AUDIOSAMPLECODEC_H

#include <QByteArray>

#include "AudioSample.h"

class AudioSampleCodec {
public:
    static QByteArray encode(const AudioSample& sample);
    static bool decode(const QByteArray& bytes, AudioSample* sample);
};

#endif // P2PPLAY_AUDIOSAMPLECODEC_H
