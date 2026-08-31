//
// Created by ASUS on 2026/8/29.
//

#include "AudioServiceWorker.h"
#include <windows.h>

AudioServiceWorker::AudioServiceWorker(QObject* parent)
        : QObject(parent),
          microphoneSource_(this),
          desktopSource_(this),
          audioMixer_(this),
          audioPlayback_(this) {
    connect(&microphoneSource_,
            &MicrophoneAudioSource::pcmFrameReady,
            this,
            &AudioServiceWorker::onMicrophonePcmFrame);
    connect(&desktopSource_,
            &DesktopAudioSource::pcmFrameReady,
            this,
            &AudioServiceWorker::onDesktopPcmFrame);

    connect(&microphoneSource_,
            &MicrophoneAudioSource::errorOccurred,
            this,
            &AudioServiceWorker::errorOccurred);
    connect(&desktopSource_,
            &DesktopAudioSource::errorOccurred,
            this,
            &AudioServiceWorker::errorOccurred);
    connect(&audioMixer_,
            &AudioMixer::mixedPcmReady,
            this,
            [this](const QByteArray& pcm) {
                if (!running_ || !playbackEnabled_) {
                    return;
                }
                if (!audioPlayback_.isRunning() &&
                    !audioPlayback_.start(
                            config_.desktopCodec.sampleRate,
                            2,
                            config_.playbackBufferMs)) {
                    emit errorOccurred("failed to start audio playback");
                    return;
                }
                audioPlayback_.playPcm(pcm);
            });
    connect(&audioPlayback_,
            &AudioPlayback::logReceived,
            this,
            &AudioServiceWorker::logReceived);
    connect(&audioPlayback_,
            &AudioPlayback::errorOccurred,
            this,
            &AudioServiceWorker::errorOccurred);
}

AudioServiceWorker::~AudioServiceWorker() {
    stop();
}

void AudioServiceWorker::applyConfig(const AppConfig& config) {
    QString error;
    if (!OpusCodecConfig::validateOpusCodecConfig(config.audio.microphoneCodec, &error) ||
        !OpusCodecConfig::validateOpusCodecConfig(config.audio.desktopCodec, &error)) {
        emit errorOccurred(QString("invalid audio config: %1").arg(error));
        return;
    }

    const bool wasRunning = running_;
    if (wasRunning) {
        stop();
    }

    config_ = config.audio;
    microphoneEnabled_ = config_.microphoneEnabled;
    desktopAudioEnabled_ = config_.desktopAudioEnabled;
    playbackEnabled_ = config_.playbackEnabled;

    audioMixer_.setGains(config_.microphonePlaybackGain,
                         config_.desktopPlaybackGain);
    audioMixer_.setMaxQueuedFramesPerSource(
            config_.mixerMaxQueuedFramesPerSource);
    resetReorderBuffers();

    if (wasRunning) {
        start();
    }
}

void AudioServiceWorker::setRole(Role role) {
    if (running_ && role_ != role) {
        stop();
    }
    role_ = role;
}

void AudioServiceWorker::start() {
    if (running_) {
        return;
    }
    if (role_ == Role::Unknown) {
        emit errorOccurred("audio role is unknown");
        return;
    }

    running_ = true;
    if (!audioMixer_.start(config_.desktopCodec.sampleRate,
                           config_.mixerFrameDurationMs)) {
        running_ = false;
        return;
    }
    startSources();
    emit logReceived("audio service started");
}

void AudioServiceWorker::stop() {
    if (!running_) {
        stopSources();
        audioMixer_.stop();
        audioPlayback_.stop();
        return;
    }

    running_ = false;
    stopSources();
    audioMixer_.stop();
    audioPlayback_.stop();
    resetReorderBuffers();
    microphoneEncoder_.close();
    desktopEncoder_.close();
    microphoneDecoder_.close();
    desktopDecoder_.close();
    emit logReceived("audio service stopped");
}

void AudioServiceWorker::setMicrophoneEnabled(bool enabled) {
    microphoneEnabled_ = enabled;
    if (!running_) {
        return;
    }
    if (enabled) {
        microphoneSource_.start(
                config_.microphoneCodec.sampleRate,
                config_.microphoneCodec.channels,
                config_.microphoneCodec.frameDurationMs);
    } else {
        microphoneSource_.stop();
    }
}

