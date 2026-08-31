//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_AUDIOPLAYBACK_H
#define P2PPLAY_AUDIOPLAYBACK_H

#include <QAudioOutput>
#include <QByteArray>
#include <QObject>

#include "AudioFormatConverter.h"

class AudioPlayback : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayback(QObject* parent = nullptr);
    ~AudioPlayback() override;

    bool start(int sampleRate, int channels, int bufferMs);
    void stop();
    bool isRunning() const;
    void playPcm(const QByteArray& pcm);

signals:
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    void flushPending();

    QAudioOutput* audioOutput_ = nullptr;
    QIODevice* outputDevice_ = nullptr;
    AudioFormatConverter converter_;
    QByteArray pendingPcm_;
    int sampleRate_ = 48000;
    int channels_ = 2;
    int bufferMs_ = 80;
};

#endif //P2PPLAY_AUDIOPLAYBACK_H
