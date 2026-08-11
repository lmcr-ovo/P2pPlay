可以。你现在升级 H.264，不需要把整个传输层推倒重来。你的项目已经有这个枚举：

```cpp
enum class VideoSampleCodecType : quint8 {
    Unknown = 0,
    Jpeg = 1,
    H264 = 2
};
```

所以建议第一版就这么做：

```text
采集 QImage
  -> FFmpeg 编码成 H.264 bytes
  -> 塞进 VideoSample.data
  -> VideoSampleCodec::encode(sample)
  -> UDP 分片发送

接收端
  -> UDP 重组
  -> VideoSampleCodec::decode(bytes, sample)
  -> sample.codec == H264
  -> FFmpeg 解码 sample.data
  -> QImage
  -> 渲染
```

也就是说，UDP 层、重组器、日志系统、`MediaService` 都基本不用动。只改 `Media` 层的编码/解码。

---

## 一、你需要理解的 FFmpeg 几个对象

你不用精通 FFmpeg，先掌握这几个东西就够了。

### 1. `AVCodecContext`

编码器/解码器实例。可以理解为：

```text
H.264 编码器对象
```

里面配置：

```cpp
width
height
fps
bit_rate
pix_fmt
gop_size
max_b_frames
```

### 2. `AVFrame`

原始图像帧。

编码时你要把 `QImage` 转成 `AVFrame`，通常是：

```text
QImage ARGB32/BGRA
  -> sws_scale
  -> AVFrame YUV420P
```

H.264 编码器一般吃 `YUV420P`。

### 3. `AVPacket`

编码后的压缩数据。

```text
AVFrame 原图
  -> avcodec_send_frame
  -> avcodec_receive_packet
  -> AVPacket H.264 bytes
```

你最终要发送的就是 `AVPacket` 里的数据。

### 4. `SwsContext`

像素格式转换器。

你现在的 `QImage` 是 RGB/ARGB，H.264 要 YUV，所以中间需要：

```cpp
sws_scale(...)
```

---

## 二、CMake 先加 FFmpeg

你之前 `RemoteControl` 里已经有一套可用 CMake，可以搬到 `P2Pplay/Media/CMakeLists.txt`。

你现在的 `Media/CMakeLists.txt` 需要增加 FFmpeg 查找和链接。

大概这样：

```cmake
set(FFMPEG_ROOT "" CACHE PATH "Path to an FFmpeg shared/dev build that contains include and lib directories")

if(NOT FFMPEG_ROOT)
    find_program(FFMPEG_EXECUTABLE ffmpeg)
    if(FFMPEG_EXECUTABLE)
        get_filename_component(FFMPEG_BIN_DIR "${FFMPEG_EXECUTABLE}" DIRECTORY)
        get_filename_component(FFMPEG_ROOT "${FFMPEG_BIN_DIR}" DIRECTORY)
    endif()
endif()

find_path(FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h libswscale/swscale.h
        PATHS "${FFMPEG_ROOT}/include"
        NO_DEFAULT_PATH)

find_library(FFMPEG_AVCODEC_LIBRARY
        NAMES avcodec
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH)

find_library(FFMPEG_SWSCALE_LIBRARY
        NAMES swscale
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH)

find_library(FFMPEG_AVUTIL_LIBRARY
        NAMES avutil
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH)

if(NOT FFMPEG_INCLUDE_DIR
        OR NOT FFMPEG_AVCODEC_LIBRARY
        OR NOT FFMPEG_SWSCALE_LIBRARY
        OR NOT FFMPEG_AVUTIL_LIBRARY)
    message(FATAL_ERROR
            "FFmpeg development files were not found. "
            "Set FFMPEG_ROOT to a shared/dev FFmpeg build directory containing include/ and lib/.")
endif()
```

然后：

```cmake
target_include_directories(Media PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${FFMPEG_INCLUDE_DIR}")
```

链接加上：

```cmake
target_link_libraries(Media PUBLIC
        Qt5::Core
        Qt5::Network
        Qt5::Gui
        Qt5::Widgets
        Tcp
        Config
        Logger
        "${FFMPEG_AVCODEC_LIBRARY}"
        "${FFMPEG_SWSCALE_LIBRARY}"
        "${FFMPEG_AVUTIL_LIBRARY}")
```

