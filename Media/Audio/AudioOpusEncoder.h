//
// Created by ASUS on 2026/8/27.
//

#ifndef P2PPLAY_AUDIOOPUSENCODER_H
#define P2PPLAY_AUDIOOPUSENCODER_H

#include <QByteArray>

#include "AudioSample.h"
#include "AppConfig.h"

struct OpusEncoder;

class AudioOpusEncoder {
public:
    ~AudioOpusEncoder();

    bool open(const OpusCodecConfig& config, AudioStreamKind kind);
    QByteArray encodePcm16(const QByteArray& pcm);
    void close();
    bool isOpen() const;

private:
    ::OpusEncoder* encoderHandle_ = nullptr;
    OpusCodecConfig config_;
    AudioStreamKind kind_ = AudioStreamKind::Unknown;
};

#endif //P2PPLAY_AUDIOOPUSENCODER_H
