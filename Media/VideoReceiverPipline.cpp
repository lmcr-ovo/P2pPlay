//
// Created by ASUS on 2026/8/5.
//

#include "VideoReceiverPipline.h"

VideoRecevierPipline::VideoRecevierPipline(QObject* parent)
    : QObject(parent),
    decoder_(this) {
    //connect(this, &VideoRecevierPipline::videoSampleBytesReady,
            //decoder_.worker(), &VideoDecoderWorker::onVideoSampleBytesReceived,
            //Qt::QueuedConnection);
    //connect(decoder_.worker(), &VideoDecoderWorker::videoImageReady,
            //this, &VideoRecevierPipline::videoImageReady,
            //Qt::QueuedConnection);
    connect(decoder_.worker(), &VideoDecoderWorker::keyFrameRequestNeeded,
            this, &VideoRecevierPipline::keyFrameRequestNeeded);
}

void VideoRecevierPipline::applyConfig(const AppConfig& config) {
    decoder_.applyConfig(config);
}

VideoDecoderWorker* VideoRecevierPipline::decoderWorker() const {
    return decoder_.worker();
}


