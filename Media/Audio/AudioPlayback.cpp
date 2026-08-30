//
// Created by ASUS on 2026/8/29.
//

#include "AudioPlayback.h"

#include <QAudioDeviceInfo>
#include <QAudioFormat>

AudioPlayback::AudioPlayback(QObject* parent)
        : QObject(parent) {
}

AudioPlayback::~AudioPlayback() {
    stop();
}

bool AudioPlayback::start(int sampleRate, int channels, int bufferMs) {
    stop();

    if (sampleRate <= 0 ||
        (channels != 1 && channels != 2) ||
        bufferMs <= 0) {
        emit errorOccurred("invalid playback format");
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    const QAudioDeviceInfo outputInfo =
            QAudioDeviceInfo::defaultOutputDevice();
    if (!outputInfo.isFormatSupported(format)) {
        emit errorOccurred("playback format is not supported");
        return false;
    }

    audioOutput_ = new QAudioOutput(outputInfo, format, this);
    audioOutput_->setBufferSize(
            sampleRate * channels * 2 * bufferMs / 1000);
    outputDevice_ = audioOutput_->start();
    if (outputDevice_ == nullptr) {
        delete audioOutput_;
        audioOutput_ = nullptr;
        emit errorOccurred("failed to start audio output");
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;
    bufferMs_ = bufferMs;
    return true;
}

void AudioPlayback::stop() {
    outputDevice_ = nullptr;
    if (audioOutput_ != nullptr) {
        audioOutput_->stop();
        delete audioOutput_;
        audioOutput_ = nullptr;
    }
}

bool AudioPlayback::isRunning() const {
    return audioOutput_ != nullptr &&
           outputDevice_ != nullptr &&
           audioOutput_->state() != QAudio::StoppedState;
}

void AudioPlayback::playPcm(const QByteArray& pcm) {
    if (!isRunning() || pcm.isEmpty()) {
        return;
    }

    const qint64 writable = audioOutput_->bytesFree();
    if (writable <= 0) {
        return;
    }

    const int bytesToWrite = static_cast<int>(
            qMin<qint64>(writable, pcm.size()));
        if (outputDevice_->write(pcm.constData(), bytesToWrite) != bytesToWrite) {
        emit errorOccurred("failed to write audio output");
    }
}