如果你电脑上只有 `ffmpeg.exe`，但没有 `include/` 和 `lib/`，那不够。你需要的是 FFmpeg dev/shared 包，结构大概是：

```text
ffmpeg/
  bin/
    ffmpeg.exe
    avcodec-xx.dll
  include/
    libavcodec/avcodec.h
    libavutil/avutil.h
    libswscale/swscale.h
  lib/
    avcodec.lib 或 libavcodec.dll.a
    avutil.lib 或 libavutil.dll.a
    swscale.lib 或 libswscale.dll.a
```

你用 MinGW，所以一般需要 `.dll.a` 这种 import library。

---

## 三、编码器怎么改

你现在 `VideoEncoderWorker` 已经有线程，正好放 H.264。

当前逻辑是：

```cpp
void VideoEncoderWorker::onVideoImageReady(const QImage &img,
        quint32 sampleSeq) {
    switch (codecType_) {
        case VideoSampleCodecType::Jpeg : {
            QByteArray bytes = handleJpeg(img, sampleSeq);
            ...
            emit videoSampleBytesReady(bytes);
            break;
        }
        default:
            break;
    }
}
```

应该加：

```cpp
case VideoSampleCodecType::H264: {
    QByteArray bytes = handleH264(img, sampleSeq);
    if (bytes.isEmpty()) {
        return;
    }
    emit videoSampleBytesReady(bytes);
    break;
}
```

---

## 四、`VideoEncoder.h` 需要加的成员

参考你之前 `RemoteControl` 的写法，`VideoEncoderWorker` 里加这些成员：

```cpp
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
```

然后类里面加：

```cpp
QByteArray handleH264(const QImage& img, quint32 sampleSeq);

bool ensureH264Encoder(int width, int height);
void stopH264Encoder();
QImage prepareH264Image(const QImage& img) const;
QByteArray drainH264Packets();

AVCodecContext* h264CodecContext_ = nullptr;
AVFrame* h264Frame_ = nullptr;
AVPacket* h264Packet_ = nullptr;
SwsContext* h264SwsContext_ = nullptr;

int h264Width_ = 0;
int h264Height_ = 0;
qint64 nextPts_ = 0;
```

为什么要存成员？

因为 H.264 编码器不能每帧创建一次。必须复用：

```text
创建编码器一次
每帧 send_frame / receive_packet
析构时释放
```

如果每帧创建 FFmpeg 编码器，延迟会爆炸。

---

## 五、H.264 编码核心代码

你可以先照这个结构写。

```cpp
QByteArray VideoEncoderWorker::handleH264(const QImage& img, quint32 sampleSeq) {
    if (img.isNull()) {
        return {};
    }

    if (!ensureH264Encoder(img.width(), img.height())) {
        return {};
    }

    QImage input = img.convertToFormat(QImage::Format_ARGB32);
    if (input.isNull()) {
        return {};
    }

    if (av_frame_make_writable(h264Frame_) < 0) {
        return {};
    }

    const uint8_t* sourceSlice[] = {
        input.constBits()
    };

    const int sourceStride[] = {
        input.bytesPerLine()
    };

    sws_scale(h264SwsContext_,
              sourceSlice,
              sourceStride,
              0,
              h264CodecContext_->height,
              h264Frame_->data,
              h264Frame_->linesize);

    h264Frame_->pts = nextPts_++;

    if (avcodec_send_frame(h264CodecContext_, h264Frame_) < 0) {
        return {};
    }

    TraceManager::instance().record(sampleSeq,
                                    TraceStage::EncodeEnd,
                                    TraceManager::nowUs());

    QByteArray h264Bytes = drainH264Packets();
    if (h264Bytes.isEmpty()) {
        return {};
    }

    VideoSample sample;
    sample.videoSeq = sampleSeq;
    sample.captureTimeStampMs =
            static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    sample.width = static_cast<quint16>(img.width());
    sample.height = static_cast<quint16>(img.height());
    sample.codec = VideoSampleCodecType::H264;
    sample.flags = 0;
    sample.data = h264Bytes;

    QByteArray encodedSample = VideoSampleCodec::encode(sample);

    TraceManager::instance().record(sampleSeq,
                                    TraceStage::PackEnd,
                                    TraceManager::nowUs());

    return encodedSample;
}
```

