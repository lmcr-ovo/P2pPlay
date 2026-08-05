//
// Created by ASUS on 2026/8/5.
//

#include "VideoReceiverPipline.h"

VideoRecevierPipline::VideoRecevierPipline(QObject* parent)
    : QObject(parent),
    decoder_(this) {
    connect(this, &VideoRecevierPipline::videoSampleBytesReady,
            &decoder_, &VideoDecoder::onVideoSampleBytesReceived);
    connect(&decoder_, &VideoDecoder::videoImageReady,
            this, &VideoRecevierPipline::videoImageReady);
}

void VideoRecevierPipline::onVideoSampleBytesReceived(const QByteArray& sampleBytes) {
    emit videoSampleBytesReady(sampleBytes);
}