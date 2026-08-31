//
// Created by ASUS on 2026/8/29.
//

#include "AudioPlayback.h"

#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QStringList>

namespace {
QString sampleTypeToString(QAudioFormat::SampleType type) {
    switch (type) {
        case QAudioFormat::SignedInt:
            return "SignedInt";
        case QAudioFormat::UnSignedInt:
            return "UnSignedInt";
        case QAudioFormat::Float:
            return "Float";
        case QAudioFormat::Unknown:
        default:
            return "Unknown";
    }
}

QString audioFormatToString(const QAudioFormat& format) {
    return QString("sr=%1 ch=%2 size=%3 type=%4 order=%5 codec=%6")
            .arg(format.sampleRate())
            .arg(format.channelCount())
            .arg(format.sampleSize())
            .arg(sampleTypeToString(format.sampleType()))
            .arg(format.byteOrder() == QAudioFormat::LittleEndian
                         ? "LE" : "BE")
            .arg(format.codec());
}
}

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

    QAudioFormat inputFormat;
    inputFormat.setSampleRate(sampleRate);
    inputFormat.setChannelCount(channels);
    inputFormat.setSampleSize(16);
    inputFormat.setCodec("audio/pcm");
    inputFormat.setByteOrder(QAudioFormat::LittleEndian);
    inputFormat.setSampleType(QAudioFormat::SignedInt);

    const QAudioDeviceInfo outputInfo =
            QAudioDeviceInfo::defaultOutputDevice();
    const QAudioFormat outputFormat = outputInfo.nearestFormat(inputFormat);
    emit logReceived(QString("audio playback device=%1 inputFormat=(%2) outputFormat=(%3) supportedInput=%4")
                             .arg(outputInfo.deviceName())
                             .arg(audioFormatToString(inputFormat))
                             .arg(audioFormatToString(outputFormat))
                             .arg(outputInfo.isFormatSupported(inputFormat) ? "true" : "false"));
    if (!converter_.open(inputFormat, outputFormat)) {
        emit errorOccurred(QString("failed to initialize audio converter: input=(%1) output=(%2)")
                                   .arg(audioFormatToString(inputFormat))
                                   .arg(audioFormatToString(outputFormat)));
        return false;
    }

    audioOutput_ = new QAudioOutput(outputInfo, outputFormat, this);
    const int bytesPerFrame =
            outputFormat.channelCount() * (outputFormat.sampleSize() / 8);
    audioOutput_->setBufferSize(
            outputFormat.sampleRate() * bytesPerFrame * bufferMs / 1000);
    outputDevice_ = audioOutput_->start();
    if (outputDevice_ == nullptr) {
        delete audioOutput_;
        audioOutput_ = nullptr;
        emit errorOccurred(QString("failed to start audio output: output=(%1)")
                                   .arg(audioFormatToString(outputFormat)));
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;
    bufferMs_ = bufferMs;
    pendingPcm_.clear();
    emit logReceived(QString("audio playback started: bufferMs=%1").arg(bufferMs));
    return true;
}

void AudioPlayback::stop() {
    outputDevice_ = nullptr;
    pendingPcm_.clear();
    if (audioOutput_ != nullptr) {
        audioOutput_->stop();
        delete audioOutput_;
        audioOutput_ = nullptr;
    }
    converter_.close();
    emit logReceived("audio playback stopped");
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

    const QByteArray converted = converter_.convert(pcm);
    if (converted.isEmpty()) {
        emit errorOccurred(QString("audio format convert failed: pcmBytes=%1")
                                   .arg(pcm.size()));
        return;
    }

    pendingPcm_.append(converted);
    flushPending();
}

void AudioPlayback::flushPending() {
    if (!isRunning() || pendingPcm_.isEmpty()) {
        return;
    }

    while (isRunning() && !pendingPcm_.isEmpty()) {
        const qint64 writable = audioOutput_->bytesFree();
        if (writable <= 0) {
            emit logReceived(QString("audio playback buffer full: pending=%1")
                                     .arg(pendingPcm_.size()));
            return;
        }

        const int bytesToWrite = static_cast<int>(
                qMin<qint64>(writable, pendingPcm_.size()));
        if (bytesToWrite <= 0) {
            return;
        }

        const int written = outputDevice_->write(
                pendingPcm_.constData(), bytesToWrite);
        if (written <= 0) {
            emit errorOccurred("failed to write audio output");
            return;
        }

        pendingPcm_.remove(0, written);
    }
}