需要的 FFmpeg include：

```cpp
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
```

---

## 六、初始化 H.264 编码器

```cpp
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

    h264CodecContext_->time_base = AVRational{1, 60};
    h264CodecContext_->framerate = AVRational{60, 1};

    h264CodecContext_->pix_fmt = AV_PIX_FMT_YUV420P;

    h264CodecContext_->bit_rate = 4 * 1000 * 1000;

    h264CodecContext_->gop_size = 60;
    h264CodecContext_->max_b_frames = 0;

    h264CodecContext_->thread_count = 1;
    h264CodecContext_->flags |= AV_CODEC_FLAG_LOW_DELAY;
```

低延迟参数：

```cpp
    av_opt_set(h264CodecContext_->priv_data, "preset", "ultrafast", 0);
    av_opt_set(h264CodecContext_->priv_data, "tune", "zerolatency", 0);
    av_opt_set(h264CodecContext_->priv_data, "profile", "baseline", 0);
    av_opt_set(h264CodecContext_->priv_data,
               "x264-params",
               "keyint=60:min-keyint=60:scenecut=0",
               0);
```

打开编码器：

```cpp
    if (avcodec_open2(h264CodecContext_, codec, nullptr) < 0) {
        stopH264Encoder();
        return false;
    }
```

创建 frame / packet：

```cpp
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
```

创建颜色转换器：

```cpp
    h264SwsContext_ = sws_getContext(
            h264CodecContext_->width,
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

    return true;
}
```

---

## 七、取出编码后的 H.264 packet

```cpp
QByteArray VideoEncoderWorker::drainH264Packets() {
    QByteArray result;

    while (avcodec_receive_packet(h264CodecContext_, h264Packet_) == 0) {
        result.append(reinterpret_cast<const char*>(h264Packet_->data),
                      h264Packet_->size);
        av_packet_unref(h264Packet_);
    }

    return result;
}
```

这里第一版直接把同一帧产生的所有 packet 合并成一个 `QByteArray`。虽然有一次拷贝，但简单稳。

后面你如果要更极致，可以让 UDP 层按 `AVPacket` 直接分片，减少中间大包拷贝。

---

## 八、释放编码器

析构时一定要释放。

```cpp
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
```

`VideoEncoderWorker` 析构函数里调用：

```cpp
VideoEncoderWorker::~VideoEncoderWorker() {
    stopH264Encoder();
}
```

---

## 九、解码器怎么改

你现在 `VideoDecoder::onVideoSampleBytesReceived()` 是：

```cpp
switch (sample.codec) {
    case VideoSampleCodecType::Jpeg : {
        handleJpeg(sample);
        break;
    }
}
```

加：

```cpp
case VideoSampleCodecType::H264: {
    handleH264(sample);
    break;
}
```

H.264 解码器也应该是成员，不要每帧创建。

`VideoDecoder.h` 里加：

```cpp
struct AVCodecContext;
struct AVCodecParserContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
```

成员：

```cpp
void handleH264(VideoSample& sample);
bool ensureH264Decoder();
void stopH264Decoder();
void drainH264Frames(quint32 sampleSeq);

AVCodecContext* h264CodecContext_ = nullptr;
AVCodecParserContext* h264Parser_ = nullptr;
AVFrame* h264Frame_ = nullptr;
AVPacket* h264Packet_ = nullptr;
SwsContext* h264SwsContext_ = nullptr;
```

---

## 十、H.264 解码初始化

```cpp
bool VideoDecoder::ensureH264Decoder() {
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
```

---

## 十一、H.264 解码处理

