# 远程输入系统需要用到的 Windows API

这篇文档参考之前 `RemoteControl` TCP 版本里已经跑通的按键逻辑，说明远控输入需要用哪些 API、这些 API 分别解决什么问题，以及迁移到当前 `P2Pplay` 项目时应该放在哪一层。

之前 TCP 版的核心代码在：

```text
D:\CODE\C++\qt\RemoteControl\Input\ButtonSender.*
D:\CODE\C++\qt\RemoteControl\Input\ButtonReceiver.*
D:\CODE\C++\qt\RemoteControl\Input\InputPacket.*
```

旧版本已经验证通过的链路是：

```text
guest 全局捕获键盘
  -> 按键映射
  -> 序列化 InputEvent
  -> TCP 发送 Button 包
  -> host 反序列化 InputEvent
  -> SendInput 注入系统按键
```

迁移到现在的 `P2Pplay` 时，不需要照搬 TCP 的 `Mailbox/Packet`，只需要保留输入业务层：

```text
InputEventCodec
InputCapture / InputSender
InputReceiver / InputInjector
```

传输层改为走现有的：

```cpp
UdpFrameType::InputEvent
MediaServiceWorker::sendInputSampleBytes(...)
MediaServiceWorker::inputSampleBytesReceived(...)
```

---

## 1. 总体分层

建议不要把 Windows API 直接塞进 `P2pSession` 或 `MediaServiceWorker`。

当前项目已经有比较清楚的结构：

```text
P2pSessionWorker
  只负责 UDP/P2P 收发

MediaServiceWorker
  只负责媒体业务帧路由
  VideoFrame / AudioFrame / InputEvent / KeyFrameRequest

Input 模块
  负责输入捕获、映射、编码、解码、注入
```

建议新增一个独立的 `Input` 层，类似老项目的 `Input` 目录：

```text
Input/
  InputEvent.h
  InputEventCodec.h/.cpp
  InputCaptureWorker.h/.cpp
  InputReceiverWorker.h/.cpp
  InputInjector.h/.cpp
  KeyMapper.h/.cpp
```

基础链路：

```text
Guest:
InputCaptureWorker
  -> KeyMapper
  -> InputEventCodec::encode()
  -> MediaServiceWorker::sendInputSampleBytes()
  -> P2pSessionWorker::sendMediaFrame(InputEvent)

Host:
P2pSessionWorker::mediaFrameReceived()
  -> MediaServiceWorker::onUdpMediaFrameReceived()
  -> inputSampleBytesReceived(bytes)
  -> InputEventCodec::decode()
  -> InputReceiverWorker
  -> InputInjector::execute()
  -> SendInput()
```

---

## 2. 输入包结构

老 TCP 版本的 `InputPacket.h` 已经预留了键盘和鼠标：

```cpp
enum class InputDevice : quint8 {
    Keyboard = 1,
    Mouse = 2
};

enum class InputEventType : quint8 {
    KeyDown = 1,
    KeyUp = 2,
    MouseMove = 3,
    MouseDown = 4,
    MouseUp = 5,
    MouseWheel = 6
};
```

旧版字段：

```cpp
struct InputEvent {
    InputDevice device = InputDevice::Keyboard;
    InputEventType type = InputEventType::KeyDown;

    quint32 key = 0;
    quint32 nativeVirtualKey = 0;
    quint32 nativeScanCode = 0;
    quint32 modifiers = 0;

    qint32 x = 0;
    qint32 y = 0;
    qint32 wheelDelta = 0;
    quint8 mouseButton = 0;
};
```

现在 P2P 版本建议在这个基础上增加序号和时间戳：

```cpp
struct InputEvent {
    quint32 seq = 0;
    quint64 timestampUs = 0;

    InputDevice device = InputDevice::Keyboard;
    InputEventType type = InputEventType::KeyDown;

    quint32 key = 0;
    quint32 nativeVirtualKey = 0;
    quint32 nativeScanCode = 0;
    quint32 modifiers = 0;

    qint32 x = 0;
    qint32 y = 0;
    qint32 wheelDelta = 0;
    quint8 mouseButton = 0;
};
```

字段含义：

