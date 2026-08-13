你问的是架构关系，不是构造语法。那我重新讲清楚。

现在客户端其实有三类“工作人员”：

```text
HostRoleService / GuestRoleService
  负责登录、建房、加房、发起打洞请求这些“身份/房间流程”

P2pSession
  负责 UDP 打洞和 P2P 链路

MediaService
  负责 P2P 连上以后，哪些媒体数据能发、哪些媒体数据能收
```

它们不是同一层。

**HostRoleService 是干什么的**

HostRoleService 只管“我是房主时，应该怎么和服务器走信令流程”。

它做这些事：

```text
TCP 连接成功
  -> 发送 Register
  -> 发送 CreateRoom

收到 RoomCreated
  -> 保存 roomId
  -> 通知 P2pSession：我的 roomId/clientId 是什么

收到 PeerJoined
  -> 说明客户进房了
  -> 发送 ProbeRequest 给服务器

收到 p2pReady
  -> 打印日志：P2P 已经连上
```

所以 HostRoleService 不碰 UDP，不碰媒体，不发视频。

它只负责：

```text
房主身份下的服务器信令流程
```

**GuestRoleService 是干什么的**

GuestRoleService 只管“我是客户时，应该怎么和服务器走信令流程”。

它做这些事：

```text
TCP 连接成功
  -> 发送 Register
  -> 发送 JoinRoom

启动时已经知道 roomId
  -> 通知 P2pSession：我的 roomId/clientId 是什么

收到 p2pReady
  -> 打印日志：P2P 已经连上
```

它也不碰 UDP，不碰媒体，不发输入。

它只负责：

```text
客户身份下的服务器信令流程
```

**P2pSession 是干什么的**

P2pSession 不关心你是 Host 还是 Guest。

它只知道：

```text
roomId
clientId
serverUdpEndpoint
peerEndpoint
```

它做这些事：

```text
收到 ProbePermitted
  -> 向服务器 UDP 发 Probe

收到 PeerEndpoint
  -> 向对端附近端口发 Punch

收到 Punch / PunchAck
  -> 标记 p2pReady

p2pReady 以后
  -> 可以发送/接收 UDP Frame
```

所以它是：

```text
P2P 链路层
```

**那 MediaService 为什么还要有 role**

因为 `HostRoleService / GuestRoleService` 管的是“信令角色”，不是“媒体权限”。

媒体层有自己的角色规则：

```text
Host:
  允许发送 VideoFrame
  允许发送 AudioFrame
  允许接收 InputEvent
  允许接收 KeyFrameRequest

Guest:
  允许接收 VideoFrame
  允许接收 AudioFrame
  允许发送 InputEvent
  允许发送 KeyFrameRequest
```

你看，这和 HostRoleService / GuestRoleService 不一样。

HostRoleService 负责：

```text
Host 怎么注册、建房、发 ProbeRequest
```

MediaService 负责：

```text
Host 能不能发视频
Guest 能不能发输入
```

所以 MediaService 有自己的 role 是合理的。

不是重复，而是两个维度：

```text
RoleService:
  管连接流程身份

MediaService:
  管媒体业务权限
```

**ClientApp 是怎么把它们串起来的**

ClientApp 是总装配器。

Host 模式下：

```text
ClientApp::startAsHost
  -> hostRoleService_.setClientId("Alice")
  -> mediaService_.setRole(Host)
  -> p2pSession_.setServerUdpEndpoint(...)
  -> p2pSession_.bind(localUdpPort)
  -> TCP 连接服务器
```

然后信号流是：

```text
SignalingClient 收到服务器消息
  -> ClientDispatcher 分发

RoomCreated
  -> HostRoleService::onRoomCreated

PeerJoined
  -> HostRoleService::onPeerJoined
  -> HostRoleService 发 ProbeRequest

ProbePermitted
  -> P2pSession::onProbePermitted

PeerEndpoint
  -> P2pSession::onPeerEndpoint

P2pSession::p2pReady
  -> HostRoleService::onP2pReady
  -> MediaService::onP2pReady
```

Guest 模式下：

```text
ClientApp::startAsGuest
  -> guestRoleService_.setClientInfo(roomId, "Bob")
  -> mediaService_.setRole(Guest)
  -> p2pSession_.setServerUdpEndpoint(...)
  -> p2pSession_.bind(localUdpPort)
  -> TCP 连接服务器
```

信号流是：

```text
SignalingClient connected
  -> GuestRoleService::onConnected
  -> Register + JoinRoom

ProbePermitted
  -> P2pSession::onProbePermitted

PeerEndpoint
  -> P2pSession::onPeerEndpoint

P2pSession::p2pReady
  -> GuestRoleService::onP2pReady
  -> MediaService::onP2pReady
```

**媒体数据怎么走**

发送媒体：

```text
Host 的屏幕采集模块
  -> MediaService::sendVideoSampleBytes(payload)
  -> MediaService 检查当前 role 是不是 Host
  -> emit udpMediaFrameToSend(VideoFrame, payload)
  -> P2pSession::sendMediaFrame(VideoFrame, payload)
  -> P2pUdpTransport 发 UDP
```

接收媒体：

```text
P2pUdpTransport 收到 UDP
  -> P2pSession::mediaFrameReceived(frame)
  -> MediaService::onUdpMediaFrameReceived(frame)
  -> MediaService 根据 frame.type 分发
  -> videoSampleBytesReceived(payload)
  -> 视频渲染模块
```

输入事件类似：

```text
Guest 捕获鼠标键盘
  -> MediaService::sendInputEvent(payload)
  -> P2pSession
  -> Host 收到 InputEvent
  -> MediaService::inputSampleBytesReceived
  -> 输入执行模块
```

**所以三个类的关系是**

```text
HostRoleService / GuestRoleService
  只负责“怎么进入房间并发起连接”

P2pSession
  只负责“UDP P2P 链路怎么建立和传输帧”

MediaService
  只负责“连上以后媒体/输入业务怎么按角色使用”
```

你可以理解成：

```text
RoleService = 开门流程
P2pSession = 两台机器之间的通道
MediaService = 通道里跑什么业务，以及谁能发什么
```

这不是重复，而是在分层。  
如果以后你做 UI，UI 只需要告诉 ClientApp：

```text
我要当 Host
我要当 Guest
```

后面这些工作人员各干各的。