对，你需要留一份“我到底动了什么”的记录。按目前工作区看，我主要修改/新增了这些：

**Client**
新增/完善了客户端整套打洞框架：

- `Client/SignalingClient.h/.cpp`
  - 客户端 TCP 连接服务器。
  - 复用 `SignalingConnection` 收发信令。

- `Client/ClientDispatcher.h/.cpp`
  - 按 `SignalingType` 分发服务器消息。

- `Client/NatTraversalService.h/.cpp`
  - UDP Probe。
  - 接收 `PeerEndpoint`。
  - Punch / PunchAck。
  - `p2pReady` 通知。
  - 后来修复为对服务器提供端口附近做 Punch，默认 `±8`。

- `Client/HostRoleService.h/.cpp`
  - Host 连接成功后 Register + CreateRoom。
  - 收到 `PeerJoined` 后发 `ProbeRequest`。
  - 收到 `p2pReady` 后输出日志。

- `Client/GuestRoleService.h/.cpp`
  - Guest 连接成功后 Register + JoinRoom。
  - 收到 `p2pReady` 后输出日志。

- `Client/ClientApp.h/.cpp`
  - 组装 `SignalingClient / Dispatcher / NatTraversalService / HostRoleService / GuestRoleService`。
  - 提供：
    ```cpp
    startAsHost(...)
    startAsGuest(...)
    ```

- `Client/CMakeLists.txt`
  - 添加 `Client` 静态库。
  - 添加 `Client/test` 子目录。

- `Client/test/testP2pClient.cpp`
  - 命令行测试客户端。
  - 支持：
    ```bash
    testP2pClient host Alice ...
    testP2pClient guest Bob roomId ...
    ```

**Server**
为了让测试服务端能直接启动，我改了：

- `Server/ServerApp.h/.cpp`
  - 增加：
    ```cpp
    bool start(tcpAddress, tcpPort, udpAddress, udpPort);
    ```
  - 连接 `NatProbeService::probeReceived` 到 `RoomService::onSingleUdpEndpointReady`。
  - 暴露 `errorOccurred`。

- `Server/SignalingServer.h/.cpp`
  - `start()` 从 `void` 改成 `bool`，方便知道监听是否成功。
  - 确保 `messageReceived` 传 `SignalingConnection*`，业务层可以回信。

- `Server/CMakeLists.txt`
  - 添加 `Server/test` 子目录。

- `Server/test/testP2pServer.cpp`
  - 命令行测试服务端：
    ```bash
    testP2pServer 0.0.0.0 9000 0.0.0.0 9001
    ```

**Common**
- `Common/SignalingConnection.cpp`
  - 修了 socket 生命周期问题：
    ```cpp
    if (sock_->parent() == nullptr) {
        sock_->setParent(this);
    }
    ```
  - 这样服务端动态 socket 仍由 `SignalingConnection` 管，客户端成员 socket 不会被重复析构。

**CMake**
- 根目录 `CMakeLists.txt`
  - 当前已经包含：
    ```cmake
    add_subdirectory(Client)
    ```

**额外注意**
当前 `git status` 里还有这些变化我不建议直接算成“我这次新增功能的一部分”：

- `Server/RoomStore.h/.cpp` 显示删除，这更像是你之前淘汰 `RoomStore` 的结构调整。
- `doc/client.md` 是未跟踪文件，我这轮没有依赖它完成测试。
- 还有一些行尾 `LF -> CRLF` 警告，不是功能变化。

一句话总结：我主要补了 **客户端 Host/Guest 打洞架构、测试客户端/服务端入口、ServerApp 启动接口、TCP socket 生命周期修复、端口附近 Punch 修复**。这些就是让公网打洞跑到 `p2p ready` 的关键修改。