```text
seq
  输入事件序号，用于 ACK、去重、重传和顺序执行。

timestampUs
  guest 产生输入事件的时间，可用于日志分析输入延迟。

device
  Keyboard 或 Mouse。

type
  KeyDown / KeyUp / MouseMove / MouseDown / MouseUp / MouseWheel。

key
  业务层键值，可保存 Qt key 或映射后的逻辑键。

nativeVirtualKey
  Windows 虚拟键，例如 'A'、VK_LEFT、VK_CONTROL。

nativeScanCode
  Windows 扫描码，host 注入时优先使用它。

modifiers
  修饰键状态，例如 Ctrl、Shift、Alt。

x / y
  鼠标坐标。建议用 host 屏幕坐标，而不是 guest 窗口坐标。

wheelDelta
  滚轮增量。Windows 一格通常是 WHEEL_DELTA，也就是 120。

mouseButton
  鼠标按钮，例如 Left / Right / Middle。
```

---

## 3. guest 捕获键盘：SetWindowsHookExW

旧 TCP 版本使用的是 Windows 全局低级键盘钩子：

```cpp
SetWindowsHookExW(
    WH_KEYBOARD_LL,
    keyboardHookProc,
    GetModuleHandleW(nullptr),
    0
);
```

对应头文件：

```cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
```

这个 API 的作用是：

```text
在当前 Windows 桌面会话中捕获键盘事件。
```

参数含义：

```text
WH_KEYBOARD_LL
  低级键盘钩子。可以捕获全局键盘事件。

keyboardHookProc
  钩子回调函数。

GetModuleHandleW(nullptr)
  当前模块句柄。

0
  线程 id 为 0，表示全局低级钩子，而不是只监听某个线程。
```

回调函数形式：

```cpp
LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
```

在回调里通过 `KBDLLHOOKSTRUCT` 获取按键信息：

```cpp
const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

quint32 vkCode = info->vkCode;
quint32 scanCode = info->scanCode;
DWORD flags = info->flags;
```

按下和抬起通过 `wParam` 判断：

```cpp
const bool pressed =
        wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;

const bool released =
        wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
```

这里的 `WM_SYSKEYDOWN / WM_SYSKEYUP` 很重要。它们通常和 Alt、系统组合键有关。如果不处理，Alt 相关组合键容易漏。

---

## 4. 过滤注入事件：LLKHF_INJECTED

旧 TCP 版本里有这段逻辑：

```cpp
if ((info->flags & LLKHF_INJECTED) != 0) {
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
```

这个非常重要。

`LLKHF_INJECTED` 表示这个按键事件不是物理键盘产生的，而是程序注入出来的。

远控场景里如果不排除注入事件，可能出现回环：

```text
host SendInput 注入一个键
  -> 本机 hook 又捕获到这个键
  -> 又发送出去
  -> 对端又注入
  -> 输入风暴
```

所以只要用全局 hook，就要考虑过滤 injected 事件。

---

## 5. 拦截本机按键：return 1

旧 TCP 版本里，如果某个按键已经被远控系统处理，就返回：

```cpp
return 1;
```

含义是：

```text
这个按键事件被当前 hook 吃掉，不继续传给本机其他窗口。
```

如果不想拦截，就继续传递：

```cpp
return CallNextHookEx(nullptr, code, wParam, lParam);
```

远控里这点要做成配置。

有两种模式：

```text
独占控制模式
  guest 窗口获得控制权后，按键只发给 host，不作用于 guest 本机。
  适合玩游戏。

非独占控制模式
  guest 本机也能收到按键。
  适合调试，但容易误操作。
```

基础版建议：

```text
鼠标进入远控窗口并点击捕获后，进入独占控制模式；
按 Esc 或指定快捷键退出独占控制模式。
```

---

## 6. 释放钩子：UnhookWindowsHookEx

启动 hook 后，退出时必须释放：

```cpp
UnhookWindowsHookEx(static_cast<HHOOK>(keyboardHook));
keyboardHook = nullptr;
```

旧 TCP 版本的 `ButtonSender::stop()` 里已经这么做了。

如果不释放，可能导致程序退出异常，或者调试时留下很奇怪的输入状态。

---

## 7. 键位映射：MapVirtualKeyW

旧 TCP 版本里，映射按键时用了：

```cpp
MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC)
```

作用是：

```text
把 Windows 虚拟键 VK 转为扫描码 scan code。
```

旧版示例：

```cpp
setKeyMapping('A', VK_LEFT);
setKeyMapping('D', VK_RIGHT);
setKeyMapping('W', VK_UP);
setKeyMapping('S', VK_DOWN);
```

`setKeyMapping('A', VK_LEFT)` 的意思是：

