//
// Created by ASUS on 2026/8/4.
//

#include <QDateTime>
#include "VideoEncoder.h"
#include "VideoSampleCodec.h"

VideoEncoder::VideoEncoder(QObject* parent)
    : QObject(parent) {
}

void VideoEncoder::applyConfig(const AppConfig &config) {
    codecType_ = config.video.codecType;
    jpegQuality_ = config.video.jpegQuality;
}

void VideoEncoder::onVideoImageReady(const QImage &img) {
    switch (codecType_) {
        case VideoSampleCodecType::Jpeg : {
            QByteArray bytes = handleJpeg(img);
            if (bytes.isEmpty()) {
                return;
            }
            emit videoSampleBytesReady(bytes);
            break;
        }
        default:
            break;
    }
}

QByteArray VideoEncoder::handleJpeg(const QImage &img) {
    QByteArray bytes;
    QBuffer buffer(&bytes);

    if (!buffer.open(QIODevice::WriteOnly)) {
        return QByteArray();
    }

    if (!img.save(&buffer, "JPG", jpegQuality_)) {
        return QByteArray();
    }
    VideoSample sample;
    sample.videoSeq = nextVideoSeq_++;
    sample.captureTimeStampMs =
            static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    sample.width = static_cast<quint16>(img.width());
    sample.height = static_cast<qint16>(img.height());
    sample.codec = VideoSampleCodecType::Jpeg;
    sample.flags = 0;
    sample.data = bytes;

    qDebug() << "send sample"
             << sample.videoSeq
             << sample.width << sample.height
             << sample.data.size();

    return VideoSampleCodec::encode(sample);
}

