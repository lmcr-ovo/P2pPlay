//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_AUDIOMIXER_H
#define P2PPLAY_AUDIOMIXER_H


#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include "AvSync/AvSyncFrame.h"

class AudioMixer : public QObject {
    Q_OBJECT
public:
    explicit AudioMixer(QObject* parent = nullptr);
    ~AudioMixer() override;

    void setGains(double microphoneGain, double desktopGain);
    void setMaxQueuedFramesPerSource(int maxQueuedFrames);

    bool start(int sampleRate, int frameDurationMs);
    void stop();
    void clear();
    void pushFrame(const DecodedAudioFrame& frame);

signals:
    void mixedPcmReady(const QByteArray& pcm);
    void error(const QString& message);

private slots:
    void mixOnce();

private:
    struct PendingFrame {
        QByteArray pcm;
        int sampleRate = 48000;
        int channels = 1;
        AudioStreamKind streamKind = AudioStreamKind::Unknown;
        quint64 ptsMs = 0;
    };

    static int frameCountOf(const PendingFrame& frame);
    static qint16 readSample(const PendingFrame& frame,
                             int frameIndex,
                             int channelIndex);
    static qint16 clampToS16(double value);

    QTimer mixTimer_;
    QQueue<PendingFrame> microphoneFrames_;
    QQueue<PendingFrame> desktopFrames_;
    int sampleRate_ = 48000;
    int frameDurationMs_ = 20;
    int maxQueuedFramesPerSource_ = 10;
    double microphoneGain_ = 1.0;
    double desktopGain_ = 0.8;
    bool running_ = false;
};


#endif //P2PPLAY_AUDIOMIXER_H
