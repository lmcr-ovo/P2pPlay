#include "VideoDecoder.h"

VideoDecoder::VideoDecoder(QObject* parent)
    : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new VideoDecoderWorker;
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);

    thread_->start();
}

VideoDecoder::~VideoDecoder() {
    if (thread_ != nullptr) {
        thread_->quit();
        thread_->wait();
    }
    worker_ = nullptr;
}

void VideoDecoder::applyConfig(const AppConfig& config) {
    VideoDecoderWorker* worker = worker_;
    QMetaObject::invokeMethod(
            worker,
            [worker, config] {
                worker->applyConfig(config);
            },
            Qt::QueuedConnection
    );
}

VideoDecoderWorker* VideoDecoder::worker() const {
    return worker_;
}
