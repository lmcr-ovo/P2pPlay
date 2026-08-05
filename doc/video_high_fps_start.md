# 高帧率低延迟视频传输快速上手

目标：让本项目从当前的伪视频 `"hello"`，逐步升级成真正的高帧率、低延迟远程画面传输。

你现在已经有了比较好的底座：

```text
P2pSession
  负责 UDP P2P 打洞和数据传输

MediaService
  负责媒体业务权限和媒体帧分发

VideoCapturer
  负责产生视频数据

VideoRender
  负责接收并显示视频数据
```

后面真正的视频链路应该长这样：

```text
Host:
屏幕采集 -> 图像预处理 -> 视频编码 -> VideoFramePayload -> MediaService -> P2pSession -> UDP

Guest:
UDP -> P2pSession -> MediaService -> VideoFramePayload -> 视频解码 -> 渲染
```

## 1. 先建立正确认知

高 FPS 不是单纯靠 UDP 发得快。

真正决定视频效果的是这几件事：

```text
采集速度
编码速度
单帧大小
网络带宽
发送队列
丢包处理
接收端解码速度
渲染速度
端到端延迟控制
```

你现在的 UDP 底层已经能支撑媒体传输，但如果每帧 JPEG 有几百 KB，再跑 50 FPS，带宽和 CPU 很快会炸。

所以要把目标拆开：

```text
第一阶段：能显示真实画面
第二阶段：稳定 30/50 FPS
第三阶段：低延迟 H.264
第四阶段：GPU 采集/编码/解码/渲染优化
```

## 2. 你必须掌握的技术地图

### 屏幕采集

用于从 Host 电脑拿到画面。

简单路线：

```text
QScreen::grabWindow(0)
```

优点：

```text
Qt 自带
实现简单
适合第一版跑通真实画面
```

缺点：

```text
性能一般
高 FPS 压力大
指定窗口能力有限
```

高性能 Windows 路线：

```text
Desktop Duplication API
Windows.Graphics.Capture
```

Desktop Duplication API 基于 DXGI，适合远程桌面、屏幕协作类场景，可以拿到桌面帧更新，并且数据在 DXGI surface 中，后续可以接 GPU 处理。

Windows.Graphics.Capture 是较新的 Windows 捕获 API，可以捕获显示器或应用窗口，支持系统选择器，适合窗口捕获和现代 Windows 应用。

### 图像格式

屏幕采集得到的通常是：

```text
RGB / BGRA
```

视频编码通常需要：

```text
YUV420P / NV12
```

所以中间会有格式转换：

```text
BGRA -> NV12
BGRA -> YUV420P
```

如果用 JPEG，可以暂时跳过复杂 YUV 流程。

如果用 H.264，迟早要理解 YUV/NV12。

### 视频编码

编码就是把原始图像压缩成适合网络传输的数据。

第一版：

```text
JPEG / MJPEG
```

优点：

```text
实现简单
每帧独立
丢一帧不影响下一帧
```

缺点：

```text
码率巨大
50 FPS 压力很大
画质/带宽比差
```

正式路线：

```text
H.264
H.265
AV1
```

远控项目优先推荐：

```text
H.264 low latency
```

原因：

```text
兼容性好
延迟低
硬件编码支持广
解码支持广
```

### 硬件编码

高 FPS + 低延迟基本需要硬件编码。

常见硬件编码：

```text
NVIDIA NVENC
Intel Quick Sync
AMD AMF
Windows Media Foundation
FFmpeg 硬件加速封装
```

你不一定要自己写 CUDA 或 GPU shader。

更实际的路线是：

```text
调用现成硬件编码 API
```

比如：

```text
FFmpeg + h264_nvenc
FFmpeg + h264_qsv
FFmpeg + h264_amf
Media Foundation H.264 encoder
NVIDIA Video Codec SDK
```

### 网络传输

你已经有：

