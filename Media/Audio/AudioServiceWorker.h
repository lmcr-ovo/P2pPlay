//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_AUDIOSERVICEWORKER_H
#define P2PPLAY_AUDIOSERVICEWORKER_H


#include <QObject>

#include "AppConfig.h"
#include "Role.h"
#include "AudioSample.h"
#include "AudioSampleCodec.h"
#include "AudioOpusEncoder.h"
#include "AudioOpusDecoder.h"
#include "MicrophoneAudioSource.h"
#include "DesktopAudioSource.h"
#include "AudioMixer.h"
#include "AudioPlayback.h"
#include "AvSync/AvSyncFrame.h"

class AudioServiceWorker : public QObject {
Q_OBJECT
public:
    explicit AudioServiceWorker(QObject* parent = nullptr);
    ~AudioServiceWorker() override;

public slots:
    void applyConfig(const AppConfig& config);
    void setRole(Role role);
    void start();
    void stop();

    void setMicrophoneEnabled(bool enabled);
    void setDesktopAudioEnabled(bool enabled);
    void setPlaybackEnabled(bool enabled);

    void onAudioSampleBytesReceived(const QByteArray& bytes);
    void onAudioFrameToPlay(const DecodedAudioFrame& frame);

signals:
    void audioSampleBytesReady(const QByteArray& bytes);
    void decodedAudioFrameReady(const DecodedAudioFrame& frame);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private slots:
    void onMicrophonePcmFrame(const QByteArray& pcm,
                              quint64 captureTimeStampMs);
    void onDesktopPcmFrame(const QByteArray& pcm,
                           quint64 captureTimeStampMs);

private:
    bool ensureMicrophoneEncoder();
    bool ensureDesktopEncoder();
    bool ensureDecoder(AudioStreamKind kind);
    void startSources();
    void stopSources();

    Role role_ = Role::Unknown;
    bool running_ = false;
    bool microphoneEnabled_ = true;
    bool desktopAudioEnabled_ = true;
    bool playbackEnabled_ = true;

    AudioConfig config_;
    quint32 nextMicrophoneSeq_ = 0;
    quint32 nextDesktopSeq_ = 0;

    MicrophoneAudioSource microphoneSource_;
    DesktopAudioSource desktopSource_;
    AudioOpusEncoder microphoneEncoder_;
    AudioOpusEncoder desktopEncoder_;
    AudioOpusDecoder microphoneDecoder_;
    AudioOpusDecoder desktopDecoder_;
    AudioMixer audioMixer_;
    AudioPlayback audioPlayback_;
};
#endif //P2PPLAY_AUDIOSERVICEWORKER_H
