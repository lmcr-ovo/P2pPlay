**媒体架构设计**

统一术语先定下来：

```text
UdpPacket
  UDP 传输层分片包，真实 writeDatagram/readDatagram 的单位

UdpFrame
  UDP 传输层逻辑单元，由多个 UdpPacket 合成

VideoSample
  媒体层视频样本，一次可解码的视频数据
  v1 中是一张 JPEG
  后续 H.264 中可以是一帧 access unit / IDR / P frame

SignalingMessage
  TCP 信令消息，只用于建房、入房、打洞协商
```

整体链路：

```text
Host:
ScreenVideoSource
  -> VideoEncoder
  -> VideoSample
  -> VideoSampleCodec
  -> MediaService
  -> P2pSession
  -> P2pUdpTransport
  -> UdpPacket

Guest:
UdpPacket
  -> P2pUdpTransport
  -> P2pSession
  -> MediaService
  -> VideoSampleCodec
  -> VideoDecoder
  -> VideoWidget
```

**核心分层**

```text
Udp 层
  只负责分片、合片、发送队列、peer endpoint
  不理解 JPEG/H264

P2pSession
  只负责 P2P 打洞状态和 UDP frame 收发
  不理解媒体内容

MediaService
  只负责媒体类型分发和角色权限
  Host 可发 Video/Audio
  Guest 可发 Input/KeyFrameRequest

VideoSenderPipeline
  Host 端视频发送流水线
  负责采集、编码、封装、发送节奏

VideoReceiverPipeline
  Guest 端视频接收流水线
  负责解封装、解码、丢旧帧、输出 QImage

VideoWidget
  只负责显示 QImage
  不接触网络、不解协议
```

**推荐文件结构**

```text
Media/
  VideoSample.h
  VideoSampleCodec.h/.cpp

  VideoSenderPipeline.h/.cpp
  VideoReceiverPipeline.h/.cpp

  IVideoSource.h
  ScreenVideoSource.h/.cpp

  IVideoEncoder.h
  JpegVideoEncoder.h/.cpp
  H264VideoEncoder.h/.cpp

  IVideoDecoder.h
  JpegVideoDecoder.h/.cpp
  H264VideoDecoder.h/.cpp

  VideoWidget.h/.cpp
```

v1 可以先少做：

```text
Media/
  VideoSample.h
  VideoSampleCodec.h/.cpp
  VideoSenderPipeline.h/.cpp
  VideoReceiverPipeline.h/.cpp
  VideoWidget.h/.cpp
```

**VideoSample 建议字段**

```cpp
enum class VideoSampleCodecType : quint8 {
    Unknown = 0,
    Jpeg = 1,
    H264 = 2
};

struct VideoSample {
    quint32 sequence = 0;
    quint64 captureTimestampMs = 0;
    quint16 width = 0;
    quint16 height = 0;
    VideoSampleCodecType codec = VideoSampleCodecType::Unknown;
    quint32 flags = 0;
    QByteArray data;
};
```

字段含义：

```text
sequence
  视频业务序号，用于统计丢帧和调试

captureTimestampMs
  粗略观察端到端延迟

width / height
  解码、渲染、日志、分辨率变化使用

codec
  data 的编码格式

flags
  预留关键帧、配置帧等标记

data
  编码后的视频数据
```

暂时可以不放 `fps`。FPS 是配置项，不是解码单帧必需字段。

**信号连接**

Host：

```text
P2pSession::p2pReady
  -> MediaService::onP2pReady
  -> VideoSenderPipeline::start

VideoSenderPipeline::sampleReady(QByteArray)
  -> MediaService::sendVideoFrame
  -> P2pSession::sendMediaFrame
```

Guest：

```text
P2pSession::mediaFrameReceived
  -> MediaService::onMediaFrameReceived

MediaService::videoFrameReceived(QByteArray)
  -> VideoReceiverPipeline::onVideoSampleBytesReceived(QByteArray)

VideoReceiverPipeline::imageReady(QImage)
  -> ClientApp::remoteImageReady(QImage)
  -> VideoWidget::setImage(QImage)
```

**ClientApp 角色管理**

同一个程序支持 Host/Guest，但启用不同 pipeline：

```text
Host:
  启用 VideoSenderPipeline
  不启用 VideoReceiverPipeline
  UI 左侧显示共享状态，不显示本机视频

Guest:
  不启用 VideoSenderPipeline
  启用 VideoReceiverPipeline
  UI 左侧显示远端视频
```

`ClientApp` 负责：

```text
读取 AppConfig
启动 Host/Guest 角色服务
连接 P2pSession 和 MediaService
按角色连接 sender/receiver pipeline
转发 remoteImageReady/log/error/status 给 MainWindow
```

`MainWindow` 负责：

```text
读取 UI 配置
调用 ClientApp::startAsHost / startAsGuest
显示 roomId、连接状态、日志
把 VideoWidget 放入 videoFrameLayout
连接 ClientApp::remoteImageReady 到 VideoWidget::setImage
```

**低延迟原则**

视频不能像文件一样排队保真。后续升级要遵守：

```text
宁可丢旧帧，不要堆积延迟
采集层只保留最新帧
编码层忙时跳过旧帧
网络层视频包可丢，控制包优先
接收层只解码最新完整样本
渲染层只显示最新图像
```

**升级路线**

```text
v1:
  JPEG + VideoSample + QWidget 显示
  目标：真实画面跑通

v2:
  VideoSenderPipeline / VideoReceiverPipeline 完整化
  LatestFrameBuffer
  UDP 视频队列丢旧帧
  目标：延迟稳定不增长

v3:
  H.264 软件编码/解码
  KeyFrameRequest
  目标：降低码率

v4:
  Desktop Duplication / Windows Graphics Capture
  NVENC/QSV/AMF/Media Foundation
  目标：高 FPS、低 CPU

v5:
  动态码率、动态 FPS、动态分辨率
  目标：弱网稳定可用
```

一句话总结：  
**信令叫 Message，传输叫 Packet/Frame，媒体叫 Sample；P2P 负责怎么送，MediaService 负责送什么类型，Pipeline 负责视频怎么生产和消费，Widget 只负责显示。**