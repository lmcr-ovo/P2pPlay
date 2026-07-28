你现在这个设计很适合按 **三步搭 UDP**，不要一上来就做视频/音频。先把“UDP 能互相打通”跑起来，再往上叠业务包。

建议顺序：

1. **Common：先定义 UDP 包格式**
2. **Server：做 `NatProbeServer`，只负责探测公网 endpoint**
3. **Client：做 `UdpHolePuncher`，复用同一个 `QUdpSocket` 完成 probe + punch + 后续 P2P**

核心思想是：**UDP 没有连接，谁给你发包，你就从 `senderAddress/senderPort` 看到它在公网侧的地址。**

---

**第一步：Common/UdpPacket**

建议先加：

```text
Common/
  UdpPacket.h
  UdpPacketCodec.h/.cpp
```

包类型可以这样：

```cpp
enum class UdpPacketType : quint16 {
    Unknown = 0,
    Probe = 1,
    ProbeAck = 2,
    Punch = 3,
    PunchAck = 4,
    Heartbeat = 5,
    Disconnect = 6,
    Video = 100,
    Audio = 101,
    Input = 102
};

struct UdpPacket {
    UdpPacketType type = UdpPacketType::Unknown;
    QString roomId;
    QString clientId;
    quint32 sequence = 0;
    QByteArray payload;
};
```

UDP Codec 可以类似你现在的 `TcpFrameCodec`：固定 `magic/version/type/sequence/payloadLength`，然后追加 payload。区别是 UDP 天然就是一整个 datagram，不需要像 TCP 那样处理粘包/半包。

---

**第二步：服务器 NatProbeServer**

服务器 UDP 只做一件事：

```text
bind 9001
收到 Probe
读取 senderAddress / senderPort
发 ProbeAck
通知 RoomManager：某个 clientId 的 UDP endpoint 准备好了
```

伪代码：

```cpp
class NatProbeServer : public QObject {
    Q_OBJECT

public:
    explicit NatProbeServer(QObject* parent = nullptr);

    bool start(quint16 port);
    void stop();

signals:
    void endpointDiscovered(
        const QString& roomId,
        const QString& clientId,
        const QHostAddress& address,
        quint16 port
    );

private slots:
    void onReadyRead();

private:
    QUdpSocket socket_;
};
```

关键实现逻辑：

```cpp
void NatProbeServer::onReadyRead() {
    while (socket_.hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(socket_.pendingDatagramSize()));

        QHostAddress senderAddress;
        quint16 senderPort = 0;

        socket_.readDatagram(
            data.data(),
            data.size(),
            &senderAddress,
            &senderPort
        );

        UdpPacket packet;
        if (!UdpPacketCodec::decode(data, packet)) {
            continue;
        }

        if (packet.type != UdpPacketType::Probe) {
            continue;
        }

        emit endpointDiscovered(
            packet.roomId,
            packet.clientId,
            senderAddress,
            senderPort
        );

        UdpPacket ack;
        ack.type = UdpPacketType::ProbeAck;
        ack.roomId = packet.roomId;
        ack.clientId = packet.clientId;

        socket_.writeDatagram(
            UdpPacketCodec::encode(ack),
            senderAddress,
            senderPort
        );
    }
}
```

注意：这里记录的 `senderAddress/senderPort` 才是对方的公网 UDP endpoint。

---

**第三步：客户端 UdpHolePuncher**

客户端重点：**probe 服务器和 punch 对方必须用同一个 `QUdpSocket`。**

不要这样：

```text
socketA -> 发 Probe
socketB -> 发 Punch
```

这样 NAT 映射可能不同，服务器看到的端口和你打洞用的端口不一致。

应该这样：

```text
同一个 QUdpSocket
  -> bind 本地任意端口
  -> 发 Probe 给 server:9001
  -> 收到 TCP 的 PeerEndpoint
  -> 对 peer endpoint 附近端口持续发 Punch
  -> 收到对方 Punch/PunchAck 后锁定 endpoint
```

建议类：

```cpp
class UdpHolePuncher : public QObject {
    Q_OBJECT

public:
    explicit UdpHolePuncher(QObject* parent = nullptr);

    bool bindLocal(quint16 preferredPort = 0);

    void sendProbe(
        const QHostAddress& serverAddress,
        quint16 serverUdpPort,
        const QString& roomId,
        const QString& clientId
    );

    void startPunch(
        const QHostAddress& peerAddress,
        quint16 peerPort,
        const QString& roomId,
        const QString& clientId
    );

    void stop();

signals:
    void p2pConnected(const QHostAddress& address, quint16 port);
    void packetReceived(const UdpPacket& packet);
    void errorOccurred(const QString& reason);

private slots:
    void onReadyRead();
    void onPunchTimeout();

private:
    QUdpSocket socket_;
    QTimer punchTimer_;

    QHostAddress peerAddress_;
    quint16 peerPort_ = 0;
    bool connected_ = false;

    QString roomId_;
    QString clientId_;
    quint32 sequence_ = 0;
};
```

打洞定时器：

```text
每 50ms 发一轮
端口范围 peerPort - 16 到 peerPort + 16
持续 3 到 5 秒
收到对方任意 Punch/PunchAck 就成功
```

---

**你应该先实现的最小闭环**

先别管视频，先跑这个：

```text
1. Host 创建房间
2. Client 加入房间
3. Host/Client 都向服务器 UDP 9001 发 Probe
4. 服务器记录双方 endpoint
5. 服务器通过 TCP 给双方发 PeerEndPoint
6. 双方 UdpHolePuncher 开始互发 Punch
7. 一方收到 Punch 后回 PunchAck
8. 双方打印：P2P connected
```

只要这一步能跑通，后面的 `Video/Input/Audio` 都只是 `UdpPacket.payload` 上面的业务层。

---

几个边界点你一定要记住：

```text
UDP 不保证送达
UDP 不保证顺序
UDP 不需要 accept/connect
一次 writeDatagram 对应一次 readDatagram
单个包别太大，视频后面要做分片
公网 endpoint 必须从服务器收到的 senderAddress/senderPort 获取
probe 和 p2p 必须尽量复用同一个 socket
```

按你现在项目结构，我建议下一步先加这 4 个文件：

```text
Common/UdpPacket.h
Common/UdpPacketCodec.h
Common/UdpPacketCodec.cpp
Signaling/Server/NatProbeServer.h/.cpp
Transport/UdpHolePuncher.h/.cpp
```

然后把视频、输入都先放一边，先把 `Punch/PunchAck` 打通。这个阶段跑通后，你对 UDP 的感觉会一下清楚很多。