void AudioServiceWorker::setDesktopAudioEnabled(bool enabled) {
    desktopAudioEnabled_ = enabled;
    if (!running_ || role_ != Role::Host) {
        return;
    }
    if (enabled) {
        const DWORD selfPid = GetCurrentProcessId();
        desktopSource_.start(
                config_.desktopCodec.sampleRate,
                config_.desktopCodec.channels,
                config_.desktopCodec.frameDurationMs,
                WASAPI_CAPTURE_EXCLUDE_PROCESS_TREE,
                selfPid);
    } else {
        desktopSource_.stop();
    }
}

void AudioServiceWorker::setPlaybackEnabled(bool enabled) {
    playbackEnabled_ = enabled;
    if (!enabled) {
        audioPlayback_.stop();
        audioMixer_.clear();
    }
}

void AudioServiceWorker::onMicrophonePcmFrame(
        const QByteArray& pcm,
        quint64 captureTimeStampMs) {
    if (!running_ || !microphoneEnabled_ ||
        !ensureMicrophoneEncoder()) {
        return;
    }

    const QByteArray opus = microphoneEncoder_.encodePcm16(pcm);
    if (opus.isEmpty()) {
        return;
    }

    AudioSample sample;
    sample.seq = nextMicrophoneSeq_++;
    sample.captureTimeStampMs = captureTimeStampMs;
    sample.streamKind = AudioStreamKind::Microphone;
    sample.sampleRate = static_cast<quint16>(
            config_.microphoneCodec.sampleRate);
    sample.channels = static_cast<quint8>(
            config_.microphoneCodec.channels);
    sample.frameDurationMs = static_cast<quint16>(
            config_.microphoneCodec.frameDurationMs);
    sample.data = opus;
    emit audioSampleBytesReady(AudioSampleCodec::encode(sample));
}

void AudioServiceWorker::onDesktopPcmFrame(
        const QByteArray& pcm,
        quint64 captureTimeStampMs) {
    if (!running_ || role_ != Role::Host ||
        !desktopAudioEnabled_ || !ensureDesktopEncoder()) {
        return;
    }

    const QByteArray opus = desktopEncoder_.encodePcm16(pcm);
    if (opus.isEmpty()) {
        return;
    }

    AudioSample sample;
    sample.seq = nextDesktopSeq_++;
    sample.captureTimeStampMs = captureTimeStampMs;
    sample.streamKind = AudioStreamKind::Desktop;
    sample.sampleRate = static_cast<quint16>(
            config_.desktopCodec.sampleRate);
    sample.channels = static_cast<quint8>(
            config_.desktopCodec.channels);
    sample.frameDurationMs = static_cast<quint16>(
            config_.desktopCodec.frameDurationMs);
    sample.data = opus;
    emit audioSampleBytesReady(AudioSampleCodec::encode(sample));
}

void AudioServiceWorker::onAudioSampleBytesReceived(
        const QByteArray& bytes) {
    AudioSample sample;
    if (!AudioSampleCodec::decode(bytes, &sample) ||
        !ensureDecoder(sample.streamKind)) {
        return;
    }

    AudioOpusDecoder* decoder =
            sample.streamKind == AudioStreamKind::Microphone
            ? &microphoneDecoder_
            : &desktopDecoder_;
    const QByteArray pcm = decoder->decodeToPcm16(sample.data);
    if (pcm.isEmpty()) {
        return;
    }

    const OpusCodecConfig& codecConfig =
            sample.streamKind == AudioStreamKind::Microphone
            ? config_.microphoneCodec
            : config_.desktopCodec;

    enqueueDecodedFrame(
            DecodedAudioFrame{
                    sample.seq,
                    sample.streamKind,
                    sample.captureTimeStampMs,
                    codecConfig.sampleRate,
                    codecConfig.channels,
                    pcm
            });
}

void AudioServiceWorker::onAudioFrameToPlay(
        const DecodedAudioFrame& frame) {
    if (running_ && playbackEnabled_) {
        audioMixer_.pushFrame(frame);
    }
}

bool AudioServiceWorker::ensureMicrophoneEncoder() {
    return microphoneEncoder_.isOpen() ||
           microphoneEncoder_.open(
                   config_.microphoneCodec,
                   AudioStreamKind::Microphone);
}

bool AudioServiceWorker::ensureDesktopEncoder() {
    return desktopEncoder_.isOpen() ||
           desktopEncoder_.open(
                   config_.desktopCodec,
                   AudioStreamKind::Desktop);
}