```text
guest 按 A
  -> 映射成 host 的方向键 Left
  -> 发送 VK_LEFT 和它对应的 scan code
```

为什么要发送扫描码？

因为 host 端注入时用的是：

```cpp
KEYEVENTF_SCANCODE
```

扫描码比单纯的虚拟键更接近物理键盘事件，适合远控和游戏场景。

---

## 8. activeMappings：保证 KeyUp 和 KeyDown 对应

旧 TCP 版本有两个集合：

```cpp
QHash<quint32, MappedKey> keyMappings;
QHash<quint32, MappedKey> activeMappings;
QSet<quint32> pressedKeys;
```

其中 `activeMappings` 很关键。

原因是：KeyUp 必须释放 KeyDown 时注入的那个键。

例如：

```text
guest A down
  -> 映射为 host Left down

如果映射配置中途改变：
guest A up
  -> 仍然必须发送 host Left up
```

不能因为映射变化就释放成别的键。否则 host 端可能卡键。

所以逻辑应该是：

```cpp
if (pressed) {
    mapped = keyMappings.value(virtualKey, originalKey);
    activeMappings.insert(virtualKey, mapped);
    return mapped;
}

mapped = activeMappings.take(virtualKey);
if (mapped.valid()) {
    return mapped;
}

return originalKey;
```

---

## 9. pressedKeys：去掉长按重复 KeyDown

Windows 长按某个键时，会不断产生重复的 `WM_KEYDOWN`。

旧 TCP 版本通过：

```cpp
QSet<quint32> pressedKeys;
```

过滤重复按下：

```cpp
if (pressed) {
    if (pressedKeys.contains(virtualKey)) {
        return true;
    }
    pressedKeys.insert(virtualKey);
} else if (!pressedKeys.remove(virtualKey)) {
    return true;
}
```

作用是：

```text
同一个键按住期间，只发送一次 KeyDown；
真正抬起时，只发送一次 KeyUp。
```

远控基础版建议保留这个逻辑。否则网络层会收到大量重复 KeyDown，host 端也可能产生非预期输入。

如果以后要支持文本输入里的长按重复，可以另外加配置：

```text
游戏模式：过滤重复 KeyDown
文本模式：允许系统重复 KeyDown
```

但当前目标是丝滑远控和游戏输入，优先过滤重复。

---

## 10. host 注入键盘：SendInput

旧 TCP 版本 host 端用的是：

```cpp
SendInput(1, &input, sizeof(INPUT))
```

这是当前 Windows 推荐的输入注入 API。

基础代码：

```cpp
INPUT input;
ZeroMemory(&input, sizeof(INPUT));

input.type = INPUT_KEYBOARD;
input.ki.wVk = 0;
input.ki.wScan = static_cast<WORD>(event.nativeScanCode);
input.ki.dwFlags = KEYEVENTF_SCANCODE;

if (event.type == InputEventType::KeyUp) {
    input.ki.dwFlags |= KEYEVENTF_KEYUP;
}

if (isExtendedVirtualKey(event.nativeVirtualKey)) {
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
}

SendInput(1, &input, sizeof(INPUT));
```

这里没有使用：

```cpp
input.ki.wVk = event.nativeVirtualKey;
```

而是使用：

```cpp
input.ki.wVk = 0;
input.ki.wScan = event.nativeScanCode;
input.ki.dwFlags = KEYEVENTF_SCANCODE;
```

原因是：

```text
扫描码注入更像真实键盘；
对游戏和某些底层输入读取方式更友好；
减少键盘布局带来的虚拟键歧义。
```

---

## 11. 扩展键：KEYEVENTF_EXTENDEDKEY

旧 TCP 版本有一个函数：

```cpp
bool isExtendedVirtualKey(quint32 virtualKey) {
    switch (virtualKey) {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_INSERT:
        case VK_DELETE:
        case VK_RCONTROL:
        case VK_RMENU:
        case VK_DIVIDE:
        case VK_NUMLOCK:
            return true;
        default:
            return false;
    }
}
```

这些键在 Windows 扫描码注入里通常需要：

```cpp
KEYEVENTF_EXTENDEDKEY
```

如果漏掉，方向键、右 Ctrl、右 Alt、Insert/Delete/Home/End 等键可能表现不正确。

---

## 12. 组合键怎么处理

组合键不要做成一个特殊事件。

不要这样：

```text
Combo Ctrl+C
```

