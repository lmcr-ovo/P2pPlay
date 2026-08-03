//
// Created by ASUS on 2026/8/2.
//

#include "VideoCapturer.h"

VideoCapturer::VideoCapturer(QObject* parent)
    : QObject(parent),
    timer_(this) {
    connect(&timer_, &QTimer::timeout, this, [this]() {
        emit videoFrameReady(QByteArray("hello"));
    });
}

void VideoCapturer::onP2pReady(const QHostAddress &address, quint16 port) {
    timer_.setInterval(500);
    if (!timer_.isActive()) {
        timer_.start();
    }
}