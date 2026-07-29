可以，下面是更新后的完整架构总结，已经把 **服务器总类、TCP/UDP 服务拆分、消息解析复用、Dispatcher 分端处理、生命周期管理** 都合并进去。

**总体目标**

```text
服务器只做信令和 NAT 协调，不传视频/音频/按键业务数据。
Host 和 Client 之间通过 UDP P2P 直连传输视频、音频、输入。
如果 P2P 失败，则提示失败，不做服务器中转。
```

整体链路：

```text
Host   <--TCP 信令--> Server <--TCP 信令--> Client
Host   <=========== UDP P2P ============> Client
```

服务器实际有两个入口：

```text
TCP 9000：信令、房间、交换 endpoint
UDP 9001：NAT 探测，发现客户端公网 UDP endpoint
```

**分层结构**

```text
UI 层
  只负责显示、按钮、日志、状态

Coordinator 层
  负责整体流程编排和状态机

Service 层
  负责具体能力：信令、房间、打洞、媒体、输入、统计

Dispatcher 层
  负责把已解析消息分发成具体事件

Protocol 层
  负责消息结构、序列化、反序列化

System 层
  Qt Socket、Timer、Thread、Windows API、FFmpeg
```

**推荐目录**

```text
RemoteControl/
├── Common/
│   ├── NetworkTypes.h
│   ├── SignalingMessage.h/.cpp
│   ├── SignalingCodec.h/.cpp
│   ├── UdpPacket.h/.cpp
│   ├── UdpPacketCodec.h/.cpp
│   ├── MediaPacket.h/.cpp
│   └── InputPacket.h/.cpp
│
├── ServerApp/
│   ├── main.cpp
│   ├── SignalingServerApp.h/.cpp
│   ├── ServerConfig.h
│   └── Stats/
│       ├── StatsCollector.h/.cpp
│       ├── StatsStore.h/.cpp
│       └── StatsTypes.h
│
├── Signaling/
│   ├── Server/
│   │   ├── SignalingServer.h/.cpp
│   │   ├── NatProbeServer.h/.cpp
│   │   ├── ServerSignalingDispatcher.h/.cpp
│   │   └── RoomManager.h/.cpp
│   │
│   └── Client/
│       ├── SignalingClient.h/.cpp
│       └── ClientSignalingDispatcher.h/.cpp
│
├── Transport/
│   ├── UdpHolePuncher.h/.cpp
│   ├── UdpP2pTransport.h/.cpp
│   └── MediaTransport.h/.cpp
│
├── SessionController/
│   └── P2pSessionController.h/.cpp
│
├── Video/
├── Input/
└── UI/
```

**服务器端对象关系**

服务器端用一个总类作为组合根：

```cpp
class SignalingServerApp : public QObject {
    Q_OBJECT

public:
    explicit SignalingServerApp(QObject *parent = nullptr);

    bool start(const ServerConfig &config);
    void stop();

private:
    RoomManager roomManager;
    SignalingServer signalingServer;       // TCP 信令
    NatProbeServer natProbeServer;         // UDP 探测
    ServerSignalingDispatcher dispatcher;
    StatsCollector statsCollector;
};
```

`SignalingServerApp` 只负责：

```text
创建对象
持有对象
连接信号槽
启动/停止
生命周期管理
```

它不负责：

```text
socket 读写细节
消息解析
消息分发
房间逻辑
统计逻辑
```

**服务器核心职责**

```text
SignalingServer
  监听 TCP 9000
  接收客户端 TCP 信令
  调用 SignalingCodec 解码
  把消息交给 ServerSignalingDispatcher
  向客户端发送信令消息

NatProbeServer
  监听 UDP 9001
  接收客户端 UDP Probe
  从 senderAddress/senderPort 得到公网 endpoint
  回 ProbeAck
  通知 RoomManager 更新该客户端 UDP endpoint

RoomManager
  创建房间
  加入房间
  保存 Host/Client 状态
  保存 TCP socket/clientId
  保存 UDP endpoint
  双方 endpoint 准备好后触发交换

StatsCollector
  旁路订阅事件
  统计房间数、连接数、打洞成功率、失败原因等
```

**客户端核心职责**

