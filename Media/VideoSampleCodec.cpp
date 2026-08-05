//
// Created by ASUS on 2026/8/3.
//
#include <QDataStream>
#include "VideoSampleCodec.h"

QByteArray VideoSampleCodec::encode(const VideoSample &vSample) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << vSample.videoSeq;
    stream << vSample.captureTimeStampMs;
    stream << vSample.width;
    stream << vSample.height;
    stream << static_cast<quint8>(vSample.codec);
    stream << vSample.flags;

    bytes.append(vSample.data);
    return bytes;
}

bool VideoSampleCodec::decode(const QByteArray& bytes,
                         VideoSample& vSample) {
    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 videoSeq = 0;
    quint64 captureTimeStampMs = 0;
    quint16 width = 0;
    quint16 height = 0;
    VideoSampleCodecType codec = VideoSampleCodecType::Unknown;
    quint32 flags = 0;
    QByteArray data;

    stream >> videoSeq;
    stream >> captureTimeStampMs;
    stream >> width;
    stream >> height;
    stream >> codec;
    stream >> flags;

    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    vSample.videoSeq = videoSeq;
    vSample.captureTimeStampMs = captureTimeStampMs;
    vSample.width = width;
    vSample.height = height;
    vSample.codec = codec;
    vSample.flags = flags;
    vSample.data = bytes.mid(HeaderSize);
    return true;
}