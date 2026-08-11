//
// Created by ASUS on 2026/8/4.
//

#include <QDateTime>
#include "VideoEncoder.h"
#include "VideoSampleCodec.h"

VideoEncoder::VideoEncoder(QObject* parent)
    : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new VideoEncoderWorker;

    worker_->moveToThread(thread_);
    thread_->start();
}

VideoEncoder::~VideoEncoder() {
    disconnect(worker_, nullptr, this, nullptr);
    thread_->quit();
    worker_ = nullptr;
}


void VideoEncoder::applyConfig(const AppConfig &config) {
    VideoEncoderWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, config] {
                worker->applyConfig(config);
            },
            Qt::QueuedConnection
            );
}


VideoEncoderWorker* VideoEncoder::worker() const {
    return worker_;
}

void VideoEncoderWorker::applyConfig(const AppConfig& config) {
    jpegQuality_ = config.video.jpegQuality;
    codecType_ = config.video.codecType;
}


void VideoEncoderWorker::onVideoImageReady(const QImage &img,
        quint32 sampleSeq) {
    switch (codecType_) {
        case VideoSampleCodecType::Jpeg : {
            QByteArray bytes = handleJpeg(img, sampleSeq);
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

QByteArray VideoEncoderWorker::handleJpeg(const QImage &img,
        quint32 sampleSeq) {
    QByteArray bytes;
    QBuffer buffer(&bytes);

    if (!buffer.open(QIODevice::WriteOnly)) {
        return QByteArray();
    }


    if (!img.save(&buffer, "JPG", jpegQuality_)) {
        return QByteArray();
    }
    TraceManager::instance().record(sampleSeq, TraceStage::EncodeEnd, TraceManager::nowUs());

    VideoSample sample;
    sample.videoSeq = sampleSeq;
    sample.captureTimeStampMs =
            static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    sample.width = static_cast<quint16>(img.width());
    sample.height = static_cast<qint16>(img.height());
    sample.codec = VideoSampleCodecType::Jpeg;
    sample.flags = 0;
    sample.data = bytes;

    const QByteArray encoded = VideoSampleCodec::encode(sample);
    TraceManager::instance().record(sampleSeq, TraceStage::PackEnd, TraceManager::nowUs());

    return encoded;
}

