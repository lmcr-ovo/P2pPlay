//
// Created by ASUS on 2026/8/29.
//

#include "AudioMixer.h"

#include <algorithm>
#include <cmath>
#include <limits>

AudioMixer::AudioMixer(QObject* parent)
        : QObject(parent),
        mixTimer_(this) {
    mixTimer_.setTimerType(Qt::PreciseTimer);
    connect(&mixTimer_, &QTimer::timeout,
            this, &AudioMixer::mixOnce);
}

AudioMixer::~AudioMixer() {
    stop();
}

void AudioMixer::setGains(double microphoneGain, double desktopGain) {
    microphoneGain_ = std::max(0.0, microphoneGain);
    desktopGain_ = std::max(0.0, desktopGain);
}

void AudioMixer::setMaxQueuedFramesPerSource(int maxQueuedFrames) {
    maxQueuedFramesPerSource_ = std::max(1, maxQueuedFrames);
}

bool AudioMixer::start(int sampleRate, int frameDurationMs) {
    if (sampleRate <= 0 || frameDurationMs <= 0) {
        emit error("invalid mixer format");
        return false;
    }

    sampleRate_ = sampleRate;
    frameDurationMs_ = frameDurationMs;
    clear();
    running_ = true;
    mixTimer_.start(frameDurationMs_);
    return true;
}

void AudioMixer::stop() {
    mixTimer_.stop();
    running_ = false;
    clear();
}

void AudioMixer::clear() {
    microphoneFrames_.clear();
    desktopFrames_.clear();
}

void AudioMixer::pushFrame(const DecodedAudioFrame& frame) {
    if (!running_ || frame.pcm.isEmpty() ||
        frame.sampleRate != sampleRate_ ||
        (frame.channels != 1 && frame.channels != 2)) {
        return;
    }

    PendingFrame pending;
    pending.pcm = frame.pcm;
    pending.sampleRate = frame.sampleRate;
    pending.channels = frame.channels;
    pending.streamKind = frame.streamKind;
    pending.ptsMs = frame.ptsMs;

    QQueue<PendingFrame>* queue = nullptr;
    if (frame.streamKind == AudioStreamKind::Microphone) {
        queue = &microphoneFrames_;
    } else if (frame.streamKind == AudioStreamKind::Desktop) {
        queue = &desktopFrames_;
    }
    if (queue == nullptr) {
        return;
    }

    queue->enqueue(pending);
    while (queue->size() > maxQueuedFramesPerSource_) {
        queue->dequeue();
    }
}

void AudioMixer::mixOnce() {
    if (!running_ ||
        (microphoneFrames_.isEmpty() && desktopFrames_.isEmpty())) {
        return;
    }

    PendingFrame microphone;
    PendingFrame desktop;
    const PendingFrame* microphonePtr = nullptr;
    const PendingFrame* desktopPtr = nullptr;

    if (!microphoneFrames_.isEmpty()) {
        microphone = microphoneFrames_.dequeue();
        microphonePtr = &microphone;
    }
    if (!desktopFrames_.isEmpty()) {
        desktop = desktopFrames_.dequeue();
        desktopPtr = &desktop;
    }

    const int microphoneCount = microphonePtr
                                ? frameCountOf(*microphonePtr) : 0;
    const int desktopCount = desktopPtr
                             ? frameCountOf(*desktopPtr) : 0;
    const int frameCount = std::max(microphoneCount, desktopCount);
    if (frameCount <= 0) {
        return;
    }

    QByteArray output(frameCount * 2 * static_cast<int>(sizeof(qint16)),
                      Qt::Uninitialized);
    auto* destination = reinterpret_cast<qint16*>(output.data());
    for (int i = 0; i < frameCount; ++i) {
        for (int channel = 0; channel < 2; ++channel) {
            double value = 0.0;
            if (microphonePtr) {
                value += readSample(*microphonePtr, i, channel) *
                         microphoneGain_;
            }
            if (desktopPtr) {
                value += readSample(*desktopPtr, i, channel) *
                         desktopGain_;
            }
            destination[i * 2 + channel] = clampToS16(value);
        }
    }

    emit mixedPcmReady(output);
}

int AudioMixer::frameCountOf(const PendingFrame& frame)
{
    if (frame.channels <= 0) {
        return 0;
    }
    return frame.pcm.size() /
           static_cast<int>(sizeof(qint16)) /
           frame.channels;
}

qint16 AudioMixer::readSample(const PendingFrame& frame,
                              int frameIndex,
                              int channelIndex) {
    const int frameCount = frameCountOf(frame);
    if (frameIndex < 0 || frameIndex >= frameCount) {
        return 0;
    }

    const auto* samples =
            reinterpret_cast<const qint16*>(frame.pcm.constData());
    if (frame.channels == 1) {
        return samples[frameIndex];
    }

    const int channel = std::max(0, std::min(channelIndex,
                                             frame.channels - 1));
    return samples[frameIndex * frame.channels + channel];
}

qint16 AudioMixer::clampToS16(double value) {
    const double minimum =
            static_cast<double>(std::numeric_limits<qint16>::min());
    const double maximum =
            static_cast<double>(std::numeric_limits<qint16>::max());
    return static_cast<qint16>(std::lrint(
            std::max(minimum, std::min(maximum, value))));
}