```text
UDP 分片
UDP 合片
P2P 打洞
发送队列
channel/type 区分
```

视频层还要补：

```text
视频帧号
时间戳
关键帧标记
丢帧统计
关键帧请求
码率/FPS 控制
```

### 视频解码

Guest 收到压缩数据后要解码。

JPEG：

```text
QImage::loadFromData
```

H.264：

```text
FFmpeg 解码
硬件解码
Media Foundation 解码
```

### 渲染

第一版：

```text
QWidget + QPainter
```

中期：

```text
QOpenGLWidget
```

高性能：

```text
Direct3D / OpenGL / Vulkan / Qt RHI
```

第一版先用 QWidget 足够。

## 3. 是否需要 GPU 编程

短答案：

```text
第一版不需要。
要做高 FPS、低 CPU、低延迟，后面需要使用 GPU 能力。
但不一定要自己写复杂 GPU 程序。
```

你可以分三层理解：

### 不使用 GPU

```text
QScreen 抓屏
CPU JPEG 编码
QImage 解码
QPainter 渲染
```

适合：

```text
快速跑通
学习链路
验证架构
```

不适合：

```text
长期 1080p 50 FPS
低 CPU 占用
高画质低码率
```

### 使用 GPU API，但不自己写 shader

```text
Desktop Duplication API
Windows.Graphics.Capture
NVENC
Media Foundation
FFmpeg 硬件编码
```

这是最推荐路线。

你是在调用系统或显卡厂商提供的硬件能力，不是从零写 GPU 编程。

### 深入 GPU 编程

```text
Direct3D texture
CUDA
OpenGL texture
shader
zero-copy pipeline
```

这是后期优化路线。

目标是减少拷贝：

```text
GPU 采集 texture
  -> GPU 编码
  -> 网络发送
  -> GPU 解码
  -> GPU 渲染
```

这条链路性能最好，但复杂度也最高。

## 4. 视频帧协议应该怎么设计

不要直接把图片 bytes 当作最终协议。

建议定义：

```cpp
enum class VideoSampleCodecType : quint16 {
    Unknown = 0,
    Jpeg = 1,
    H264 = 2,
    H265 = 3,
    AV1 = 4
};

struct VideoFramePayload {
    quint32 videoSeq = 0;
    qint64 captureTimestampMs = 0;

    quint16 width = 0;
    quint16 height = 0;

    quint16 fps = 0;
    VideoSampleCodecType codec = VideoSampleCodecType::Unknown;

    bool keyFrame = false;
    quint32 flags = 0;

    QByteArray data;
};
```

字段含义：

```text
videoSeq
  视频业务帧号，用于统计丢帧、乱序、延迟

captureTimestampMs
  Host 采集时间，用于计算端到端延迟

width / height
  解码和渲染需要

fps
  当前发送目标 FPS

codec
  data 是 JPEG、H.264 还是其他格式

keyFrame
  JPEG 每帧都可以看成 key frame
  H.264 里 IDR 帧是 key frame

flags
  预留字段，例如旋转、HDR、鼠标指针叠加等

data
  编码后的真实视频数据
```

注意：

```text
UdpPacket.frameSeq 是传输层帧号
VideoFramePayload.videoSeq 是视频业务帧号
不要混用
```

## 5. 建议的类结构

建议后面拆成：

```text
Media/
  VideoFramePayload.h
  VideoFrameCodec.h/.cpp

  ScreenCapturer.h/.cpp

  IVideoEncoder.h
  JpegVideoEncoder.h/.cpp
  H264VideoEncoder.h/.cpp

  IVideoDecoder.h
  JpegVideoDecoder.h/.cpp
  H264VideoDecoder.h/.cpp

  VideoRender.h/.cpp
  VideoWidget.h/.cpp
```

第一版可以少一点：

```text
Media/
  VideoFramePayload
  VideoFrameCodec
  ScreenCapturer
  JpegVideoEncoder
  JpegVideoDecoder
  VideoRender
```

