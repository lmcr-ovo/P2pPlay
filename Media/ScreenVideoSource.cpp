//
// Created by ASUS on 2026/8/4.
//
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QImage>
#include "ScreenVideoSource.h"

ScreenVideoSource::ScreenVideoSource(QObject* parent)
    : QObject(parent) {
    connect(&timer_, &QTimer::timeout, this, [this] {
        screenShot();
    })
}

void ScreenVideoSource::applyConfig(const AppConfig &config) {
    intervalMs_ = config.video.frameIntervalMs();
}

void ScreenVideoSource::start() {
    if (!timer_.isActive()) {
        timer_.start(intervalMs_);
    }
}

void ScreenVideoSource::stop() {
    if (timer_.isActive()) {
        timer_.stop();
    }
}

void ScreenVideoSource::screenShot() {
    QScreen* screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0);
    QImage img = pixmap.toImage();
    emit videoImageReady(img);
}