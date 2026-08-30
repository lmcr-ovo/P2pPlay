//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_MICROPHONEAUDIOSOURCE_H
#define P2PPLAY_MICROPHONEAUDIOSOURCE_H

#include <QAudioInput>
#include <QByteArray>
#include <QIODevice>
#include <QObject>

class MicrophoneAudioSource : public QObject {
Q_OBJECT
public:
    explicit MicrophoneAudioSource(QObject* parent = nullptr);
    ~MicrophoneAudioSource() override;

    bool start(int sampleRate, int channels, int frameDurationMs);
    void stop();
    bool isRunning() const;

signals:
    void pcmFrameReady(const QByteArray& pcm, quint64 captureTimeStampMs);
    void errorOccurred(const QString& reason);

private slots:
    void onReadyRead();

private:
    QAudioFormat buildFormat(int sampleRate, int channels) const;
    void emitCompleteFrames();

    QAudioInput* audioInput_ = nullptr;
    QIODevice* inputDevice_ = nullptr;
    QByteArray pendingPcm_;
    int sampleRate_ = 48000;
    int channels_ = 1;
    int frameDurationMs_ = 20;
    int frameBytes_ = 0;
    bool running_ = false;
};


#endif //P2PPLAY_MICROPHONEAUDIOSOURCE_H
