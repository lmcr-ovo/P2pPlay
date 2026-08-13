//
// Created by ASUS on 2026/8/4.
//

#include "VideoSenderPipeline.h"

VideoSenderPipeline::VideoSenderPipeline(QObject* parent)
    : QObject(parent),
    videoSource_(this),
    encoder_(this) {
    /*
    connect(&videoSource_, &ScreenVideoSource::videoImageReady,
            &encoder_, &VideoEncoder::onVideoImageReady);
    connect(&encoder_, &VideoEncoder::videoSampleBytesReady,
            this, &VideoSenderPipeline::videoSampleBytesReady);
            */
}

void VideoSenderPipeline::applyConfig(const AppConfig &config) {
    videoSource_.applyConfig(config);
    encoder_.applyConfig(config);
}

VideoEncoderWorker* VideoSenderPipeline::encoderWorker() const {
    return encoder_.worker();
}

ScreenVideoSource* VideoSenderPipeline::screenVideoSource() {
    return &videoSource_;
}

void VideoSenderPipeline::start() {
    videoSource_.start();
}

void VideoSenderPipeline::stop() {
    videoSource_.stop();
}