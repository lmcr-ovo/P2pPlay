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
    connect(&decoder_, &VideoDecoder::keyFrameRequestNeeded,
            this, &VideoRecevierPipline::keyFrameRequestNeeded);
}

void VideoRecevierPipline::applyConfig(const AppConfig& config) {
    decoder_.applyConfig(config);
}



void VideoRecevierPipline::onVideoSampleBytesReceived(const QByteArray& sampleBytes) {
    emit videoSampleBytesReady(sampleBytes);
}
