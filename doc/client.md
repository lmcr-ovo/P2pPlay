可以，客户端就按“服务器同款工作室架构”来搭，很顺手，而且以后 Host/Guest 两种角色不会互相搅在一起。

我建议客户端分成这几个模块：

```text
Client/
  ClientApp
  SignalingClient
  ClientDispatcher
  NatTraversalService
  HostRoleService
  GuestRoleService
```

职责这样分：

```text
SignalingClient
  TCP 收发室
  负责 connectToHost()
  负责 sendMessage()
  收到服务器信令后 emit messageReceived(message)

ClientDispatcher
  分发员
  根据 SignalingType 把消息分发给不同业务模块

NatTraversalService
  UDP 打洞工作人员
  绑定本地 UDP socket
  收到 ProbePermitted 后向服务器发 Probe
  收到 PeerEndpoint 后保存对端 endpoint
  开始向对端发送 Punch
  收到 Punch 后回复 PunchAck
  打洞成功后 emit p2pReady(address, port)

HostRoleService
  Host 业务
  注册
  创建房间
  收到 PeerJoined 后发 ProbeRequest
  收到 p2pReady 后进入媒体发送准备阶段

GuestRoleService
  Guest 业务
  注册
  加入房间
  收到 ProbePermitted 后配合 NatTraversalService 发 Probe
  收到 p2pReady 后进入接收媒体/发送输入准备阶段

ClientApp
  组装所有对象
  设置 role
  连接信号槽
```

核心事件流建议这样：

**Host 流程**

```text
1. SignalingClient TCP 连接服务器
2. HostRoleService 发送 Register
3. HostRoleService 发送 CreateRoom
4. Server 回复 RoomCreated
5. Guest 加入后，Server 发 PeerJoined
6. HostRoleService 收到 PeerJoined
7. HostRoleService 发送 ProbeRequest
8. Server 发 ProbePermitted 给 Host 和 Guest
9. NatTraversalService 收到 ProbePermitted
10. UDP -> Server: Probe(roomId, clientId)
11. Server TCP -> Host: PeerEndpoint(Guest endpoint)
12. NatTraversalService 收到 PeerEndpoint
13. 开始向 Guest endpoint 发 Punch
14. 收到 Punch/PunchAck 后 p2pReady
```

**Guest 流程**

```text
1. SignalingClient TCP 连接服务器
2. GuestRoleService 发送 Register
3. GuestRoleService 发送 JoinRoom
4. Server 回复 Log / Error
5. Server 发 ProbePermitted
6. NatTraversalService 收到 ProbePermitted
7. UDP -> Server: Probe(roomId, clientId)
8. Server TCP -> Guest: PeerEndpoint(Host endpoint)
9. NatTraversalService 收到 PeerEndpoint
10. 开始向 Host endpoint 发 Punch
11. 收到 Punch/PunchAck 后 p2pReady
```

比较关键的是：  
**`NatTraversalService` 不应该关心自己是 Host 还是 Guest。**

它只需要知道：

```cpp
clientId
roomId
serverUdpAddress
serverUdpPort
localUdpPort
```

然后它收到 `ProbePermitted` 就发：

```cpp
UdpProbePayload payload;
payload.roomId = roomId_;
payload.clientId = clientId_;

udpTransport_.sendFrameTo(
    serverUdpAddress_,
    serverUdpPort_,
    UdpChannelType::Control,
    UdpFrameType::Probe,
    UdpControlPayloadCodec::encodeProbe(payload)
);
```

收到 `PeerEndpoint` 后：

```cpp
udpTransport_.setPeerEndpoint(message.endpointAddress, message.endpointPort);
udpTransport_.setPeerFilterEnabled(true);
startPunch();
```

Punch payload：

```cpp
UdpPunchPayload payload;
payload.roomId = roomId_;
payload.clientId = clientId_;
```

发：

```cpp
udpTransport_.sendFrame(
    UdpChannelType::Control,
    UdpFrameType::Punch,
    UdpControlPayloadCodec::encodePunch(payload)
);
```

收到 UDP Punch：

```cpp
UdpPunchPayload payload;
if (!UdpControlPayloadCodec::decodePunch(frame.payload, &payload)) {
    return;
}

if (payload.roomId != roomId_) {
    return;
}

udpTransport_.setPeerEndpoint(frame.senderAddress, frame.senderPort);
udpTransport_.setPeerFilterEnabled(true);

UdpPunchAckPayload ack;
ack.roomId = roomId_;
ack.clientId = clientId_;

udpTransport_.sendFrame(
    UdpChannelType::Control,
    UdpFrameType::PunchAck,
    UdpControlPayloadCodec::encodePunchAck(ack)
);

emit p2pReady(frame.senderAddress, frame.senderPort);
```

