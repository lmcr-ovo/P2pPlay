//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_AUDIOOPUSDECODER_H
#define P2PPLAY_AUDIOOPUSDECODER_H

#include <QByteArray>

#include "AppConfig.h"

struct OpusDecoder;

class AudioOpusDecoder {
public:
    ~AudioOpusDecoder();

    bool open(const OpusCodecConfig& config);
    QByteArray decodeToPcm16(const QByteArray& opusBytes);
    void close();
    bool isOpen() const;

private:
    ::OpusDecoder* decoderHandle_ = nullptr;
    OpusCodecConfig config_;
};

#endif //P2PPLAY_AUDIOOPUSDECODER_H