应该这样：

```text
KeyDown Ctrl
KeyDown C
KeyUp C
KeyUp Ctrl
```

Windows 和目标程序是根据当前按键状态理解组合键的。

所以协议层只需要保证：

```text
KeyDown / KeyUp 有序到达；
重复包不会重复执行；
丢包后能恢复。
```

这也是为什么后面需要 `seq`、ACK、重传和 host 端去重。

---

## 13. 鼠标注入需要的 API

旧 TCP 版本协议已经预留鼠标事件：

```cpp
MouseMove
MouseDown
MouseUp
MouseWheel
```

但 `ButtonReceiver` 只实现了键盘：

```cpp
if (event.device == InputDevice::Keyboard) {
    return executeKeyboardEvent(event);
}
```

鼠标迁移时需要补完整。

### 13.1 移动鼠标：SetCursorPos

最简单的绝对坐标移动：

```cpp
SetCursorPos(x, y);
```

适合基础版远控。

这里的 `x/y` 是 host 屏幕坐标，不是 guest 窗口坐标。

guest 发送前要把窗口坐标转换成 host 屏幕坐标：

```text
remoteX = (localX - videoRect.left) * remoteWidth  / videoRect.width
remoteY = (localY - videoRect.top)  * remoteHeight / videoRect.height
```

### 13.2 鼠标点击：SendInput

左键按下：

```cpp
INPUT input;
ZeroMemory(&input, sizeof(INPUT));
input.type = INPUT_MOUSE;
input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
SendInput(1, &input, sizeof(INPUT));
```

左键抬起：

```cpp
INPUT input;
ZeroMemory(&input, sizeof(INPUT));
input.type = INPUT_MOUSE;
input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
SendInput(1, &input, sizeof(INPUT));
```

右键：

```cpp
MOUSEEVENTF_RIGHTDOWN
MOUSEEVENTF_RIGHTUP
```

中键：

```cpp
MOUSEEVENTF_MIDDLEDOWN
MOUSEEVENTF_MIDDLEUP
```

### 13.3 滚轮：SendInput + MOUSEEVENTF_WHEEL

```cpp
INPUT input;
ZeroMemory(&input, sizeof(INPUT));
input.type = INPUT_MOUSE;
input.mi.dwFlags = MOUSEEVENTF_WHEEL;
input.mi.mouseData = wheelDelta;
SendInput(1, &input, sizeof(INPUT));
```

Windows 一格滚轮通常是：

```cpp
WHEEL_DELTA // 120
```

Qt 的 `QWheelEvent::angleDelta().y()` 通常也以 120 为一格。

---

## 14. host 获取鼠标位置：GetCursorPos

如果视频采集不包含鼠标，那么 guest 需要自己绘制远端鼠标 overlay。

这时 host 应该周期性发送鼠标状态：

```cpp
POINT pt;
GetCursorPos(&pt);
```

然后封装为：

```cpp
struct CursorState {
    qint32 x = 0;
    qint32 y = 0;
    bool visible = true;
    quint64 timestampUs = 0;
};
```

发送方向：

```text
host -> guest
```

当前 `UdpFrameType` 里还没有 `CursorState`，以后可以新增：

```cpp
CursorState = 9
```

也可以第一版先复用 `InputEvent`，但从分层上我更建议单独帧类型：

```text
InputEvent:
  guest -> host
  输入命令

CursorState:
  host -> guest
  远端鼠标显示状态
```

---

## 15. guest 显示鼠标

如果 host 采集画面不带鼠标，guest 的 `VideoWidget` 应该绘制两层：

```text
远端视频帧
远端鼠标图标
```

guest 自己的本地鼠标应该在远控窗口内隐藏：

```cpp
setCursor(Qt::BlankCursor);
```

退出远控窗口或释放控制时恢复：

```cpp
unsetCursor();
```

显示逻辑：

```text
guest 本地鼠标移动
  -> 发送 MouseMove 给 host
  -> host SetCursorPos
  -> host GetCursorPos 回传真实位置
  -> guest VideoWidget 绘制远端鼠标
```

基础版可以直接以 host 回传位置为准。

后续为了降低体感延迟，可以加本地预测：

```text
guest 鼠标移动时立即更新 overlay
host CursorState 回来后再校准
```

---

## 16. 当前 P2Pplay 应该如何接入

当前 `UdpPacket.h` 已经有：

