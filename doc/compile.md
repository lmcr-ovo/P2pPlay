sudo apt update
sudo apt install -y build-essential cmake ninja-build qtbase5-dev qtbase5-dev-tools


sudo apt install -y libqt5core5a libqt5network5


cd '/mnt/d/CODE/C++/qt/P2Pplay'

cmake -S . -B cmake-build-wsl -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-wsl --target testP2pServer





主要改 **启动测试程序时的命令行参数**，不用改代码。

你现在服务端测试程序是：

```cpp
testP2pServer [tcpAddress] [tcpPort] [udpAddress] [udpPort]
```

默认是：

```text
tcp: 0.0.0.0 9000
udp: 0.0.0.0 9001
```

所以公网服务器上直接这样启动就行：

```powershell
./testP2pServer 0.0.0.0 9000 0.0.0.0 9001
```

`0.0.0.0` 的意思是监听服务器所有网卡，不是客户端连接用的地址。

客户端连接公网服务器时，改的是客户端参数：

Host：

```powershell
testP2pClient.exe host Alice 你的公网IP 9000 9001 10000
```

Guest：

```powershell
testP2pClient.exe guest Bob 房间号 你的公网IP 9000 9001 10001
```

例如你的公网 IP 是 `1.2.3.4`：

```powershell
testP2pClient.exe host Alice 1.2.3.4 9000 9001 10000
testP2pClient.exe guest Bob 898285 1.2.3.4 9000 9001 10001
```

还要记得在云服务器安全组/防火墙放行：

```text
TCP 9000
UDP 9001
```

服务端监听地址一般仍然用 `0.0.0.0`，客户端才填写服务器公网 IP。testP2pClient.exe host Alice 119.45.223.242 9000 9001 10000