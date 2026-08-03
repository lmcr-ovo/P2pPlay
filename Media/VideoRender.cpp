//
// Created by ASUS on 2026/8/2.
//

#include "VideoRender.h"
#include <QDebug>

VideoRender::VideoRender(QObject* parent)
    : QObject(parent) {

}

void VideoRender::onVideoFrameRecevied(const QByteArray &payload) {
    qDebug() << QString::fromUtf8(payload);
}