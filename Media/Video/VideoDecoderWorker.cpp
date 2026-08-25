//
// Created by ASUS on 2026/8/11.
//
#include <QDateTime>
#include "VideoSampleCodec.h"
#include "VideoDecoderWorker.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

VideoDecoderWorker::VideoDecoderWorker(QObject* parent)
        : QObject(parent) {
}

VideoDecoderWorker::~VideoDecoderWorker() {
    stopH264Decoder();
}

void VideoDecoderWorker::applyConfig(const AppConfig& config) {
    keyFrameRequestIntervalMs_ = qMax(50, config.video.h264KeyFrameRequestIntervalMs);
}

void VideoDecoderWorker::onVideoSampleBytesReceived(const QByteArray &bytes) {
    VideoSample sample;
    if (!VideoSampleCodec::decode(bytes, sample)) {
        return;
    }

    switch (sample.codec) {
        case VideoSampleCodecType::Jpeg : {
            handleJpeg(sample);
            break;
        }
        case VideoSampleCodecType::H264 : {
            handleH264(sample);
            break;
        }
        default:
            break;
    }
}

void VideoDecoderWorker::handleJpeg(VideoSample& sample) {
    if (sample.data.isEmpty()) {
        return;
    }

    QImage img;
    if (!img.loadFromData(sample.data, "JPG")) {
        return;
    }

    TraceManager::instance().record(sample.videoSeq,
                                    TraceStage::DecodeEnd,
                                    TraceManager::nowUs());

    emit videoImageReady(img, sample.videoSeq);
}

void VideoDecoderWorker::handleH264(VideoSample& sample) {
    if (sample.data.isEmpty()) {
        return;
    }

    const bool isKeyFrame =
            (sample.flags & VideoSampleFlag_KeyFrame) != 0;

    // 断号检测
    if (hasLastH264Seq_ && sample.videoSeq != lastH264Seq_ + 1) {

        qDebug() << QString("[H264断号] seq=%1 last=%2")
                .arg(sample.videoSeq).arg(lastH264Seq_);

        waitingForKeyFrame_ = true;
        stopH264Decoder();
        requestKeyFrameIfNeeded();
    }

    hasLastH264Seq_ = true;
    lastH264Seq_ = sample.videoSeq;

    if (waitingForKeyFrame_ && !isKeyFrame) {

        qDebug() << QString("[H264跳过][guest] seq=%1 等关键帧")
                .arg(sample.videoSeq);

        requestKeyFrameIfNeeded();
        return;
    }

    if (isKeyFrame) {
        qDebug() << QString("[H264恢复][guest] seq=%1 收到关键帧")
                .arg(sample.videoSeq);

        waitingForKeyFrame_ = false;
        lastKeyFrameRequestMs_ = 0;
    }

    if (!ensureH264Decoder()) {
        qDebug() << QString("[H264恢复][guest] seq=%1 收到关键帧")
                .arg(sample.videoSeq);

        waitingForKeyFrame_ = true;
        requestKeyFrameIfNeeded();
        return;
    }

    const uint8_t* inputData =
            reinterpret_cast<const uint8_t*>(sample.data.constData());
    int inputSize = sample.data.size();

    while (inputSize > 0) {
        uint8_t* packetData = nullptr;
        int packetSize = 0;

        const int consumed = av_parser_parse2(h264Parser_,
                                              h264CodecContext_,
                                              &packetData,
                                              &packetSize,
                                              inputData,
                                              inputSize,
                                              AV_NOPTS_VALUE,
                                              AV_NOPTS_VALUE,
                                              0);
        if (consumed < 0) {
            qDebug() << QString("[H264失败][guest] seq=%1 av_parser_parse2失败")
                    .arg(sample.videoSeq);

            waitingForKeyFrame_ = true;
            stopH264Decoder();
            requestKeyFrameIfNeeded();
            return;
        }

        inputData += consumed;
        inputSize -= consumed;

        if (packetSize <= 0) {
            continue;
        }

        av_packet_unref(h264Packet_);
        h264Packet_->data = packetData;
        h264Packet_->size = packetSize;

        if (avcodec_send_packet(h264CodecContext_, h264Packet_) == 0) {
            drainH264Frames(sample.videoSeq);
        } else {
            qDebug() << QString("[H264失败][guest] seq=%1 avcodec_send_packet失败")
                    .arg(sample.videoSeq);

            waitingForKeyFrame_ = true;
            stopH264Decoder();
            requestKeyFrameIfNeeded();
            return;
        }
    }
}

