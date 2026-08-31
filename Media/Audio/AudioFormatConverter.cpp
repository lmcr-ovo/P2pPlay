//
// Created by ASUS on 2026/8/30.
//

#include "AudioFormatConverter.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

AudioFormatConverter::AudioFormatConverter() = default;

AudioFormatConverter::~AudioFormatConverter() {
    close();
}

bool AudioFormatConverter::open(const QAudioFormat& inputFormat,
                                const QAudioFormat& outputFormat) {
    close();

    if (!isSupportedFormat(inputFormat) || !isSupportedFormat(outputFormat)) {
        return false;
    }

    inputFormat_ = inputFormat;
    outputFormat_ = outputFormat;
    passthrough_ = inputFormat_ == outputFormat_;
    if (passthrough_) {
        return true;
    }

    AVChannelLayout inputLayout;
    AVChannelLayout outputLayout;
    av_channel_layout_default(&inputLayout, inputFormat_.channelCount());
    av_channel_layout_default(&outputLayout, outputFormat_.channelCount());
    const AVSampleFormat inputSampleFormat =
            static_cast<AVSampleFormat>(toSampleFormat(inputFormat_));
    const AVSampleFormat outputSampleFormat =
            static_cast<AVSampleFormat>(toSampleFormat(outputFormat_));
    if (inputSampleFormat == AV_SAMPLE_FMT_NONE ||
        outputSampleFormat == AV_SAMPLE_FMT_NONE) {
        av_channel_layout_uninit(&inputLayout);
        av_channel_layout_uninit(&outputLayout);
        close();
        return false;
    }

    if (swr_alloc_set_opts2(
            &swrContext_,
            &outputLayout,
            outputSampleFormat,
            outputFormat_.sampleRate(),
            &inputLayout,
            inputSampleFormat,
            inputFormat_.sampleRate(),
            0,
            nullptr) < 0 ||
        swrContext_ == nullptr ||
        swr_init(swrContext_) < 0) {
        av_channel_layout_uninit(&inputLayout);
        av_channel_layout_uninit(&outputLayout);
        close();
        return false;
    }

    av_channel_layout_uninit(&inputLayout);
    av_channel_layout_uninit(&outputLayout);

    return true;
}

QByteArray AudioFormatConverter::convert(const QByteArray& input) {
    if (input.isEmpty() || !isOpen()) {
        return {};
    }

    if (passthrough_) {
        return input;
    }

    const int inputFrameBytes = bytesPerFrame(inputFormat_);
    const int outputFrameBytes = bytesPerFrame(outputFormat_);
    const int outputSampleBytes = bytesPerSample(outputFormat_);
    if (inputFrameBytes <= 0 || outputFrameBytes <= 0 || outputSampleBytes <= 0 ||
        input.size() % inputFrameBytes != 0) {
        return {};
    }

    const int inputSamples = input.size() / inputFrameBytes;
    const int maxOutputSamples = static_cast<int>(
            av_rescale_rnd(
                    swr_get_delay(swrContext_, inputFormat_.sampleRate()) +
                            inputSamples,
                    outputFormat_.sampleRate(),
                    inputFormat_.sampleRate(),
                    AV_ROUND_UP));
    if (maxOutputSamples <= 0) {
        return {};
    }

    const int swrOutputSampleBytes =
            (outputFormat_.sampleType() == QAudioFormat::SignedInt &&
             outputFormat_.sampleSize() == 24)
            ? 4 : outputSampleBytes;

    QByteArray output(maxOutputSamples * outputFormat_.channelCount() * swrOutputSampleBytes,
                      Qt::Uninitialized);
    uint8_t* outData[] = { reinterpret_cast<uint8_t*>(output.data()) };
    const uint8_t* inData[] = {
            reinterpret_cast<const uint8_t*>(input.constData())
    };

    const int convertedSamples = swr_convert(
            swrContext_,
            outData,
            maxOutputSamples,
            inData,
            inputSamples);
    if (convertedSamples < 0) {
        return {};
    }

    if (outputFormat_.sampleType() == QAudioFormat::SignedInt &&
        outputFormat_.sampleSize() == 24) {
        const int channels = outputFormat_.channelCount();
        const int sampleCount = convertedSamples * channels;
        QByteArray packed(sampleCount * 3, Qt::Uninitialized);
        const quint8* src = reinterpret_cast<const quint8*>(output.constData());
        quint8* dst = reinterpret_cast<quint8*>(packed.data());
        for (int i = 0; i < sampleCount; ++i) {
            const int srcOffset = i * 4;
            const int dstOffset = i * 3;
            dst[dstOffset + 0] = src[srcOffset + 0];
            dst[dstOffset + 1] = src[srcOffset + 1];
            dst[dstOffset + 2] = src[srcOffset + 2];
        }
        return packed;
    }

    output.resize(convertedSamples * outputFrameBytes);
    return output;
}

void AudioFormatConverter::close() {
    if (swrContext_ != nullptr) {
        swr_free(&swrContext_);
    }
    inputFormat_ = QAudioFormat();
    outputFormat_ = QAudioFormat();
    passthrough_ = false;
}

bool AudioFormatConverter::isOpen() const {
    return inputFormat_.sampleRate() > 0 &&
           outputFormat_.sampleRate() > 0;
}

int AudioFormatConverter::bytesPerFrame(const QAudioFormat& format) {
    const int sampleSize = format.sampleSize();
    const int channels = format.channelCount();
    if (sampleSize <= 0 || channels <= 0) {
        return 0;
    }
    return channels * bytesPerSample(format);
}

int AudioFormatConverter::bytesPerSample(const QAudioFormat& format) {
    const int sampleSize = format.sampleSize();
    if (sampleSize == 24) {
        return 3;
    }
    if (sampleSize > 0 && sampleSize % 8 == 0) {
        return sampleSize / 8;
    }
    return 0;
}

bool AudioFormatConverter::isSupportedFormat(const QAudioFormat& format) {
    return format.sampleRate() > 0 &&
           format.channelCount() > 0 &&
           ((format.sampleType() == QAudioFormat::SignedInt &&
             (format.sampleSize() == 16 ||
              format.sampleSize() == 24 ||
              format.sampleSize() == 32)) ||
            (format.sampleType() == QAudioFormat::UnSignedInt &&
             format.sampleSize() == 8) ||
            (format.sampleType() == QAudioFormat::Float &&
             format.sampleSize() == 32));
}

int AudioFormatConverter::toSampleFormat(const QAudioFormat& format) {
    switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            if (format.sampleSize() == 16) {
                return AV_SAMPLE_FMT_S16;
            }
            if (format.sampleSize() == 24) {
                return AV_SAMPLE_FMT_S32;
            }
            if (format.sampleSize() == 32) {
                return AV_SAMPLE_FMT_S32;
            }
            break;
        case QAudioFormat::UnSignedInt:
            if (format.sampleSize() == 8) {
                return AV_SAMPLE_FMT_U8;
            }
            break;
        case QAudioFormat::Float:
            if (format.sampleSize() == 32) {
                return AV_SAMPLE_FMT_FLT;
            }
            break;
        default:
            break;
    }
    return AV_SAMPLE_FMT_NONE;
}
