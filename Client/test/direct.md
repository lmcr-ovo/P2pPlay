我检查了一遍，你这个“伪视频数据传输”的主链路思路是对的：

```text
Host:
P2pSession::p2pReady
  -> VideoCapturer::onP2pReady
  -> timer 每 500ms emit videoFrameReady("hello")
  -> MediaService::sendVideoFrame
  -> P2pSession::sendMediaFrame
  -> UDP Media/VideoFrame

Guest:
P2pSession 收到 Media/VideoFrame
  -> MediaService::onMediaFrameReceived
  -> videoFrameReceived
  -> VideoRender::onVideoFrameRecevied
  -> 打印 hello
```

我只改了测试文件 [testV.cpp](D:/CODE/C++/qt/P2Pplay/Client/test/testV.cpp)，没有改正式代码。`testV` 已经编译通过：

```text
[100%] Built target testV
```

**需要注意的问题**
1. `Media` 和 `Client` 现在互相依赖：`Client` 链接 `Media`，`Media` 又链接 `Client`。架构上不太好。建议后面让 `Media` 不依赖 `Client`，`VideoCapturer.h` 里也不要 include `P2pSession.h`，改成只 include `QHostAddress`。

2. `P2pSession::sendMediaFrame()` 当前没有检查 `p2pReady_`，建议后面改成未连接时拒绝发送。

3. `VideoRender::onVideoFrameRecevied` 拼写是 `Recevied`，建议后面统一改成 `Received`。

**测试方法**
先启动服务器：

```bat
testP2pServer.exe 0.0.0.0 9000 0.0.0.0 9001
```

Host 端：

```bat
testV.exe host Alice 119.45.223.242 9000 9001 10000
```

看到房间号，比如：

```text
room created: 123456
```

Guest 端：

```bat
testV.exe guest Bob 123456 119.45.223.242 9000 9001 10001
```

成功标准：Guest 端持续打印：

```text
"hello"
"hello"
"hello"
```

这就说明伪视频帧已经通过 P2P UDP 媒体通道从 Host 发到 Guest。程序默认 60 秒后退出。