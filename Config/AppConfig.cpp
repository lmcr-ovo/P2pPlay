//
// Created by ASUS on 2026/8/3.
//

#include "AppConfig.h"

AppConfig AppConfig::defaultHost() {
    AppConfig config;
    config.p2p.localUdpPort = 10000;
    return config;
};

AppConfig AppConfig::defaultGuest() {
    AppConfig config;
    config.p2p.localUdpPort = 10001;
    return config;
}

bool OpusCodecConfig::validateOpusCodecConfig(const OpusCodecConfig& config,
                                    QString* errorMessage)
{
    if (config.sampleRate != 8000 &&
        config.sampleRate != 12000 &&
        config.sampleRate != 16000 &&
        config.sampleRate != 24000 &&
        config.sampleRate != 48000) {
        if (errorMessage) *errorMessage = "unsupported sample rate";
        return false;
    }

    if (config.channels != 1 && config.channels != 2) {
        if (errorMessage) *errorMessage = "channels must be 1 or 2";
        return false;
    }

    if (config.frameDurationMs != 5 &&
        config.frameDurationMs != 10 &&
        config.frameDurationMs != 20 &&
        config.frameDurationMs != 40 &&
        config.frameDurationMs != 60) {
        if (errorMessage) *errorMessage = "unsupported frame duration";
        return false;
    }

    if (config.bitrateBps <= 0 ||
        config.complexity < 0 ||
        config.complexity > 10 ||
        config.maxPacketBytes <= 0) {
        if (errorMessage) *errorMessage = "invalid opus config";
        return false;
    }

    return true;
}