```text
SignalingClient
  连接服务器 TCP 9000
  发送 CreateRoom / JoinRoomResult / P2pResultReport
  接收服务器信令
  调用 SignalingCodec 解码
  把消息交给 ClientSignalingDispatcher

ClientSignalingDispatcher
  把 RoomCreated、PeerJoined、PeerEndpoint、Error 等消息分发成 signal

UdpHolePuncher
  持有客户端自己的唯一 QUdpSocket
  向服务器 UDP 9001 probe
  收到 peer endpoint 后持续 punch
  尝试 peerPort 附近端口
  收到对方包后锁定真实 endpoint

UdpP2pTransport
  P2P 成功后的 UDP 通道封装
  heartbeat
  sendDatagram
  datagramReceived

MediaTransport
  在 P2P 通道上收发视频、音频、输入业务包
  处理 sequence、分片、丢包、关键帧请求

P2pSessionController
  客户端总控状态机
  编排连接服务器、创建/加入房间、UDP 探测、打洞、启动视频/输入
```

**协议复用原则**

服务器和客户端应该复用：

```text
SignalingMessage
SignalingCodec
UdpPacket
UdpPacketCodec
NetworkTypes
错误码
公共 enum
endpoint 类型
```

也就是：

```text
Common/ 里的协议结构和 encode/decode 两端共用
```

但 Dispatcher 建议分开：

```text
ServerSignalingDispatcher
ClientSignalingDispatcher
```

原因是同一种消息在两端含义不同：

```text
CreateRoom
  客户端：发送请求
  服务器：处理请求

PeerEndpoint
  服务器：构造并发送
  客户端：收到后开始打洞
```

**消息流**

TCP 信令：

```text
Socket 收到 bytes
  -> SignalingCodec::decode()
  -> SignalingDispatcher::dispatch()
  -> RoomManager / P2pSessionController / StatsCollector 等对象处理
```

UDP 探测：

```text
NatProbeServer 收到 UDP datagram
  -> 读取 senderAddress / senderPort
  -> 得到客户端公网 endpoint
  -> 通知 RoomManager
  -> 必要时通过 SignalingServer 发 PeerEndpoint 给双方
```

UDP P2P：

```text
UdpHolePuncher 收到 datagram
  -> UdpPacketCodec::decode()
  -> 如果是 Punch/PunchAck，锁定 peer endpoint
  -> 如果是业务数据，交给 UdpP2pTransport / MediaTransport
```

**消息类型划分**

TCP 信令：

```text
CreateRoom
RoomCreated
JoinRoomResult
JoinRoomResult
PeerJoined
UdpEndpointReady
PeerEndpoint
P2pResultReport
Error
```

UDP 控制：

```text
Probe
ProbeAck
Punch
PunchAck
Heartbeat
Disconnect
```

UDP 业务：

```text
Video
Audio
Input
KeyFrameRequest
```

**按键传输**

按键走 UDP P2P，不走服务器 TCP。

原因：

```text
延迟低
服务器不参与业务数据
符合低成本目标
```

可靠性增强：

```text
KeyDown / KeyUp 带 sequence
接收端去重
周期发送 KeyStateSync
超时自动释放未同步按键
```

**打洞流程**

```text
1. Host/Client TCP 连接服务器
2. Host 创建房间
3. Client 加入房间
4. 双方各自用自己的同一个 UDP socket 向服务器 UDP probe
5. NatProbeServer 记录双方公网 endpoint
6. RoomManager 判断双方 endpoint 是否准备好
7. SignalingServer 通过 TCP 向双方发送 PeerEndpoint
8. 双方同时向对方 endpoint 附近端口持续 punch
9. 收到对方 Punch/PunchAck 后锁定真实 endpoint
10. 进入 UDP P2P 通信
11. 启动视频、音频、输入传输
12. 客户端向服务器上报 P2pResultReport
```

推荐 punch 策略：

```text
每 50ms 一轮
尝试 peerPort - 16 到 peerPort + 16
持续 3-5 秒
收到任意 peer 包后立即锁定 endpoint
```

**生命周期管理**

```text
QObject 子对象：
  优先 parent 管理，或作为值成员

Socket / Timer：
  谁创建谁 stop/close

线程 worker：
  quit + wait + deleteLater

Windows hook：
  谁安装谁卸载

FFmpeg context：
  RAII 包装，析构释放
```

推荐原则：

```text
1. 能用值成员就用值成员
2. QObject 子对象用 parent 管理
3. 跨线程对象用 deleteLater
4. 总控类只编排，不释放别人内部资源
5. Service 自己释放自己创建的资源
```

**一句话原则**

```text
Protocol 管格式
Codec 管序列化
Dispatcher 管分发
Service 管能力
Coordinator 管流程
App 总类管装配和生命周期
UI 管展示
```

按这个结构做，你后面加服务器统计、后台管理、账号系统、版本上报、连接质量分析，都可以作为旁路模块订阅事件，不会把核心信令和打洞逻辑搅乱。