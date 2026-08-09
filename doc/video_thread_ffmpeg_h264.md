# 视频线程化与 FFmpeg H.264 升级方案

这份文档的目标不是把 H.264 全部原理学完，而是让当前项目能从 JPEG-over-UDP 逐步升级到更适合实时视频的结构。

你只需要先掌握这些能力：

```text
1. Qt 里怎么把耗时任务放到单独线程
2. FFmpeg 编码器怎么输入一帧图像，输出 H.264 数据
3. FFmpeg 解码器怎么输入 H.264 数据，输出 QImage
4. 当前 UDP 传输层怎么承载编码后的帧
5. 丢帧、关键帧、延迟之间怎么取舍
```

---

## 1. 当前问题

现在项目是：

```text
ScreenVideoSource
    -> VideoEncoder(JPEG)
    -> VideoSampleCodec
    -> MediaService
    -> UdpPacketQueue
    -> UDP
    -> UdpFrameReassembler
    -> MediaService
    -> VideoDecoder(JPEG)
    -> VideoWidget
```

这个结构能跑，但 JPEG 的问题很明显：

```text
1. 每帧都是完整图片，码率很大
2. 分辨率/FPS 一高，UDP 分片数量很多
3. 发送队列必须丢帧，否则延迟会无限增长
4. 编码/解码在主链路里，容易影响采集、收包、渲染
```

H.264 的优势是：

```text
1. P 帧只记录变化，码率比 JPEG 小很多
2. 同样网络下可以更高 FPS 或更低延迟
3. 可以控制码率、GOP、关键帧间隔
```

但 H.264 也带来新问题：

```text
1. 有关键帧和非关键帧
2. 丢包后可能需要请求新的关键帧
3. 编码器/解码器需要保存上下文状态
4. 不适合每帧创建一次编码器
```

所以升级顺序不要一口吃成胖子。

---

## 2. 推荐升级顺序

建议按这个顺序做：

```text
第一步：先把 JPEG 编码/解码移到 worker 线程
第二步：抽象视频编码器/解码器接口
第三步：接入 FFmpeg H.264 软件编码/解码
第四步：加入关键帧请求
第五步：根据情况再接硬件编码
```

原因是：线程模型和 codec 抽象是 H.264 的地基。  
如果直接把 FFmpeg 塞进当前 `VideoEncoder` / `VideoDecoder`，后面会很难拆。

---

## 3. Qt 线程怎么理解

Qt 里推荐的做法不是继承 `QThread` 写业务逻辑，而是：

```text
QObject Worker + moveToThread(QThread)
```

理解成：

```text
QThread 是一条工人流水线
Worker 是真正干活的人
moveToThread 是把这个人安排到那条流水线上
signal/slot 是投递任务和拿结果
```

对于视频项目，建议拆成：

```text
主线程 / GUI 线程：
    - Qt 窗口
    - VideoWidget 绘制

采集线程或主线程定时采集：
    - ScreenVideoSource

编码线程：
    - VideoEncodeWorker

发送线程 / 当前事件线程：
    - UdpPacketQueue

解码线程：
    - VideoDecodeWorker

渲染仍然回 GUI 线程：
    - VideoWidget
```

最小改造可以先只拆两个：

```text
VideoEncodeWorker
VideoDecodeWorker
```

---

## 4. 编码线程应该长什么样

不要让 `VideoEncoder` 直接做耗时编码。  
建议拆成：

```text
VideoEncoder
    对外接口层，负责信号转发

VideoEncodeWorker
    真正执行 JPEG/H.264 编码
```

大概结构：

```cpp
class VideoEncodeWorker : public QObject {
    Q_OBJECT

public slots:
    void encodeImage(const QImage& image, quint32 sampleId);

signals:
    void encoded(quint32 sampleId, const QByteArray& bytes);
    void errorOccurred(const QString& reason);
};
```

连接方式：

```cpp
QThread* encodeThread = new QThread(this);
VideoEncodeWorker* worker = new VideoEncodeWorker();

worker->moveToThread(encodeThread);

connect(this, &VideoEncoder::imageReadyForEncode,
        worker, &VideoEncodeWorker::encodeImage,
        Qt::QueuedConnection);

connect(worker, &VideoEncodeWorker::encoded,
        this, &VideoEncoder::onEncoded,
        Qt::QueuedConnection);

connect(encodeThread, &QThread::finished,
        worker, &QObject::deleteLater);

encodeThread->start();
```

注意：

```text
1. QImage 跨线程传递通常没问题，它是隐式共享
2. Worker 内不要直接操作 QWidget
3. VideoWidget 只能在 GUI 线程绘制
4. 编码线程里要做丢帧保护，不能无限排队
```

编码线程的队列策略也要实时化：

```text
如果编码线程忙：
    只保留最新待编码帧
    丢掉旧的待编码帧
```

不要让编码任务无限堆。

---

## 5. 解码线程应该长什么样