```cpp
enum class UdpFrameType : quint16 {
    VideoFrame = 5,
    AudioFrame = 6,
    InputEvent = 7,
    KeyFrameRequest = 8
};
```

当前 `MediaServiceWorker` 已经有发送入口：

```cpp
bool sendInputSampleBytes(const QByteArray& commandBytes);
```

内部会走：

```cpp
emit udpMediaFrameToSend(UdpFrameType::InputEvent, commandBytes);
```

接收入口：

```cpp
void inputSampleBytesReceived(const QByteArray& commandBytes);
```

所以输入模块不用直接碰 UDP。

建议连接关系：

```text
Guest:
InputCaptureWorker::inputEventBytesReady
  -> MediaServiceWorker::sendInputSampleBytes

Host:
MediaServiceWorker::inputSampleBytesReceived
  -> InputReceiverWorker::onInputCommandReceived
```

这样 `Input` 模块只依赖 `MediaServiceWorker` 暴露出来的信号槽，不需要理解 UDP 分片、重组和打洞。

---

## 17. ACK 和重传应该怎么加

键盘和鼠标点击是状态事件，不能随便丢。

建议：

```text
KeyDown / KeyUp
MouseDown / MouseUp
MouseWheel
  可靠、有序、ACK、必要时重传

MouseMove
  不可靠、不重传、只保留最新
```

原因：

```text
KeyUp 丢了会卡键；
MouseUp 丢了会卡鼠标；
MouseMove 丢了没关系，因为旧位置已经过时。
```

host 端执行逻辑：

```text
expectedSeq = 1

收到 seq < expectedSeq:
  重复包，不执行，只回 ACK

收到 seq == expectedSeq:
  执行事件
  expectedSeq++
  回 ACK

收到 seq > expectedSeq:
  中间缺包，不执行，回当前 ACK
```

guest 端发送逻辑：

```text
可靠事件进入 pending 队列
收到 ACK 后删除 <= ackSeq 的事件
超时后从 ackSeq + 1 开始重传
```

鼠标移动：

```text
latestMouseMoveEvent = newest
定时或立即发送最新 MouseMove
不进可靠重传队列
```

---

## 18. 必须处理的边界问题

### 18.1 防卡键

host 端必须维护已按下集合：

```cpp
QSet<quint32> pressedKeys_;
QSet<quint8> pressedMouseButtons_;
```

断开、退出控制、程序关闭时释放全部：

```text
releaseAllPressedKeys()
releaseAllPressedMouseButtons()
```

这是远控输入里最要命的边界。没处理的话，Ctrl、Alt、鼠标左键都可能卡住。

### 18.2 管理员权限

`SendInput` 受 Windows UIPI 权限限制。

普通权限程序通常不能控制管理员权限窗口。

如果要控制管理员窗口，host 端程序需要以管理员权限运行。

### 18.3 Ctrl+Alt+Del

普通桌面程序不能直接通过 `SendInput` 模拟 `Ctrl+Alt+Del`。

第一版不要支持这个。

### 18.4 Win 键和系统快捷键

`Win`、`Alt+Tab`、`Ctrl+Esc` 这类系统快捷键行为比较特殊。

第一版可以先禁用或做白名单。

### 18.5 hook 线程必须有消息循环

`SetWindowsHookExW(WH_KEYBOARD_LL, ...)` 所在线程不能死掉，也不能没有消息循环。

Qt 程序里最简单的方式是：

```text
在有 Qt 事件循环的线程里安装 hook。
```

如果单独放 `InputCaptureWorker` 线程，确保：

```text
QThread::start()
worker moveToThread(thread)
线程事件循环运行
```

不要在 worker 里写死循环阻塞事件循环。

---

## 19. 推荐第一版实现范围

第一版目标不要贪。

建议先做：

```text
1. InputEvent / InputEventCodec
2. guest 全局键盘捕获
3. guest 键位映射
4. host 键盘 SendInput 注入
5. pressedKeys / activeMappings
6. 基础 ACK 和重传
```

然后再做：

```text
7. 鼠标移动/点击/滚轮注入
8. host CursorState 回传
9. guest VideoWidget 鼠标 overlay
```

最后优化：

```text
10. 鼠标移动 latest-only
11. 输入优先级高于视频
12. 本地鼠标预测
13. 游戏模式 / 文本模式
14. 配置化按键映射
```

---

## 20. API 总结

已经在旧 TCP 版本用过并跑通的 API：

