//
// Created by ASUS on 2026/8/29.
//

#include "MicrophoneAudioSource.h"

#include <QAudioDeviceInfo>
#include <QDateTime>

MicrophoneAudioSource::MicrophoneAudioSource(QObject* parent)
        : QObject(parent) {
}

MicrophoneAudioSource::~MicrophoneAudioSource() {
    stop();
}

bool MicrophoneAudioSource::start(int sampleRate,
                                  int channels,
                                  int frameDurationMs) {
    stop();

    if (sampleRate <= 0 || channels != 1 || frameDurationMs != 20) {
        emit errorOccurred("invalid microphone format");
        return false;
    }

    const QAudioFormat format = buildFormat(sampleRate, channels);
    const QAudioDeviceInfo inputInfo =
            QAudioDeviceInfo::defaultInputDevice();
    if (!inputInfo.isFormatSupported(format)) {
        emit errorOccurred("microphone format is not supported");
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;
    frameDurationMs_ = frameDurationMs;
    frameBytes_ = sampleRate_ * frameDurationMs_ / 1000 *
                  channels_ * static_cast<int>(sizeof(qint16));
    pendingPcm_.clear();

    audioInput_ = new QAudioInput(inputInfo, format, this);
    inputDevice_ = audioInput_->start();
    if (inputDevice_ == nullptr) {
        delete audioInput_;
        audioInput_ = nullptr;
        emit errorOccurred("failed to start microphone");
        return false;
    }

    connect(inputDevice_, &QIODevice::readyRead,
            this, &MicrophoneAudioSource::onReadyRead);
    running_ = true;
    return true;
}

void MicrophoneAudioSource::stop() {
    running_ = false;
    pendingPcm_.clear();
    inputDevice_ = nullptr;

    if (audioInput_ != nullptr) {
        audioInput_->stop();
        delete audioInput_;
        audioInput_ = nullptr;
    }
}

bool MicrophoneAudioSource::isRunning() const {
    return running_;
}

void MicrophoneAudioSource::onReadyRead() {
    if (!running_ || inputDevice_ == nullptr) {
        return;
    }

    pendingPcm_.append(inputDevice_->readAll());
    emitCompleteFrames();
}

QAudioFormat MicrophoneAudioSource::buildFormat(int sampleRate,
                                                int channels) const {
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
    return format;
}

void MicrophoneAudioSource::emitCompleteFrames() {
    while (frameBytes_ > 0 && pendingPcm_.size() >= frameBytes_) {
        const QByteArray frame = pendingPcm_.left(frameBytes_);
        pendingPcm_.remove(0, frameBytes_);
        emit pcmFrameReady(
                frame,
                static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
    }
}