bool VideoDecoderWorker::ensureH264Decoder() {
    if (h264CodecContext_ != nullptr) {
        return true;
    }

    stopH264Decoder();

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (codec == nullptr) {
        return false;
    }

    h264Parser_ = av_parser_init(codec->id);
    h264CodecContext_ = avcodec_alloc_context3(codec);
    h264Frame_ = av_frame_alloc();
    h264Packet_ = av_packet_alloc();

    if (h264Parser_ == nullptr
        || h264CodecContext_ == nullptr
        || h264Frame_ == nullptr
        || h264Packet_ == nullptr) {
        stopH264Decoder();
        return false;
    }

    h264CodecContext_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    h264CodecContext_->thread_count = 1;

    if (avcodec_open2(h264CodecContext_, codec, nullptr) < 0) {
        stopH264Decoder();
        return false;
    }

    return true;
}

void VideoDecoderWorker::drainH264Frames(quint32 sampleSeq) {
    while (avcodec_receive_frame(h264CodecContext_, h264Frame_) == 0) {
        const AVPixelFormat sourceFormat =
                static_cast<AVPixelFormat>(h264Frame_->format);

        h264SwsContext_ = sws_getCachedContext(h264SwsContext_,
                                               h264Frame_->width,
                                               h264Frame_->height,
                                               sourceFormat,
                                               h264Frame_->width,
                                               h264Frame_->height,
                                               AV_PIX_FMT_BGRA,
                                               SWS_FAST_BILINEAR,
                                               nullptr,
                                               nullptr,
                                               nullptr);
        if (h264SwsContext_ == nullptr) {
            av_frame_unref(h264Frame_);
            continue;
        }

        QImage img(h264Frame_->width,
                   h264Frame_->height,
                   QImage::Format_ARGB32);

        uint8_t* destination[] = {
                img.bits()
        };
        const int destinationStride[] = {
                img.bytesPerLine()
        };

        sws_scale(h264SwsContext_,
                  h264Frame_->data,
                  h264Frame_->linesize,
                  0,
                  h264Frame_->height,
                  destination,
                  destinationStride);

        // 记录重组结束到解码为image的时间
        TraceManager::instance().record(sampleSeq,
                                        TraceStage::DecodeEnd,
                                        TraceManager::nowUs());

        emit videoImageReady(img, sampleSeq);
        av_frame_unref(h264Frame_);
    }
}

void VideoDecoderWorker::stopH264Decoder() {
    if (h264SwsContext_ != nullptr) {
        sws_freeContext(h264SwsContext_);
        h264SwsContext_ = nullptr;
    }

    if (h264Packet_ != nullptr) {
        av_packet_free(&h264Packet_);
    }

    if (h264Frame_ != nullptr) {
        av_frame_free(&h264Frame_);
    }

    if (h264CodecContext_ != nullptr) {
        avcodec_free_context(&h264CodecContext_);
    }

    if (h264Parser_ != nullptr) {
        av_parser_close(h264Parser_);
        h264Parser_ = nullptr;
    }
}

void VideoDecoderWorker::requestKeyFrameIfNeeded() {
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (lastKeyFrameRequestMs_ > 0
        && nowMs - lastKeyFrameRequestMs_ < keyFrameRequestIntervalMs_) {
        return;
    }

    lastKeyFrameRequestMs_ = nowMs;

    qDebug() << "[关键帧][guest] 请求关键帧" << keyFrameRequestIntervalMs_;

    emit keyFrameRequestNeeded();
}