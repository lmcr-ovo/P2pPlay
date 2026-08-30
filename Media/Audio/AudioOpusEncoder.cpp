//
// Created by ASUS on 2026/8/27.
//

#include "AudioOpusEncoder.h"

#include <opus/opus.h>

AudioOpusEncoder::~AudioOpusEncoder() {
    close();
}

bool AudioOpusEncoder::open(const OpusCodecConfig& config,
                            AudioStreamKind kind) {
    close();

    if ((config.sampleRate != 8000 &&
         config.sampleRate != 12000 &&
         config.sampleRate != 16000 &&
         config.sampleRate != 24000 &&
         config.sampleRate != 48000) ||
        (config.channels != 1 && config.channels != 2) ||
        config.frameDurationMs <= 0 ||
        config.bitrateBps <= 0 ||
        config.maxPacketBytes <= 0 ||
        kind == AudioStreamKind::Unknown) {
        return false;
    }

    int error = OPUS_OK;
    encoderHandle_ = ::opus_encoder_create(
            config.sampleRate,
            config.channels,
            OPUS_APPLICATION_AUDIO,
            &error);
    if (encoderHandle_ == nullptr || error != OPUS_OK) {
        close();
        return false;
    }

    if (::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_BITRATE(config.bitrateBps)) != OPUS_OK ||
        ::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_COMPLEXITY(config.complexity)) != OPUS_OK ||
        ::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_INBAND_FEC(
                                   config.inbandFecEnabled ? 1 : 0)) != OPUS_OK ||
        ::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_DTX(config.dtxEnabled ? 1 : 0)) != OPUS_OK) {
        close();
        return false;
    }

    config_ = config;
    kind_ = kind;
    return true;
}

QByteArray AudioOpusEncoder::encodePcm16(const QByteArray& pcm) {
    if (!isOpen()) {
        return {};
    }

    const int frameSamples =
            config_.sampleRate * config_.frameDurationMs / 1000;
    const int expectedBytes =
            frameSamples * config_.channels * static_cast<int>(sizeof(opus_int16));
    if (frameSamples <= 0 || pcm.size() != expectedBytes) {
        return {};
    }

    QByteArray encoded(config_.maxPacketBytes, Qt::Uninitialized);
    const int encodedBytes = ::opus_encode(
            encoderHandle_,
            reinterpret_cast<const opus_int16*>(pcm.constData()),
            frameSamples,
            reinterpret_cast<unsigned char*>(encoded.data()),
            encoded.size());
    if (encodedBytes <= 0) {
        return {};
    }

    encoded.resize(encodedBytes);
    return encoded;
}

void AudioOpusEncoder::close() {
    if (encoderHandle_ != nullptr) {
        ::opus_encoder_destroy(encoderHandle_);
        encoderHandle_ = nullptr;
    }
    kind_ = AudioStreamKind::Unknown;
}

bool AudioOpusEncoder::isOpen() const {
    return encoderHandle_ != nullptr;
}