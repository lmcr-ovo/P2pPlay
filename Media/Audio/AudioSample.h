//
// Created by ASUS on 2026/8/27.
//

#ifndef P2PPLAY_AUDIOSAMPLE_H
#define P2PPLAY_AUDIOSAMPLE_H

#include <QtCore>

enum class AudioStreamKind : quint8 {
    Unknown = 0,
    Microphone = 1,
    Desktop = 2
};

struct AudioSample {
    quint32 seq = 0;
    quint64 captureTimeStampMs = 0;

    AudioStreamKind streamKind = AudioStreamKind::Unknown;

    quint16 sampleRate = 48000;
    quint8 channels = 1;
    quint16 frameDurationMs = 20;

    QByteArray data;
};


#endif //P2PPLAY_AUDIOSAMPLE_H
