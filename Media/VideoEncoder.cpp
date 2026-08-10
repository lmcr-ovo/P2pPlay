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
    connect(this, &VideoEncoder::forwadEncode,
            worker_, &VideoEncoderWorker::onVideoImageReady);
    connect(worker_, &VideoEncoderWorker::videoSampleBytesReady,
            this, &VideoEncoder::videoSampleBytesReady);
}

VideoEncoder::~VideoEncoder() {
    disconnect(worker_, nullptr, this, nullptr);
    thread_->quit();
    thread_->wait(5000);
    delete worker_;
    worker_ = nullptr;
}


void VideoEncoder::applyConfig(const AppConfig &config) {
    worker_->applyConfig(config);
}

void VideoEncoderWorker::applyConfig(const AppConfig& config) {
    jpegQuality_ = config.video.jpegQuality;
    codecType_ = config.video.codecType;
}

void VideoEncoder::onVideoImageReady(const QImage &img,
                                     quint32 sampleSeq) {
    emit forwadEncode(img, sampleSeq);
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

    TraceManager::instance().record(sampleSeq, TraceStage::EncodeEnd, TraceManager::nowUs());
    if (!img.save(&buffer, "JPG", jpegQuality_)) {
        return QByteArray();
    }

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

