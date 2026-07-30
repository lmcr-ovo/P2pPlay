在 CMake 里配置自己写的库，一般就是三步：

```text
1. add_library() 创建库
2. target_include_directories() 暴露头文件目录
3. target_link_libraries() 链接依赖库
```

以你现在的 `Common` 为例：

```cmake
add_library(Common
        TcpFrame.h
        TcpFrameCodec.h TcpFrameCodec.cpp
        SignalingMessage.h
        SignalingCodec.h SignalingCodec.cpp
        SignalingConnection.h SignalingConnection.cpp)

target_include_directories(Common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Common PUBLIC
        Qt5::Core
        Qt5::Network)
```

含义是：

```text
add_library(Common ...)
  创建一个叫 Common 的库

target_include_directories(Common PUBLIC ...)
  让 Common 自己和使用 Common 的目标都能 include Common 目录里的头文件

target_link_libraries(Common PUBLIC Qt5::Core Qt5::Network)
  Common 依赖 QtCore/QtNetwork
  使用 Common 的目标也会继承这些依赖
```

然后其他模块使用它，比如 `Server`：

```cmake
add_executable(ServerApp
        main.cpp
        SignalingServer.cpp
        SignalingServer.h)

target_link_libraries(ServerApp PRIVATE
        Common
        Qt5::Core
        Qt5::Network)
```

因为 `ServerApp` 链接了 `Common`，所以代码里可以：

```cpp
#include "SignalingConnection.h"
#include "SignalingMessage.h"
```

---

如果你要新建一个自己的库，比如 `ServerCore`：

目录：

```text
Server/
  CMakeLists.txt
  SignalingServer.h
  SignalingServer.cpp
  RoomRegistry.h
  RoomRegistry.cpp
```

`Server/CMakeLists.txt`：

```cmake
add_library(ServerCore
        SignalingServer.h SignalingServer.cpp
        RoomRegistry.h RoomRegistry.cpp)

target_include_directories(ServerCore PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ServerCore PUBLIC
        Qt5::Core
        Qt5::Network
        Common)
```

根目录 `CMakeLists.txt` 加：

```cmake
add_subdirectory(Common)
add_subdirectory(Udp)
add_subdirectory(Server)
```

---

`PUBLIC / PRIVATE / INTERFACE` 简单理解：

```text
PRIVATE
  只有当前库自己需要

PUBLIC
  当前库自己需要，使用它的人也需要

INTERFACE
  当前库自己不编译用，但使用它的人需要
```

你项目里大多数 Qt 依赖建议用 `PUBLIC`，因为头文件里暴露了 Qt 类型：

```cpp
QTcpSocket
QUdpSocket
QHostAddress
QObject
QByteArray
```

如果某个 `.cpp` 才用，头文件没暴露，就可以 `PRIVATE`。

最常用模板：

```cmake
add_library(MyLib
        A.h A.cpp
        B.h B.cpp)

target_include_directories(MyLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(MyLib PUBLIC
        Qt5::Core
        Qt5::Network
        Common)
```