```text
SetWindowsHookExW
  安装全局低级键盘钩子。

WH_KEYBOARD_LL
  低级键盘钩子类型。

KBDLLHOOKSTRUCT
  键盘 hook 回调里的按键信息。

LLKHF_INJECTED
  判断事件是否由程序注入，用于避免输入回环。

CallNextHookEx
  不拦截时继续传递键盘事件。

UnhookWindowsHookEx
  停止捕获时释放 hook。

MapVirtualKeyW
  虚拟键转扫描码。

SendInput
  host 注入键盘事件。

KEYEVENTF_SCANCODE
  使用扫描码注入。

KEYEVENTF_KEYUP
  表示按键抬起。

KEYEVENTF_EXTENDEDKEY
  表示扩展键。
```

鼠标基础功能要新增使用的 API：

```text
SetCursorPos
  host 移动鼠标到指定屏幕坐标。

GetCursorPos
  host 获取当前真实鼠标位置，用于回传给 guest 显示。

SendInput + INPUT_MOUSE
  注入鼠标点击、抬起、滚轮。

MOUSEEVENTF_LEFTDOWN / LEFTUP
MOUSEEVENTF_RIGHTDOWN / RIGHTUP
MOUSEEVENTF_MIDDLEDOWN / MIDDLEUP
MOUSEEVENTF_WHEEL
  鼠标事件标志。
```

后续如果需要更专业的鼠标捕获，可以研究：

```text
WH_MOUSE_LL
  全局低级鼠标 hook。

RegisterRawInputDevices
GetRawInputData
WM_INPUT
  Raw Input，高频游戏鼠标输入更适合，但第一版没必要上。
```

---

## 21. 最终建议

当前 `P2Pplay` 里最合适的做法是：

```text
不要让 P2pSession 认识输入细节；
不要让 MediaServiceWorker 调 Windows API；
新增 Input 模块；
Input 模块把事件编码成 QByteArray；
通过 UdpFrameType::InputEvent 发送；
host 收到后由 InputReceiverWorker 解码并调用 InputInjector；
InputInjector 统一封装 SendInput / SetCursorPos / GetCursorPos。
```

也就是：

```text
传输层管字节
媒体服务层管业务帧类型
输入层管键鼠语义
Windows API 只藏在 InputInjector / InputCapture 里
```

这个分层最稳，也方便后面继续加：

```text
配置化映射
鼠标 overlay
ACK 重传
游戏模式
输入日志
输入延迟统计
```







可以，相关官方文档主要看 Microsoft Learn 这些：

### 键盘捕获 Hook

- `SetWindowsHookEx`：安装 hook  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexa

- `LowLevelKeyboardProc`：`WH_KEYBOARD_LL` 回调说明  
  https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc

- `KBDLLHOOKSTRUCT`：键盘 hook 事件结构体  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-kbdllhookstruct

- `UnhookWindowsHookEx`：释放 hook  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwindowshookex

- Hooks 总览  
  https://learn.microsoft.com/en-us/windows/win32/winmsg/hooks

### 键盘注入

- `SendInput`：注入键盘/鼠标输入  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput

- `INPUT`：`SendInput` 使用的总结构体  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-input

- `KEYBDINPUT`：键盘注入结构体  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-keybdinput

- Virtual-Key Codes：`VK_LEFT`、`VK_CONTROL` 等虚拟键表  
  https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

- `MapVirtualKeyW`：虚拟键和扫描码转换  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapvirtualkeyw

### 鼠标相关

- `SetCursorPos`：设置鼠标位置  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcursorpos

- `GetCursorPos`：获取鼠标位置  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos

- `MOUSEINPUT`：鼠标注入结构体  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-mouseinput

- `LowLevelMouseProc`：低级鼠标 hook 回调  
  https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc

- `MSLLHOOKSTRUCT`：鼠标 hook 事件结构体  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-msllhookstruct

### Raw Input，后续游戏鼠标可研究

- Raw Input 总览  
  https://learn.microsoft.com/en-us/windows/win32/inputdev/raw-input

- `RegisterRawInputDevices`  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices

- `GetRawInputData`  
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputdata

- `WM_INPUT`  
  https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input

你现在第一版重点看这几个就够了：`SetWindowsHookExW`、`KBDLLHOOKSTRUCT`、`MapVirtualKeyW`、`SendInput`、`INPUT`、`KEYBDINPUT`、`SetCursorPos`、`GetCursorPos`。