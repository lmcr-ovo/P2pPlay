# Client 重构后架构与通信说明

## 1. 重构目标

客户端将 UDP 数据处理分为三类职责：

```text
P2pSessionWorker
    UDP socket、分片重组、P2P 打洞、P2P 状态

MediaServiceWorker
    VideoFrame、AudioFrame

ControlServiceWorker
    InputEvent、KeyFrameRequest、ReceiverReport
```

`P2pSession`、`MediaService`、`ControlService` 是线程对象的外层管理类；真正处理数据的对象是各自的 worker。

媒体帧和业务控制帧不经过 `P2pSession` 外层对象，直接使用 worker-to-worker 的 Qt queued connection，避免高频数据经过主线程。

## 2. UDP 协议分类

`UdpFrameType` 的数值是协议字段，不能因为重新分类而改变。当前协议应保持以下数值：

```cpp
enum class UdpFrameType : quint16 {
    Unknown = 0,

    // P2P session control
    Probe = 1,
    ProbeAck = 2,
    Punch = 3,
    PunchAck = 4,

    // Media
    VideoFrame = 5,
    AudioFrame = 6,

    // Application control
    InputEvent = 7,
    KeyFrameRequest = 8,
    ReceiverReport = 9
};
```

通道与帧类型的对应关系：

| 通道 | 帧类型 | 处理者 |
|---|---|---|
| `Control` | `Probe` | `P2pSessionWorker` |
| `Control` | `ProbeAck` | `P2pSessionWorker` |
| `Control` | `Punch` | `P2pSessionWorker` |
| `Control` | `PunchAck` | `P2pSessionWorker` |
| `Control` | `InputEvent` | `ControlServiceWorker` |
| `Control` | `KeyFrameRequest` | `ControlServiceWorker` |
| `Control` | `ReceiverReport` | `ControlServiceWorker` |
| `Media` | `VideoFrame` | `MediaServiceWorker` |
| `Media` | `AudioFrame` | `MediaServiceWorker` |

## 3. P2P 会话层

### 3.1 `P2pSessionWorker`

`P2pSessionWorker` 负责：

- UDP socket 绑定和收发；
- `P2pUdpTransport` 的帧接收；
- `Probe`、`ProbeAck`、`Punch`、`PunchAck`；
- 对端地址和端口锁定；
- `p2pReady` 状态；
- 将完整帧按通道分发给媒体服务或控制服务。

接收分发逻辑：

```cpp
void P2pSessionWorker::onFrameReady(const UdpFrame& frame) {
    if (frame.channelType == UdpChannelType::Media) {
        emit mediaFrameReceived(frame);
        return;
    }

    if (frame.channelType != UdpChannelType::Control) {
        return;
    }

    switch (frame.frameType) {
        case UdpFrameType::ProbeAck:
            emit logReceived("udp probe ack received");
            break;

        case UdpFrameType::Punch:
            handlePunch(frame);
            break;

        case UdpFrameType::PunchAck:
            handlePunchAck(frame);
            break;

        case UdpFrameType::InputEvent:
        case UdpFrameType::KeyFrameRequest:
        case UdpFrameType::ReceiverReport:
            emit controlFrameReceived(frame);
            break;

        default:
            emit logReceived("unknown control frame received");
            break;
    }
}
```

发送接口仍然在 `P2pSessionWorker`：

```cpp
void P2pSessionWorker::sendMediaFrame(
        UdpFrameType type,
        const QByteArray& payload) {
    transport_.sendFrame(
            UdpChannelType::Media,
            type,
            payload);
}

void P2pSessionWorker::sendControlFrame(
        UdpFrameType type,
        const QByteArray& payload) {
    transport_.sendFrame(
            UdpChannelType::Control,
            type,
            payload);
}
```

### 3.2 `P2pSession`

`P2pSession` 只负责：

- 创建和销毁 `P2pSessionWorker` 线程；
- 向 worker 投递配置、绑定和信令消息；
- 向主线程转发 P2P 状态、日志和错误。

它不应该再拥有以下数据通道接口：

```cpp
mediaFrameReceived
controlFrameReceived
sendMediaFrame
sendControlFrame
```

## 4. 媒体服务通信

### 4.1 发送媒体帧

```text
VideoEncoderWorker
    -> MediaServiceWorker::sendVideoSampleBytes
    -> MediaServiceWorker::udpMediaFrameToSend
    -> P2pSessionWorker::sendMediaFrame
    -> UdpChannelType::Media / VideoFrame
```

### 4.2 接收媒体帧

```text
UDP
    -> P2pSessionWorker::onFrameReady
    -> P2pSessionWorker::mediaFrameReceived
    -> MediaServiceWorker::onUdpMediaFrameReceived
    -> MediaServiceWorker::videoSampleBytesReceived
    -> VideoDecoderWorker::onVideoSampleBytesReceived
```

`ClientApp` 中对应的连接：