收到 PunchAck：

```cpp
UdpPunchAckPayload payload;
if (!UdpControlPayloadCodec::decodePunchAck(frame.payload, &payload)) {
    return;
}

if (payload.roomId != roomId_) {
    return;
}

emit p2pReady(frame.senderAddress, frame.senderPort);
```

为了避免重复 emit，可以加：

```cpp
bool p2pReady_ = false;
```

然后：

```cpp
void NatTraversalService::markP2pReady(const QHostAddress& address, quint16 port)
{
    if (p2pReady_) {
        return;
    }

    p2pReady_ = true;
    emit p2pReady(address, port);
}
```

客户端模块之间的 connect 大概是：

```cpp
connect(&signalingClient_, &SignalingClient::messageReceived,
        &dispatcher_, &ClientDispatcher::onMessageReceived);

connect(&dispatcher_, &ClientDispatcher::roomCreated,
        &hostRoleService_, &HostRoleService::onRoomCreated);

connect(&dispatcher_, &ClientDispatcher::peerJoined,
        &hostRoleService_, &HostRoleService::onPeerJoined);

connect(&dispatcher_, &ClientDispatcher::probePermitted,
        &natTraversalService_, &NatTraversalService::onProbePermitted);

connect(&dispatcher_, &ClientDispatcher::peerEndpoint,
        &natTraversalService_, &NatTraversalService::onPeerEndpoint);

connect(&natTraversalService_, &NatTraversalService::p2pReady,
        &hostRoleService_, &HostRoleService::onP2pReady);

connect(&natTraversalService_, &NatTraversalService::p2pReady,
        &guestRoleService_, &GuestRoleService::onP2pReady);
```

不过最后两个可以根据 role 只连一个，避免 Guest 收 Host 的处理。

`SignalingClient` 可以这么设计：

```cpp
class SignalingClient : public QObject {
    Q_OBJECT

public:
    explicit SignalingClient(QObject* parent = nullptr);

    void connectToServer(const QHostAddress& address, quint16 port);
    bool sendMessage(const SignalingMessage& message);

signals:
    void connected();
    void disconnected();
    void messageReceived(const SignalingMessage& message);
    void errorOccurred(const QString& reason);

private:
    QTcpSocket socket_;
    SignalingConnection connection_;
};
```

但这里有个构造问题：`SignalingConnection` 当前构造必须传 `QTcpSocket*`，可以这样：

```cpp
SignalingClient::SignalingClient(QObject* parent)
    : QObject(parent),
      socket_(this),
      connection_(&socket_, this)
{
}
```

这个是可以的。

`ClientApp` 建议先别做太复杂，先支持命令式启动：

```cpp
bool startAsHost(
    const QString& clientId,
    const QHostAddress& serverTcpAddress,
    quint16 serverTcpPort,
    const QHostAddress& serverUdpAddress,
    quint16 serverUdpPort,
    quint16 localUdpPort
);

bool startAsGuest(
    const QString& clientId,
    const QString& roomId,
    const QHostAddress& serverTcpAddress,
    quint16 serverTcpPort,
    const QHostAddress& serverUdpAddress,
    quint16 serverUdpPort,
    quint16 localUdpPort
);
```

这样测试时就很舒服：

```text
client.exe host Alice 127.0.0.1 9000 127.0.0.1 9001 10000
client.exe guest Bob 123456 127.0.0.1 9000 127.0.0.1 9001 10001
```

我的建议实现顺序：

```text
1. 写 Client/CMakeLists.txt 
2. 写 SignalingClient
3. 写 ClientDispatcher
4. 写 NatTraversalService
5. 写 HostRoleService / GuestRoleService
6. 写一个 Client/test/testNatTraversalClient.cpp
7. 本机起 Server + 两个 Client 测完整打洞流程
```

第一版客户端不要加 UI，也不要碰媒体。  
只要做到这行日志出现，就算第一阶段胜利：

```text
P2P ready: peer=xxx.xxx.xxx.xxx:xxxxx
```

这个点一过，你的项目就从“协议组件”真正进入“端到端可联通系统”了。