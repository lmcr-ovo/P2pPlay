可以把 TCP 信令底层封装成三层：

```text
TcpFrameCodec     负责解决 TCP 粘包/半包
SignalingCodec    负责信令 JSON encode/decode
SignalingClient / SignalingServerConnection
                  负责 socket 收发和信号
```

你不要让 `QTcpSocket` 到处散落在业务代码里。底层应该只暴露“发送信令消息”和“收到信令消息”。

**1. TCP 外层包格式**

TCP 是字节流，所以必须有长度头。推荐：

```text
magic       quint32
version     quint16
type        quint16
length      quint32
payload     length bytes
```

其中：

```text
type    信令类型，比如 CreateRoom / JoinRoomResult / PeerEndpoint
length  payload 长度
payload JSON 内容
```

**2. TcpFrame**

```cpp
enum class SignalingType : quint16 {
    Unknown = 0,
    CreateRoom = 1,
    RoomCreated = 2,
    JoinRoomResult = 3,
    JoinRoomResult = 4,
    PeerJoined = 5,
    UdpEndpointReady = 6,
    PeerEndpoint = 7,
    P2pResultReport = 8,
    Error = 9
};

struct TcpFrame {
    SignalingType type = SignalingType::Unknown;
    QByteArray payload;
};
```

**3. TcpFrameCodec**

它只负责 TCP 包头，不管 JSON 内容。

```cpp
class TcpFrameCodec {
public:
    static QByteArray encode(const TcpFrame &frame);
    static bool tryDecode(QByteArray &buffer, TcpFrame *frame);

private:
    static const quint32 Magic = 0x50325043; // P2PC
    static const quint16 Version = 1;
    static const int HeaderSize = 12;
    static const quint32 MaxPayloadSize = 1024 * 1024;
};
```

核心逻辑：

```cpp
QByteArray TcpFrameCodec::encode(const TcpFrame &frame) {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << Magic;
    stream << Version;
    stream << static_cast<quint16>(frame.type);
    stream << static_cast<quint32>(frame.payload.size());

    data.append(frame.payload);
    return data;
}

bool TcpFrameCodec::tryDecode(QByteArray &buffer, TcpFrame *frame) {
    if (frame == nullptr || buffer.size() < HeaderSize) {
        return false;
    }

    QDataStream stream(buffer.left(HeaderSize));
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    quint16 version = 0;
    quint16 type = 0;
    quint32 length = 0;

    stream >> magic >> version >> type >> length;

    if (magic != Magic || version != Version || length > MaxPayloadSize) {
        buffer.clear();
        return false;
    }

    const int frameSize = HeaderSize + static_cast<int>(length);
    if (buffer.size() < frameSize) {
        return false;
    }

    frame->type = static_cast<SignalingType>(type);
    frame->payload = buffer.mid(HeaderSize, length);
    buffer.remove(0, frameSize);
    return true;
}
```

**4. SignalingCodec**

它只负责 JSON 和结构体转换。

```cpp
struct SignalingMessage {
    SignalingType type = SignalingType::Unknown;
    QString roomId;
    QString clientId;
    QString reason;
    QHostAddress endpointAddress;
    quint16 endpointPort = 0;
    bool success = false;
};
```

```cpp
class SignalingCodec {
public:
    static QByteArray encodePayload(const SignalingMessage &message);
    static bool decodePayload(SignalingType type,
                              const QByteArray &payload,
                              SignalingMessage *message);
};
```

**5. SignalingConnection**

这是 TCP 底层连接封装。客户端和服务器都可以复用它。

```cpp
class SignalingConnection : public QObject {
    Q_OBJECT

public:
    explicit SignalingConnection(QTcpSocket *socket, QObject *parent = nullptr);

    bool sendMessage(const SignalingMessage &message);
    QTcpSocket *tcpSocket() const;

signals:
    void messageReceived(const SignalingMessage &message);
    void disconnected();
    void errorOccurred(const QString &reason);

private slots:
    void onReadyRead();

private:
    QTcpSocket *socket = nullptr;
    QByteArray buffer;
};
```

实现流程：

```cpp
void SignalingConnection::onReadyRead() {
    buffer.append(socket->readAll());

    while (true) {
        TcpFrame frame;
        if (!TcpFrameCodec::tryDecode(buffer, &frame)) {
            break;
        }

        SignalingMessage message;
        if (!SignalingCodec::decodePayload(frame.type, frame.payload, &message)) {
            emit errorOccurred("invalid signaling message");
            continue;
        }

        emit messageReceived(message);
    }
}

bool SignalingConnection::sendMessage(const SignalingMessage &message) {
    if (socket == nullptr || socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    TcpFrame frame;
    frame.type = message.type;
    frame.payload = SignalingCodec::encodePayload(message);

    const QByteArray bytes = TcpFrameCodec::encode(frame);
    return socket->write(bytes) == bytes.size();
}
```

**6. 上层怎么用**

客户端：

```cpp
connect(connection, &SignalingConnection::messageReceived,
        dispatcher, &ClientSignalingDispatcher::dispatch);

connection->sendMessage(createRoomMessage);
```

服务器：

```cpp
connect(connection, &SignalingConnection::messageReceived,
        dispatcher, &ServerSignalingDispatcher::dispatch);
```

这样底层封装之后，上层完全不用关心：

```text
TCP 粘包
半包
字节缓存
长度字段
JSON 解析
socket readyRead
```

上层只看到：

```text
sendMessage(message)
messageReceived(message)
```

一句话：

```text
TcpFrameCodec 解决 TCP 字节流问题；
SignalingCodec 解决信令格式问题；
SignalingConnection 解决 socket 收发问题。
```