//
// Created by ASUS on 2026/8/2.
//

#include "VideoCapturer.h"
#include "AppConfig.h"

VideoCapturer::VideoCapturer(QObject* parent)
    : QObject(parent),
    timer_(this) {
    connect(&timer_, &QTimer::timeout, this, [this]() {
        emit videoFrameReady(QByteArray("hello"));
    });
}

void VideoCapturer::applyConfig(const VideoConfig& config) {
    frameIntervalMs_ = config.frameIntervalMs();
    width_ = config.width;
    height_ = config.height;
    jpegQuality_ = config.jpegQuality;
}

void VideoCapturer::onP2pReady(const QHostAddress &address, quint16 port) {
    timer_.setInterval(frameIntervalMs_);
    if (!timer_.isActive()) {
        timer_.start();
    }
}