## 6. 当前项目中的数据流

Host：

```text
ScreenCapturer
  -> JpegVideoEncoder / H264VideoEncoder
  -> VideoFrameCodec::encode
  -> MediaService::sendVideoFrame
  -> emit mediaFrameToSend(VideoFrame, payload)
  -> P2pSession::sendMediaFrame
  -> P2pUdpTransport::sendFrame(Media, VideoFrame, payload)
```

Guest：

```text
P2pUdpTransport::frameReady
  -> P2pSession::mediaFrameReceived
  -> MediaService::onMediaFrameReceived
  -> MediaService::videoFrameReceived
  -> VideoFrameCodec::decode
  -> JpegVideoDecoder / H264VideoDecoder
  -> VideoRender / VideoWidget
```

这个架构是可扩展到 H.264 的。

重点是：

```text
MediaService 不关心 JPEG/H264
P2pSession 不关心 JPEG/H264
UDP 层不关心 JPEG/H264
```

只有 Encoder/Decoder 关心具体编码格式。

## 7. 第一版真实视频建议路线

虽然你目标是 50 FPS，但第一版建议这样写：

```text
采集：QScreen::grabWindow(0)
分辨率：1280x720
编码：JPEG
质量：45-60
FPS：先 30，再试 50
渲染：QWidget + QPainter
```

Host 采集伪代码：

```cpp
QScreen* screen = QGuiApplication::primaryScreen();
QPixmap pixmap = screen->grabWindow(0);
QImage image = pixmap.toImage();

QImage scaled = image.scaled(
        1280,
        720,
        Qt::KeepAspectRatio,
        Qt::FastTransformation
);

QByteArray jpeg;
QBuffer buffer(&jpeg);
buffer.open(QIODevice::WriteOnly);
scaled.save(&buffer, "JPG", 50);
```

Guest 解码伪代码：

```cpp
QImage image;
image.loadFromData(frame.data, "JPG");
videoWidget->setImage(image);
```

如果 50 FPS 跑不动，先不要怀疑 UDP。

先看：

```text
单帧 JPEG 多大
采集耗时
JPEG 编码耗时
接收端解码耗时
渲染耗时
```

## 8. 50 FPS 的性能预算

50 FPS 意味着：

```text
每帧只有 20ms
```

一帧里要完成：

```text
采集
缩放
编码
发送
网络传输
接收
解码
渲染
```

所以你需要记录这些时间：

```text
captureCostMs
encodeCostMs
sendQueueDelayMs
networkDelayMs
decodeCostMs
renderCostMs
endToEndLatencyMs
```

第一版可以在 payload 中带：

```text
captureTimestampMs
```

Guest 收到后：

```cpp
qint64 now = QDateTime::currentMSecsSinceEpoch();
qint64 latency = now - frame.captureTimestampMs;
```

## 9. 高 FPS 必须避免的坑

### 不要堆积旧帧

视频和文件传输不同。

视频帧过期后就没有意义。

如果编码/发送跟不上，应该丢旧帧：

```text
宁可丢帧，也不要延迟越来越高
```

### 不要在 UI 线程做重编码

第一版能先跑，但后面应该拆线程：

```text
采集/编码线程
网络线程
UI 渲染线程
```

### 不要用过大的 JPEG

例如：

```text
1080p JPEG quality 90 * 50 FPS
```

这会非常大。

先用：

```text
720p
quality 45-60
```

### 不要把每一帧都可靠传输

UDP 视频不应该像文件一样追求每包必达。

如果某帧丢了：

```text
JPEG: 直接等下一帧
H.264: 必要时请求关键帧
```

## 10. H.264 扩展路线

当 JPEG 跑通后，下一步换 H.264。

架构不需要推倒，只需要换：

```text
JpegVideoEncoder -> H264VideoEncoder
JpegVideoDecoder -> H264VideoDecoder
codec = H264
keyFrame 正式启用
```

