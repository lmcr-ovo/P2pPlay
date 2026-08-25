//
// Created by ASUS on 2026/8/5.
//

#include "VideoReceiverPipeline.h"

VideoReceiverPipeline::VideoReceiverPipeline(QObject* parent)
    : QObject(parent),
    decoder_(this) {
}

void VideoReceiverPipeline::applyConfig(const AppConfig& config) {
    decoder_.applyConfig(config);
}

VideoDecoderWorker* VideoReceiverPipeline::decoderWorker() const {
    return decoder_.worker();
}


