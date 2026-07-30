void SignalingServer::onNewConnection()
{
    QTcpSocket* socket = tcpServer_.nextPendingConnection();

    auto* connection = new SignalingConnection(socket, this);
    socket->setParent(connection);

    connect(connection, &SignalingConnection::messageReceived,
            this,
            [this, connection](const SignalingMessage& message) {
                onMessageReceived(connection, message);
            });

    connect(connection, &SignalingConnection::disconnected,
            this,
            [this, connection]() {
                roomManager_.removeConnection(connection);
                connection->deleteLater();
            });
}


`RoomManager` 建议不要直接管理裸 `QTcpSocket*`，而是管理你已经封装好的：

```cpp
SignalingConnection*
```

因为上层只需要：

```text
给某个用户发送 SignalingMessage
知道连接断开
```

不应该再碰 `QTcpSocket` 细节。

---

**核心结构**

一个房间里有两个角色：

```text
Host
Client
```

每个人需要保存：

```text
clientId
SignalingConnection*
UDP endpoint
是否 endpoint ready
```

可以这样设计：

```cpp
struct UdpEndpoint {
    QHostAddress address;
    quint16 port = 0;

    bool isValid() const {
        return !address.isNull() && port != 0;
    }
};

struct RoomPeer {
    QString clientId;
    SignalingConnection* connection = nullptr;
    UdpEndpoint udpEndpoint;

    bool isValid() const {
        return !clientId.isEmpty() && connection != nullptr;
    }
};

struct Room {
    QString roomId;
    RoomPeer host;
    RoomPeer client;

    bool hasClient() const {
        return client.isValid();
    }

    bool endpointsReady() const {
        return host.udpEndpoint.isValid()
            && client.udpEndpoint.isValid();
    }
};
```

`RoomManager` 内部维护：

```cpp
QHash<QString, Room> roomsById_;
QHash<QString, QString> roomIdByClientId_;
QHash<SignalingConnection*, QString> clientIdByConnection_;
```

分别用于：

```text
roomsById_
  通过 roomId 找房间

roomIdByClientId_
  通过 clientId 找所在房间

clientIdByConnection_
  连接断开时，根据 connection 找 clientId
```

---

**RoomManager 类接口**

建议这样：

```cpp
class RoomManager : public QObject {
    Q_OBJECT

public:
    explicit RoomManager(QObject* parent = nullptr);

    QString createRoom(const QString& hostClientId,
                       SignalingConnection* hostConnection);

    bool joinRoom(const QString& roomId,
                  const QString& clientId,
                  SignalingConnection* clientConnection,
                  QString* reason);

    bool updateUdpEndpoint(const QString& clientId,
                           const QHostAddress& address,
                           quint16 port);

    Room* findRoom(const QString& roomId);
    Room* findRoomByClientId(const QString& clientId);

    void removeConnection(SignalingConnection* connection);

signals:
    void roomCreated(const QString& roomId);
    void peerJoined(const QString& roomId,
                    const QString& hostClientId,
                    const QString& clientClientId);

    void roomEndpointsReady(const QString& roomId,
                            const RoomPeer& host,
                            const RoomPeer& client);

    void roomRemoved(const QString& roomId);

private:
    QString generateRoomId() const;

private:
    QHash<QString, Room> roomsById_;
    QHash<QString, QString> roomIdByClientId_;
    QHash<SignalingConnection*, QString> clientIdByConnection_;
};
```

---

**创建房间**

```cpp
QString RoomManager::createRoom(
    const QString& hostClientId,
    SignalingConnection* hostConnection
) {
    if (hostClientId.isEmpty() || hostConnection == nullptr) {
        return QString();
    }

    const QString roomId = generateRoomId();

    Room room;
    room.roomId = roomId;
    room.host.clientId = hostClientId;
    room.host.connection = hostConnection;

    roomsById_.insert(roomId, room);
    roomIdByClientId_.insert(hostClientId, roomId);
    clientIdByConnection_.insert(hostConnection, hostClientId);

    emit roomCreated(roomId);
    return roomId;
}
```

---

**加入房间**

```cpp
bool RoomManager::joinRoom(
    const QString& roomId,
    const QString& clientId,
    SignalingConnection* clientConnection,
    QString* reason
) {
    auto it = roomsById_.find(roomId);
    if (it == roomsById_.end()) {
        if (reason) {
            *reason = "room not found";
        }
        return false;
    }

    Room& room = it.value();

    if (room.hasClient()) {
        if (reason) {
            *reason = "room is full";
        }
        return false;
    }

    room.client.clientId = clientId;
    room.client.connection = clientConnection;

    roomIdByClientId_.insert(clientId, roomId);
    clientIdByConnection_.insert(clientConnection, clientId);

    emit peerJoined(roomId, room.host.clientId, room.client.clientId);
    return true;
}
```

