//
// Created by ASUS on 2026/8/29.
//

#include "AudioOpusDecoder.h"

#include <opus/opus.h>

AudioOpusDecoder::~AudioOpusDecoder() {
    close();
}

bool AudioOpusDecoder::open(const OpusCodecConfig& config) {
    close();

    if ((config.sampleRate != 8000 &&
         config.sampleRate != 12000 &&
         config.sampleRate != 16000 &&
         config.sampleRate != 24000 &&
         config.sampleRate != 48000) ||
        (config.channels != 1 && config.channels != 2)) {
        return false;
    }

    int error = OPUS_OK;
    decoderHandle_ = ::opus_decoder_create(
            config.sampleRate,
            config.channels,
            &error);
    if (decoderHandle_ == nullptr || error != OPUS_OK) {
        close();
        return false;
    }

    config_ = config;
    return true;
}

QByteArray AudioOpusDecoder::decodeToPcm16(const QByteArray& opusBytes) {
    if (!isOpen() || opusBytes.isEmpty()) {
        return {};
    }

    const int maxFrameSamples =
            config_.sampleRate * 120 / 1000;
    QByteArray pcm(maxFrameSamples * config_.channels *
                   static_cast<int>(sizeof(opus_int16)),
                   Qt::Uninitialized);

    const int decodedSamples = ::opus_decode(
            decoderHandle_,
            reinterpret_cast<const unsigned char*>(opusBytes.constData()),
            opusBytes.size(),
            reinterpret_cast<opus_int16*>(pcm.data()),
            maxFrameSamples,
            0);
    if (decodedSamples <= 0) {
        return {};
    }

    pcm.resize(decodedSamples * config_.channels *
               static_cast<int>(sizeof(opus_int16)));
    return pcm;
}

void AudioOpusDecoder::close() {
    if (decoderHandle_ != nullptr) {
        ::opus_decoder_destroy(decoderHandle_);
        decoderHandle_ = nullptr;
    }
}

bool AudioOpusDecoder::isOpen() const {
    return decoderHandle_ != nullptr;
}