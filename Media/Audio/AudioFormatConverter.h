//
// Created by ASUS on 2026/8/30.
//

#ifndef P2PPLAY_AUDIOFORMATCONVERTER_H
#define P2PPLAY_AUDIOFORMATCONVERTER_H

#include <QAudioFormat>
#include <QByteArray>

struct SwrContext;

class AudioFormatConverter {
public:
    AudioFormatConverter();
    ~AudioFormatConverter();

    bool open(const QAudioFormat& inputFormat,
              const QAudioFormat& outputFormat);
    QByteArray convert(const QByteArray& input);
    void close();
    bool isOpen() const;

private:
    static int bytesPerFrame(const QAudioFormat& format);
    static int bytesPerSample(const QAudioFormat& format);
    static bool isSupportedFormat(const QAudioFormat& format);
    static int toSampleFormat(const QAudioFormat& format);

private:
    SwrContext* swrContext_ = nullptr;
    QAudioFormat inputFormat_;
    QAudioFormat outputFormat_;
    bool passthrough_ = false;
};

#endif //P2PPLAY_AUDIOFORMATCONVERTER_H