---

**更新 UDP endpoint**

```cpp
bool RoomManager::updateUdpEndpoint(
    const QString& clientId,
    const QHostAddress& address,
    quint16 port
) {
    auto roomIdIt = roomIdByClientId_.find(clientId);
    if (roomIdIt == roomIdByClientId_.end()) {
        return false;
    }

    auto roomIt = roomsById_.find(roomIdIt.value());
    if (roomIt == roomsById_.end()) {
        return false;
    }

    Room& room = roomIt.value();

    UdpEndpoint endpoint;
    endpoint.address = address;
    endpoint.port = port;

    if (room.host.clientId == clientId) {
        room.host.udpEndpoint = endpoint;
    } else if (room.client.clientId == clientId) {
        room.client.udpEndpoint = endpoint;
    } else {
        return false;
    }

    if (room.endpointsReady()) {
        emit roomEndpointsReady(room.roomId, room.host, room.client);
    }

    return true;
}
```

---

**连接断开清理**

这个很重要。客户端断开时，你要能清掉房间。

```cpp
void RoomManager::removeConnection(SignalingConnection* connection) {
    auto clientIt = clientIdByConnection_.find(connection);
    if (clientIt == clientIdByConnection_.end()) {
        return;
    }

    const QString clientId = clientIt.value();
    clientIdByConnection_.remove(connection);

    auto roomIdIt = roomIdByClientId_.find(clientId);
    if (roomIdIt == roomIdByClientId_.end()) {
        return;
    }

    const QString roomId = roomIdIt.value();

    auto roomIt = roomsById_.find(roomId);
    if (roomIt == roomsById_.end()) {
        roomIdByClientId_.remove(clientId);
        return;
    }

    const Room room = roomIt.value();

    if (!room.host.clientId.isEmpty()) {
        roomIdByClientId_.remove(room.host.clientId);
        clientIdByConnection_.remove(room.host.connection);
    }

    if (!room.client.clientId.isEmpty()) {
        roomIdByClientId_.remove(room.client.clientId);
        clientIdByConnection_.remove(room.client.connection);
    }

    roomsById_.remove(roomId);

    emit roomRemoved(roomId);
}
```

初期可以简单粗暴：**任意一方断开，整个房间销毁**。这最符合 P2P 远控场景。

---

**房间号怎么生成**

初期可以 6 位数字：

```cpp
QString RoomManager::generateRoomId() const {
    QString roomId;

    do {
        roomId = QString::number(QRandomGenerator::global()->bounded(100000, 1000000));
    } while (roomsById_.contains(roomId));

    return roomId;
}
```

需要 include：

```cpp
#include <QRandomGenerator>
```

---

**服务器怎么用它**

收到 `CreateRoom`：

```cpp
const QString roomId = roomManager.createRoom(
    message.clientId,
    connection
);

SignalingMessage response;
response.type = SignalingType::RoomCreated;
response.roomId = roomId;
response.success = !roomId.isEmpty();

connection->sendMessage(response);
```

收到 `JoinRoom`：

```cpp
QString reason;
const bool ok = roomManager.joinRoom(
    message.roomId,
    message.clientId,
    connection,
    &reason
);

SignalingMessage response;
response.type = SignalingType::JoinResult;
response.roomId = message.roomId;
response.success = ok;
response.reason = reason;

connection->sendMessage(response);
```

`peerJoined` 信号里通知 Host：

```cpp
SignalingMessage notify;
notify.type = SignalingType::PeerJoined;
notify.roomId = roomId;
notify.clientId = clientClientId;

host.connection->sendMessage(notify);
```

`roomEndpointsReady` 信号里交换 endpoint：

```cpp
// 发给 Host：Client 的 UDP endpoint
SignalingMessage toHost;
toHost.type = SignalingType::PeerEndPoint;
toHost.roomId = roomId;
toHost.endpointAddress = client.udpEndpoint.address;
toHost.endpointPort = client.udpEndpoint.port;
host.connection->sendMessage(toHost);

// 发给 Client：Host 的 UDP endpoint
SignalingMessage toClient;
toClient.type = SignalingType::PeerEndPoint;
toClient.roomId = roomId;
toClient.endpointAddress = host.udpEndpoint.address;
toClient.endpointPort = host.udpEndpoint.port;
client.connection->sendMessage(toClient);
```

---

一句话总结：

```text
RoomManager 管 roomId -> Room
Room 管 host/client
Peer 里保存 clientId + SignalingConnection* + udpEndpoint
断线就按 connection 反查 clientId，再清掉整个房间
```

不要让 `RoomManager` 直接操作 `QTcpSocket*`，用 `SignalingConnection*` 更符合你现在的分层。