bool AudioServiceWorker::ensureDecoder(AudioStreamKind kind) {
    if (kind == AudioStreamKind::Microphone) {
        return microphoneDecoder_.isOpen() ||
               microphoneDecoder_.open(config_.microphoneCodec);
    }
    if (kind == AudioStreamKind::Desktop) {
        return desktopDecoder_.isOpen() ||
               desktopDecoder_.open(config_.desktopCodec);
    }
    return false;
}

void AudioServiceWorker::enqueueDecodedFrame(
        const DecodedAudioFrame& frame) {
    QMap<quint32, DecodedAudioFrame>* buffer = nullptr;
    quint32* nextSeq = nullptr;
    bool* seqValid = nullptr;

    if (frame.streamKind == AudioStreamKind::Microphone) {
        buffer = &microphoneReorderBuffer_;
        nextSeq = &nextMicrophonePlaybackSeq_;
        seqValid = &microphonePlaybackSeqValid_;
    } else if (frame.streamKind == AudioStreamKind::Desktop) {
        buffer = &desktopReorderBuffer_;
        nextSeq = &nextDesktopPlaybackSeq_;
        seqValid = &desktopPlaybackSeqValid_;
    }

    if (buffer == nullptr || nextSeq == nullptr || seqValid == nullptr) {
        return;
    }

    if (!*seqValid) {
        *nextSeq = frame.seq;
        *seqValid = true;
    }

    if (frame.seq < *nextSeq || buffer->contains(frame.seq)) {
        return;
    }

    buffer->insert(frame.seq, frame);
    drainReorderBuffer(frame.streamKind);
}

void AudioServiceWorker::drainReorderBuffer(AudioStreamKind kind) {
    QMap<quint32, DecodedAudioFrame>* buffer = nullptr;
    quint32* nextSeq = nullptr;
    bool* seqValid = nullptr;

    if (kind == AudioStreamKind::Microphone) {
        buffer = &microphoneReorderBuffer_;
        nextSeq = &nextMicrophonePlaybackSeq_;
        seqValid = &microphonePlaybackSeqValid_;
    } else if (kind == AudioStreamKind::Desktop) {
        buffer = &desktopReorderBuffer_;
        nextSeq = &nextDesktopPlaybackSeq_;
        seqValid = &desktopPlaybackSeqValid_;
    }

    if (buffer == nullptr || nextSeq == nullptr || seqValid == nullptr ||
        !*seqValid) {
        return;
    }

    while (!buffer->isEmpty()) {
        if (buffer->contains(*nextSeq)) {
            const DecodedAudioFrame frame = buffer->take(*nextSeq);
            emit decodedAudioFrameReady(frame);
            ++(*nextSeq);
            continue;
        }

        const int maxBufferedFrames =
                qMax(1, config_.audioReorderMaxBufferedFrames);
        if (buffer->size() < maxBufferedFrames) {
            break;
        }

        const quint32 firstAvailableSeq = buffer->firstKey();
        if (firstAvailableSeq > *nextSeq) {
            emit logReceived(QString("audio packet lost: stream=%1 missingSeq=%2 nextSeq=%3")
                                     .arg(static_cast<int>(kind))
                                     .arg(*nextSeq)
                                     .arg(firstAvailableSeq));
            *nextSeq = firstAvailableSeq;
        } else {
            buffer->remove(firstAvailableSeq);
        }
    }
}

void AudioServiceWorker::resetReorderBuffers() {
    microphoneReorderBuffer_.clear();
    desktopReorderBuffer_.clear();
    nextMicrophonePlaybackSeq_ = 0;
    nextDesktopPlaybackSeq_ = 0;
    microphonePlaybackSeqValid_ = false;
    desktopPlaybackSeqValid_ = false;
}

void AudioServiceWorker::startSources() {
    if (microphoneEnabled_) {
        microphoneSource_.start(
                config_.microphoneCodec.sampleRate,
                config_.microphoneCodec.channels,
                config_.microphoneCodec.frameDurationMs);
    }

    if (role_ == Role::Host && desktopAudioEnabled_) {
        const DWORD selfPid = GetCurrentProcessId();
        desktopSource_.start(
                config_.desktopCodec.sampleRate,
                config_.desktopCodec.channels,
                config_.desktopCodec.frameDurationMs,
                WASAPI_CAPTURE_EXCLUDE_PROCESS_TREE,
                selfPid);
    }
}

void AudioServiceWorker::stopSources() {
    microphoneSource_.stop();
    desktopSource_.stop();
}