H.264 低延迟参数需要关注：

```text
关闭 B 帧
低延迟 preset
较短 GOP
支持强制关键帧
控制码率
控制 VBV buffer
```

概念：

```text
I 帧 / IDR 帧
  可独立解码，关键帧

P 帧
  依赖之前的帧

B 帧
  依赖前后帧，延迟更高，远控通常尽量不用

GOP
  两个关键帧之间的间隔

码率
  每秒视频数据大小
```

远控场景推荐：

```text
H.264
无 B 帧
低延迟模式
动态码率
关键帧请求
```

## 11. KeyFrameRequest 的作用

Guest 在这些情况发：

```text
刚连接成功
解码失败
检测到丢帧严重
长时间没收到关键帧
画面花屏
```

Host 收到：

```text
MediaService::keyFrameRequestReceived
  -> encoder.forceKeyFrame()
```

下一帧：

```text
VideoFramePayload.keyFrame = true
codec = H264
data = IDR frame
```

## 12. 渲染器如何接 UI

建议做一个 QWidget：

```cpp
class VideoWidget : public QWidget {
    Q_OBJECT

public slots:
    void setImage(const QImage& image);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QImage image_;
};
```

接收流程：

```text
MediaService::videoFrameReceived
  -> VideoDecoder::onVideoSampleBytesReceived
  -> emit imageReady(QImage)
  -> VideoWidget::setImage
  -> update()
  -> paintEvent
```

第一版用 QWidget 就够。

后面如果追求更高性能：

```text
QOpenGLWidget
Direct3D texture
GPU decode texture 直接渲染
```

## 13. 是否支持指定窗口采集

支持，但路线不同。

简单 Qt 方式：

```cpp
QScreen::grabWindow(windowId)
```

如果抓整个屏幕：

```cpp
grabWindow(0)
```

如果抓 Qt 自己窗口：

```cpp
WId id = widget->winId();
screen->grabWindow(id);
```

如果抓其他应用窗口，Windows 下需要：

```text
EnumWindows
FindWindow
GetWindowText
GetWindowRect
HWND -> WId
```

但注意：

```text
最小化窗口可能抓不到
受保护窗口可能黑屏
硬件加速窗口可能表现不稳定
权限不同可能抓不到
```

高性能指定窗口采集建议使用：

```text
Windows.Graphics.Capture
```

## 14. 推荐学习顺序

按这个顺序学，不容易乱：

```text
1. QImage / QPixmap / QScreen
2. JPEG 编码与 QBuffer
3. VideoFramePayload 协议
4. QWidget/QPainter 渲染
5. 采集/编码/渲染耗时统计
6. 丢帧与延迟控制
7. H.264 基础概念
8. FFmpeg 基础
9. 硬件编码 NVENC / QSV / AMF
10. Desktop Duplication API 或 Windows.Graphics.Capture
11. GPU texture zero-copy
```

## 15. 当前项目下一步任务

建议你下一步这样写：

```text
1. 写 VideoFramePayload / VideoFrameCodec
2. VideoCapturer 从 hello 改成 QScreen 抓图
3. 先 JPEG 编码
4. VideoRender 解 JPEG 并显示到 QWidget
5. 加 FPS 和 latency 日志
6. 尝试 30 FPS
7. 尝试 50 FPS
8. 再决定是否进入 H.264
```

不要一开始就写完整 FFmpeg。

先让真实画面在你的 P2P 链路里跑起来。

## 16. 参考资料

Microsoft Desktop Duplication API：

```text
https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api
```

Microsoft Windows.Graphics.Capture：

```text
https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture
```

NVIDIA Video Codec SDK：

```text
https://developer.nvidia.com/video-codec-sdk
```

FFmpeg Hardware Acceleration：

```text
https://trac.ffmpeg.org/wiki/HWAccelIntro
```