解码线程类似：

```cpp
class VideoDecodeWorker : public QObject {
    Q_OBJECT

public slots:
    void decodeSample(const QByteArray& bytes);

signals:
    void imageDecoded(const QImage& image, quint32 sampleId);
    void errorOccurred(const QString& reason);
};
```

接收端流程：

```text
MediaService 收到完整视频 sample bytes
    -> queued signal 给 VideoDecodeWorker
    -> 解码出 QImage
    -> queued signal 回 VideoWidget
```

这里也要防积压：

```text
如果解码线程忙：
    只保留最新待解码帧
```

否则网络不卡，解码堆起来，也会显示旧画面。

---

## 6. Codec 抽象

建议先定义一层 codec 接口，让 JPEG 和 H.264 都接同一个上层。

比如：

```cpp
class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;
    virtual bool open(const AppConfig& config) = 0;
    virtual QByteArray encode(const QImage& image, quint32 sampleId) = 0;
    virtual void close() = 0;
};
```

```cpp
class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;
    virtual bool open(const AppConfig& config) = 0;
    virtual QImage decode(const QByteArray& bytes, quint32& sampleId) = 0;
    virtual void close() = 0;
};
```

然后实现：

```text
JpegVideoEncoder
JpegVideoDecoder
H264VideoEncoder
H264VideoDecoder
```

这样上层不需要关心具体编码格式。

---

## 7. H.264 最少需要知道什么

你不需要一开始理解 H.264 全部细节，但必须知道这几个概念。

### 7.1 I 帧

I 帧可以独立解码，类似一张完整图片。

```text
优点：丢包后容易恢复
缺点：比较大
```

### 7.2 P 帧

P 帧依赖之前的帧，只记录变化。

```text
优点：很小
缺点：前面的关键数据丢了，后面可能解不出来
```

### 7.3 GOP

GOP 是一组帧，比如：

```text
I P P P P P P P I P P P ...
```

如果 GOP=60，60fps 下大概每 1 秒一个 I 帧。

实时远控建议：

```text
GOP = fps * 1
```

也就是大约 1 秒一个关键帧。  
如果网络很差，可以更短，比如 0.5 秒一个关键帧。

### 7.4 B 帧

B 帧会增加编码延迟。  
实时低延迟场景建议禁用：

```text
max_b_frames = 0
```

### 7.5 Annex B

H.264 裸流常见格式是 Annex B，里面用 start code 分隔 NAL：

```text
00 00 00 01
```

建议项目初期直接传 Annex B 格式的 H.264 数据，简单。

---

## 8. FFmpeg 编码最少要会哪些 API

需要用到这些库：

```text
libavcodec
libavutil
libswscale
```

编码大概流程：

```text
avcodec_find_encoder_by_name("libx264")
    或 avcodec_find_encoder(AV_CODEC_ID_H264)

avcodec_alloc_context3()
设置宽高、fps、码率、像素格式、GOP、低延迟参数
avcodec_open2()

每一帧：
    QImage(BGRA/RGB)
        -> sws_scale 转成 YUV420P
        -> avcodec_send_frame()
        -> avcodec_receive_packet()
        -> QByteArray

退出：
    avcodec_free_context()
    av_frame_free()
    av_packet_free()
    sws_freeContext()
```

关键配置方向：

```cpp
ctx->width = width;
ctx->height = height;
ctx->time_base = AVRational{1, fps};
ctx->framerate = AVRational{fps, 1};
ctx->pix_fmt = AV_PIX_FMT_YUV420P;
ctx->gop_size = fps;
ctx->max_b_frames = 0;
ctx->bit_rate = targetBitrate;
```

如果使用 `libx264`，建议加：

```cpp
av_opt_set(ctx->priv_data, "preset", "ultrafast", 0);
av_opt_set(ctx->priv_data, "tune", "zerolatency", 0);
```

初期建议参数：

```text
preset=ultrafast
tune=zerolatency
max_b_frames=0
gop_size=fps
bitrate=2Mbps ~ 8Mbps
```

---

## 9. FFmpeg 解码最少要会哪些 API

解码流程：

```text
avcodec_find_decoder(AV_CODEC_ID_H264)
avcodec_alloc_context3()
avcodec_open2()

每个 H.264 packet：
    av_packet_from_data 或 av_new_packet + memcpy
    avcodec_send_packet()
    avcodec_receive_frame()
    sws_scale 转成 QImage 支持的格式
```

解码输出通常是 YUV，需要转成 GUI 能显示的：

```text
AV_PIX_FMT_YUV420P -> QImage::Format_RGB888 / Format_ARGB32
```

---

## 10. H.264 在当前 UDP 层怎么传

不要一开始就做 RTP。当前项目可以先继续使用你的 `UdpFrame` 分片逻辑：

```text
一帧编码后的 H.264 数据
    -> VideoSampleCodec 封装 sampleId、时间戳、codec
    -> UdpFragmenter 分片
    -> UdpFrameReassembler 重组
    -> H264VideoDecoder 解码
```

