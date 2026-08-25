//
// Created by ASUS on 2026/8/4.
//

#include <QDateTime>
#include <QStringList>
#include "VideoEncoder.h"
#include "VideoSampleCodec.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

VideoEncoder::VideoEncoder(QObject* parent)
    : QObject(parent) {
    thread_ = new QThread(this);
    worker_ = new VideoEncoderWorker;

    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    thread_->start();
}

VideoEncoder::~VideoEncoder() {
    disconnect(worker_, nullptr, this, nullptr);
    thread_->quit();
    thread_->wait();
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

VideoEncoderWorker::VideoEncoderWorker(QObject* parent)
    : QObject(parent) {
}

VideoEncoderWorker::~VideoEncoderWorker() {
    stopH264Encoder();
}

void VideoEncoderWorker::applyConfig(const AppConfig& config) {
    jpegQuality_ = config.video.jpegQuality;
    codecType_ = config.video.codecType;
    fps_ = config.video.fps > 0 ? config.video.fps : 60;
    h264BitrateKbps_ = config.video.h264BitrateKbps;
    h264GopFrames_ = config.video.h264GopFrames > 0
            ? config.video.h264GopFrames
            : fps_;
    h264EncoderThreads_ = config.video.h264EncoderThreads;
    h264Preset_ = config.video.h264Preset;
    h264Tune_ = config.video.h264Tune;
    h264Profile_ = config.video.h264Profile;
    h264RepeatHeaders_ = config.video.h264RepeatHeaders;
    h264ForceIdr_ = config.video.h264ForceIdr;
    forceNextKeyFrame_ = true;

    bitrateKbpsMax_ = config.video.getBitrateKbpsMax();
    bitrateKbpsMin_ = config.video.getBitrateKbpsMin();
    stopH264Encoder();
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
        case VideoSampleCodecType::H264 : {
            QByteArray bytes = handleH264(img, sampleSeq);
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

void VideoEncoderWorker::requestKeyFrame() {
    forceNextKeyFrame_ = true;
}

void VideoEncoderWorker::onTargetBitrateChanged(double rate) {
    if (rate <= 0) {
        return;
    }

    // 新码率 = 当前码率 × 因子
    quint16 newBitRate = 0;
    if (rate > 1.0) {
        auto r1 = static_cast<quint16>(h264BitrateKbps_ * rate);
        auto r2 = static_cast<quint16>((bitrateKbpsMax_ + h264BitrateKbps_) / 2);
        newBitRate = qMin(r1, r2);
    } else {
        newBitRate = qMax(
                static_cast<quint16>(h264BitrateKbps_ * rate),
                static_cast<quint16>(bitrateKbpsMin_ / 2));
        if (newBitRate < bitrateKbpsMin_) {
            emit warningBitRateTooLow(newBitRate);
        }
    }

    qDebug() << QString("[自适应][host] 码率 %1 → %2 kbps (rate=%3) minR = %4 maxR = %5")
            .arg(h264BitrateKbps_)
            .arg(newBitRate)
            .arg(QString::number(rate))
            .arg(bitrateKbpsMin_)
            .arg(bitrateKbpsMax_);

    h264BitrateKbps_ = newBitRate;

    if (h264CodecContext_ != nullptr) {
        // bit_rate 单位是 bps，所以 newBitrate(kbps) × 1000
        h264CodecContext_->bit_rate = static_cast<int64_t>(newBitRate) * 1000;
    }
    emit changePacketsPerTick(newBitRate);
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

QByteArray VideoEncoderWorker::handleH264(const QImage& img, quint32 sampleSeq) {
    if (img.isNull()) {
        return QByteArray();
    }

    if (!ensureH264Encoder(img.width(), img.height())) {
        return QByteArray();
    }

    QImage input = img.convertToFormat(QImage::Format_ARGB32);
    if (input.isNull()) {
        return QByteArray();
    }

    // 重置画布
    if (av_frame_make_writable(h264Frame_) < 0) {
        return QByteArray();
    }

    const uint8_t* sourceSlice[] = {
            input.constBits()
    };
    const int sourceStride[] = {
            input.bytesPerLine()
    };

    // 给画布H264Frame_填入数据
    sws_scale(h264SwsContext_,
              sourceSlice,
              sourceStride,
              0,
              h264CodecContext_->height,
              h264Frame_->data,
              h264Frame_->linesize);

    h264Frame_->pts = nextPts_++;
    if (forceNextKeyFrame_) {
        // 强制下一帧为关键帧
        h264Frame_->pict_type = AV_PICTURE_TYPE_I;
        forceNextKeyFrame_ = false;
    } else {
        // 编码器自己决定
        h264Frame_->pict_type = AV_PICTURE_TYPE_NONE;
    }

    // 将h264Frame_数据输入编码器
    if (avcodec_send_frame(h264CodecContext_, h264Frame_) < 0) {
        return QByteArray();
    }

    TraceManager::instance().record(sampleSeq, TraceStage::EncodeEnd, TraceManager::nowUs());

    quint32 sampleFlags = VideoSampleFlag_None;
    QByteArray h264Bytes = drainH264Packets(sampleFlags, sampleSeq);
    if (h264Bytes.isEmpty()) {
        return QByteArray();
    }

    VideoSample sample;
    sample.videoSeq = sampleSeq;
    sample.captureTimeStampMs =
            static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    sample.width = static_cast<quint16>(img.width());
    sample.height = static_cast<quint16>(img.height());
    sample.codec = VideoSampleCodecType::H264;
    sample.flags = sampleFlags;
    sample.data = h264Bytes;

    const QByteArray encoded = VideoSampleCodec::encode(sample);
    TraceManager::instance().record(sampleSeq, TraceStage::PackEnd, TraceManager::nowUs());

    return encoded;
}

bool VideoEncoderWorker::ensureH264Encoder(int width, int height) {
    if (h264CodecContext_ != nullptr
            && h264Width_ == width
            && h264Height_ == height) {
        return true;
    }

    stopH264Encoder();

    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (codec == nullptr) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (codec == nullptr) {
        return false;
    }

    h264CodecContext_ = avcodec_alloc_context3(codec);
    if (h264CodecContext_ == nullptr) {
        return false;
    }

    h264CodecContext_->width = width;
    h264CodecContext_->height = height;
    h264CodecContext_->time_base = AVRational{1, static_cast<int>(fps_)};
    h264CodecContext_->framerate = AVRational{static_cast<int>(fps_), 1};
    h264CodecContext_->pix_fmt = AV_PIX_FMT_YUV420P;
    h264CodecContext_->bit_rate = static_cast<int64_t>(h264BitrateKbps_) * 1000;
    h264CodecContext_->gop_size = h264GopFrames_;
    h264CodecContext_->max_b_frames = 0;
    h264CodecContext_->thread_count = h264EncoderThreads_;
    h264CodecContext_->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (!h264Preset_.isEmpty()) {
        av_opt_set(h264CodecContext_->priv_data,
                   "preset",
                   h264Preset_.toUtf8().constData(),
                   0);
    }
    if (!h264Tune_.isEmpty()) {
        av_opt_set(h264CodecContext_->priv_data,
                   "tune",
                   h264Tune_.toUtf8().constData(),
                   0);
    }
    if (!h264Profile_.isEmpty()) {
        av_opt_set(h264CodecContext_->priv_data,
                   "profile",
                   h264Profile_.toUtf8().constData(),
                   0);
    }
    if (h264ForceIdr_) {
        av_opt_set(h264CodecContext_->priv_data,
                   "forced-idr",
                   "1",
                   0);
    }

    QStringList x264Params;
    x264Params << QString("keyint=%1").arg(h264GopFrames_);
    x264Params << QString("min-keyint=%1").arg(h264GopFrames_);
    x264Params << "scenecut=0";
    if (h264RepeatHeaders_) {
        x264Params << "repeat-headers=1";
    }

    const QByteArray x264ParamsBytes = x264Params.join(':').toUtf8();
    av_opt_set(h264CodecContext_->priv_data,
               "x264-params",
               x264ParamsBytes.constData(),
               0);

    if (avcodec_open2(h264CodecContext_, codec, nullptr) < 0) {
        stopH264Encoder();
        return false;
    }

    h264Frame_ = av_frame_alloc();
    h264Packet_ = av_packet_alloc();
    if (h264Frame_ == nullptr || h264Packet_ == nullptr) {
        stopH264Encoder();
        return false;
    }

    h264Frame_->format = h264CodecContext_->pix_fmt;
    h264Frame_->width = h264CodecContext_->width;
    h264Frame_->height = h264CodecContext_->height;
    if (av_frame_get_buffer(h264Frame_, 32) < 0) {
        stopH264Encoder();
        return false;
    }

    h264SwsContext_ = sws_getContext(h264CodecContext_->width,
                                     h264CodecContext_->height,
                                     AV_PIX_FMT_BGRA,
                                     h264CodecContext_->width,
                                     h264CodecContext_->height,
                                     h264CodecContext_->pix_fmt,
                                     SWS_FAST_BILINEAR,
                                     nullptr,
                                     nullptr,
                                     nullptr);
    if (h264SwsContext_ == nullptr) {
        stopH264Encoder();
        return false;
    }

    h264Width_ = width;
    h264Height_ = height;
    nextPts_ = 0;
    forceNextKeyFrame_ = true;
    return true;
}

QByteArray VideoEncoderWorker::drainH264Packets(quint32& sampleFlags, quint32 sampleSeq) {
    QByteArray result;

    while (avcodec_receive_packet(h264CodecContext_, h264Packet_) == 0) {
        if ((h264Packet_->flags & AV_PKT_FLAG_KEY) != 0) {
            qDebug() << QString("[关键帧][host] 输出关键帧, sampleSeq = %1")
                .arg(sampleSeq);
            sampleFlags |= VideoSampleFlag_KeyFrame;
        }

        result.append(reinterpret_cast<const char*>(h264Packet_->data),
                      h264Packet_->size);
        av_packet_unref(h264Packet_);
    }

    return result;
}

void VideoEncoderWorker::stopH264Encoder() {
    if (h264CodecContext_ != nullptr) {
        avcodec_send_frame(h264CodecContext_, nullptr);
    }

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

    h264Width_ = 0;
    h264Height_ = 0;
    nextPts_ = 0;
}