```cpp
void VideoDecoder::handleH264(VideoSample& sample) {
    if (sample.data.isEmpty()) {
        return;
    }

    if (!ensureH264Decoder()) {
        return;
    }

    const uint8_t* inputData =
            reinterpret_cast<const uint8_t*>(sample.data.constData());
    int inputSize = sample.data.size();

    while (inputSize > 0) {
        uint8_t* packetData = nullptr;
        int packetSize = 0;

        const int consumed = av_parser_parse2(
                h264Parser_,
                h264CodecContext_,
                &packetData,
                &packetSize,
                inputData,
                inputSize,
                AV_NOPTS_VALUE,
                AV_NOPTS_VALUE,
                0);

        if (consumed < 0) {
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
        }
    }
}
```

---

## 十二、把解码后的 AVFrame 转 QImage

```cpp
void VideoDecoder::drainH264Frames(quint32 sampleSeq) {
    while (avcodec_receive_frame(h264CodecContext_, h264Frame_) == 0) {
        const AVPixelFormat sourceFormat =
                static_cast<AVPixelFormat>(h264Frame_->format);

        h264SwsContext_ = sws_getCachedContext(
                h264SwsContext_,
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

        QImage image(h264Frame_->width,
                     h264Frame_->height,
                     QImage::Format_ARGB32);

        uint8_t* destination[] = {
            image.bits()
        };

        const int destinationStride[] = {
            image.bytesPerLine()
        };

        sws_scale(h264SwsContext_,
                  h264Frame_->data,
                  h264Frame_->linesize,
                  0,
                  h264Frame_->height,
                  destination,
                  destinationStride);

        TraceManager::instance().record(sampleSeq,
                                        TraceStage::DecodeEnd,
                                        TraceManager::nowUs());

        emit videoImageReady(image, sampleSeq);

        av_frame_unref(h264Frame_);
    }
}
```

释放：

```cpp
void VideoDecoder::stopH264Decoder() {
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
```

---

## 十三、配置里切换 codec

你现在应该已经有类似：

```cpp
config.video.codecType
```

所以第一版只需要配置改成：

```cpp
codecType = VideoSampleCodecType::H264
```

或者如果配置文件里是字符串，就解析：

```text
codec=h264
```

对应：

```cpp
if (value == "h264") {
    codecType = VideoSampleCodecType::H264;
}
```

---

## 十四、第一版要注意的问题

H.264 和 JPEG 最大区别是：

```text
JPEG 每帧独立
H.264 帧之间有依赖
```

所以 UDP 丢帧后，H.264 可能会花屏或卡住，直到下一个关键帧。

因此你需要：

```cpp
gop_size = fps;
max_b_frames = 0;
tune=zerolatency
```

比如 60fps：

```cpp
gop_size = 60;
keyint=60:min-keyint=60
```

这样最多等 1 秒有一个关键帧。后面可以做“guest 请求 key frame”，但第一版先不做。

如果你想低延迟恢复更快，可以先设：

```cpp
gop_size = 30;
keyint=30:min-keyint=30
```

代价是码率会高一点。

---

## 十五、你当前项目最推荐的修改顺序

别一口气全改，容易炸。按这个顺序：

1. `Media/CMakeLists.txt` 接入 FFmpeg。
2. `VideoEncoderWorker` 加 H.264 编码成员和 `handleH264()`。
3. `VideoDecoder` 加 H.264 解码成员和 `handleH264()`。
4. 配置切到 `VideoSampleCodecType::H264`。
5. 先跑 30fps、低分辨率，比如 640x360。
6. 看日志：
   - `encodeToPack`
   - `packToTransport`
   - `receiveToDecode`
   - `decodeToRender`
7. 稳了再提高 fps / 分辨率 / 码率。

---

## 十六、最重要的一句话

你不用“调用 ffmpeg.exe”。  
你要用的是 FFmpeg 的 C API：

```cpp
avcodec_find_encoder_by_name
avcodec_alloc_context3
avcodec_open2
avcodec_send_frame
avcodec_receive_packet
```

之前 `RemoteControl` 已经实现过一版，`P2Pplay` 迁移时核心就是把那套 `VideoEncodeWorker / VideoDecodeWorker` 的 FFmpeg 逻辑接进你现在的 `VideoSample` 封装。H.264 bytes 仍然走你现在的 UDP 分片和重组。