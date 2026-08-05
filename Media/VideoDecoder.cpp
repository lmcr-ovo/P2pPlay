//
// Created by ASUS on 2026/8/5.
//
#include <QDateTime>
#include "VideoDecoder.h"
#include "VideoSample.h"
#include "VideoSampleCodec.h"

VideoDecoder::VideoDecoder(QObject* parent)
    : QObject(parent) {
}

void VideoDecoder::onVideoSampleBytesReceived(const QByteArray &bytes) {
    VideoSample sample;
    VideoSampleCodec::decode(bytes, sample);
    switch (sample.codec) {
        case VideoSampleCodecType::Jpeg : {
            handleJpeg(sample);
            break;
        }
    }
}

void VideoDecoder::handleJpeg(VideoSample& sample) {
    if (sample.data.isEmpty()) {
        return;
    }

    QImage img;
    if (!img.loadFromData(sample.data, "JPG")) {
        return;
    }

    qDebug() << "recv sample"
             << sample.videoSeq
             << sample.width << sample.height
             << sample.data.size()
             << QDateTime::currentMSecsSinceEpoch() - sample.captureTimeStampMs << "ms";

    emit videoImageReady(img);
}