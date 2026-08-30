//
// Created by ASUS on 2026/8/27.
//

#include "AudioSampleCodec.h"

#include <QDataStream>

namespace {
    constexpr quint32 kMagic = 0x50324155; // P2AU
    constexpr quint8 kVersion = 1;
}

QByteArray AudioSampleCodec::encode(const AudioSample& sample)
{
    if (sample.data.isEmpty() ||
        sample.streamKind == AudioStreamKind::Unknown ||
        sample.sampleRate == 0 ||
        sample.channels == 0 ||
        sample.frameDurationMs == 0) {
        return {};
    }

    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_14);

    stream << kMagic
           << kVersion
           << static_cast<quint8>(sample.streamKind)
           << sample.seq
           << sample.captureTimeStampMs
           << sample.sampleRate
           << sample.channels
           << sample.frameDurationMs
           << static_cast<quint32>(sample.data.size());
    stream.writeRawData(sample.data.constData(), sample.data.size());
    return stream.status() == QDataStream::Ok ? bytes : QByteArray();
}

bool AudioSampleCodec::decode(const QByteArray& bytes, AudioSample* sample)
{
    if (sample == nullptr || bytes.isEmpty()) {
        return false;
    }

    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_5_14);

    quint32 magic = 0;
    quint8 version = 0;
    quint8 streamKind = 0;
    quint32 seq = 0;
    quint64 ptsMs = 0;
    quint16 sampleRate = 0;
    quint8 channels = 0;
    quint16 frameDurationMs = 0;
    quint32 payloadSize = 0;

    stream >> magic
           >> version
           >> streamKind
           >> seq
           >> ptsMs
           >> sampleRate
           >> channels
           >> frameDurationMs
           >> payloadSize;

    constexpr int kHeaderBytes =
            sizeof(quint32) + sizeof(quint8) + sizeof(quint8) +
            sizeof(quint32) + sizeof(quint64) + sizeof(quint16) +
            sizeof(quint8) + sizeof(quint16) + sizeof(quint32);

    if (stream.status() != QDataStream::Ok ||
        bytes.size() < kHeaderBytes ||
        magic != kMagic ||
        version != kVersion ||
        payloadSize == 0 ||
        payloadSize != static_cast<quint32>(bytes.size() - kHeaderBytes) ||
        (streamKind != static_cast<quint8>(AudioStreamKind::Microphone) &&
         streamKind != static_cast<quint8>(AudioStreamKind::Desktop)) ||
        sampleRate == 0 ||
        (channels != 1 && channels != 2) ||
        frameDurationMs == 0) {
        return false;
    }

    QByteArray payload(static_cast<int>(payloadSize), Qt::Uninitialized);
    if (stream.readRawData(payload.data(), payload.size()) != payload.size()) {
        return false;
    }

    AudioSample result;
    result.seq = seq;
    result.captureTimeStampMs = ptsMs;
    result.streamKind = static_cast<AudioStreamKind>(streamKind);
    result.sampleRate = sampleRate;
    result.channels = channels;
    result.frameDurationMs = frameDurationMs;
    result.data = payload;
    *sample = result;
    return true;
}