`VideoSampleCodecType` 需要新增：

```cpp
H264
```

`VideoSample` 的 `data` 放编码后的 H.264 bytes。

初期可以把“编码器一次输出的所有 AVPacket”合并成一个 sample：

```text
sample.data = packet1 + packet2 + ...
```

但更规范的是保留 packet 边界。  
第一版为了能跑，可以先简单合并 Annex B 裸流。

---

## 11. 丢帧策略

H.264 下丢帧要比 JPEG 小心。

JPEG：

```text
每帧独立，丢哪帧都行
```

H.264：

```text
P 帧依赖前面的帧
如果关键帧丢了，后面一串 P 帧可能都没法正确解码
```

所以需要：

```text
1. 发送队列仍然不能无限排
2. 如果发现解码失败或长时间没画面，guest 请求 key frame
3. host 收到 KeyFrameRequest 后，强制下一帧为 I 帧
```

当前项目里已经有：

```cpp
UdpFrameType::KeyFrameRequest
```

这很好，可以复用。

H.264 队列策略建议：

```text
正在发送的 sample 不打断
等待区只保留最新 sample
如果等待区覆盖了关键帧，要谨慎
如果 guest 请求关键帧，优先发送关键帧
```

---

## 12. CMake 接 FFmpeg 的方向

如果你电脑上已经有 FFmpeg 开发包，需要这些东西：

```text
include/
    libavcodec/avcodec.h
    libavutil/...
    libswscale/swscale.h

lib/
    avcodec.lib / libavcodec.dll.a
    avutil.lib / libavutil.dll.a
    swscale.lib / libswscale.dll.a

bin/
    avcodec-*.dll
    avutil-*.dll
    swscale-*.dll
```

MinGW 环境下通常链接 `.dll.a`。

CMake 可以先这样写：

```cmake
set(FFMPEG_ROOT "D:/ffmpeg")

target_include_directories(Media PUBLIC
        ${FFMPEG_ROOT}/include)

target_link_directories(Media PUBLIC
        ${FFMPEG_ROOT}/lib)

target_link_libraries(Media PUBLIC
        avcodec
        avutil
        swscale)
```

运行时要保证 DLL 能找到：

```text
1. 把 FFmpeg bin 加到 PATH
2. 或把 DLL 复制到 exe 同目录
```

---

## 13. 推荐落地计划

### 阶段 A：线程化 JPEG

目标：

```text
现有功能不变，只把编码/解码挪到 worker thread
```

验收：

```text
1. 画面正常
2. 日志中 captureToEncode / receiveToDecode 更稳定
3. GUI 不因编码/解码卡顿
```

### 阶段 B：codec 抽象

目标：

```text
上层只知道 VideoSampleCodecType
不直接依赖 JPEG/H.264 具体实现
```

验收：

```text
1. JPEG 仍能跑
2. 切换 codec 不影响 MediaService / Udp 层
```

### 阶段 C：接 FFmpeg H.264 软件编码

目标：

```text
QImage -> H.264 bytes
H.264 bytes -> QImage
```

验收：

```text
1. 本地 encode/decode 单元测试能跑
2. 不走网络时能显示画面
3. 日志能看到编码/解码耗时
```

### 阶段 D：接入网络

目标：

```text
H.264 sample 走现有 UDP 分片/重组
```

验收：

```text
1. 低分辨率下稳定显示
2. 端到端延迟明显低于 JPEG
3. 接收比例提高
```

### 阶段 E：关键帧请求

目标：

```text
guest 解码失败或长时间没收到可显示帧时，请求 host 发关键帧
```

验收：

```text
1. 网络丢包后画面能恢复
2. 不需要重启连接
```

---

## 14. 第一版不要做的事情

这些先不要做：

```text
1. 不要一开始就做 RTP/RTCP
2. 不要一开始就做硬件编码
3. 不要一开始就做复杂码率自适应
4. 不要把 FFmpeg 代码直接塞进现有 VideoEncoder.cpp
5. 不要让编码/解码队列无限增长
```

第一版最重要的是：

```text
结构清楚
能跑
能测
能通过日志判断瓶颈
```

---

## 15. 总结

推荐最终结构：

```text
ScreenVideoSource
    -> VideoEncodeWorker 线程
        -> JpegVideoEncoder / H264VideoEncoder
    -> MediaService
    -> UdpPacketQueue
    -> UDP
    -> UdpFrameReassembler
    -> MediaService
    -> VideoDecodeWorker 线程
        -> JpegVideoDecoder / H264VideoDecoder
    -> VideoWidget
```

核心原则：

```text
1. 编码/解码不能阻塞 GUI
2. 视频队列不能无限排
3. 低延迟场景宁愿丢旧帧，也不能显示旧帧
4. H.264 要支持关键帧恢复
5. 每一步都用日志验证，不靠感觉
```