```cpp
connect(mediaService_.worker(),
        &MediaServiceWorker::udpMediaFrameToSend,
        p2pSession_.worker(),
        &P2pSessionWorker::sendMediaFrame);

connect(p2pSession_.worker(),
        &P2pSessionWorker::mediaFrameReceived,
        mediaService_.worker(),
        &MediaServiceWorker::onUdpMediaFrameReceived);
```

## 5. 控制服务通信

### 5.1 控制帧发送

```text
业务模块
    -> ControlServiceWorker::sendInputEvent
       或 sendKeyFrameRequest
       或 sendReceiverReport
    -> ControlServiceWorker::controlFrameToSend
    -> P2pSessionWorker::sendControlFrame
    -> UdpChannelType::Control
```

### 5.2 控制帧接收

```text
UDP
    -> P2pSessionWorker::onFrameReady
    -> P2pSessionWorker::controlFrameReceived
    -> ControlServiceWorker::onControlFrameReceived
```

`ClientApp` 中对应的连接：

```cpp
connect(controlService_.worker(),
        &ControlServiceWorker::controlFrameToSend,
        p2pSession_.worker(),
        &P2pSessionWorker::sendControlFrame);

connect(p2pSession_.worker(),
        &P2pSessionWorker::controlFrameReceived,
        controlService_.worker(),
        &ControlServiceWorker::onControlFrameReceived);
```

## 6. 关键帧请求

Guest 端：

```text
VideoDecoderWorker::keyFrameRequestNeeded
    -> ControlServiceWorker::sendKeyFrameRequest
    -> P2pSessionWorker::sendControlFrame
    -> UDP Control / KeyFrameRequest
```

Host 端：

```text
UDP Control / KeyFrameRequest
    -> P2pSessionWorker::controlFrameReceived
    -> ControlServiceWorker::onControlFrameReceived
    -> ControlServiceWorker::keyFrameRequestReceived
    -> VideoEncoderWorker::requestKeyFrame
```

Host 端连接：

```cpp
connect(controlService_.worker(),
        &ControlServiceWorker::keyFrameRequestReceived,
        videoSenderPipeline_.encoderWorker(),
        &VideoEncoderWorker::requestKeyFrame);
```

关键帧请求不应该再连接到 `MediaServiceWorker`。

## 7. 输入事件

Guest 发送输入事件：

```text
InputCapture
    -> InputSender
    -> InputServiceWorker::inputSampleBytesReady
    -> ControlServiceWorker::sendInputEvent
    -> P2pSessionWorker
```

Host 收到输入事件并回复 Ack：

```text
P2pSessionWorker
    -> ControlServiceWorker::inputEventReceived
    -> InputServiceWorker::onInputSampleBytesReceived
    -> InputReceiver
    -> InputServiceWorker::inputAckSampleBytesReady
    -> ControlServiceWorker::sendInputEvent
    -> P2pSessionWorker
```

## 8. ReceiverReport

Guest 端：

```text
ReceiverMonitorWorker::reportFrameBytesReady
    -> ControlServiceWorker::sendReceiverReport
    -> P2pSessionWorker::sendControlFrame
```

Host 端：

```text
P2pSessionWorker
    -> ControlServiceWorker::receiverReportReceived
    -> RateController::onReceiverReportBytesReceived
```

## 9. 线程关系

```text
主线程
    ClientApp
    P2pSession
    MediaService
    ControlService
    InputService

P2pSession线程
    P2pSessionWorker

MediaService线程
    MediaServiceWorker

ControlService线程
    ControlServiceWorker

InputService线程
    InputServiceWorker
```

媒体和控制数据连接必须使用 worker-to-worker：

```text
MediaServiceWorker <-> P2pSessionWorker
ControlServiceWorker <-> P2pSessionWorker
InputServiceWorker <-> ControlServiceWorker
```

不要使用 `P2pSession` 外层包装对象转发媒体帧或控制帧。

## 10. 当前迁移检查清单

以下旧接口不应再出现在代码中：

```text
MediaServiceWorker::udpControlFrameToSend
MediaServiceWorker::sendInputSampleBytes
MediaServiceWorker::inputSampleBytesReceived
MediaServiceWorker::sendKeyFrameRequest
MediaServiceWorker::keyFrameRequestReceived
P2pSessionWorker::keyFrameRequestReceived
P2pSessionWorker::receiverReportBytesReceived
P2pSession::mediaFrameReceived
P2pSession::controlFrameReceived
P2pSession::sendMediaFrame
P2pSession::sendControlFrame
```

注释中的旧接口也应删除，避免后续误判迁移状态。

## 11. 关闭和生命周期

启动流程：

```text
ClientApp 设置 MediaService / ControlService / InputService 角色
    -> P2pSessionWorker bind
    -> P2P 打洞成功
    -> P2pSessionWorker::p2pReady
    -> MediaServiceWorker::onP2pReady
    -> ControlServiceWorker::onP2pReady
    -> InputServiceWorker::start
```

服务线程的销毁顺序由各 Service 的析构函数负责：先退出线程并等待，再释放 worker。

