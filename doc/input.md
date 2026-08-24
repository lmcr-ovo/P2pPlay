这段代码是你整个输入系统的**"最前端哨兵"**，也是系统编程里最绕的一个模式。我逐行拆成你能听懂的样子。

---

## 第 1 行：`namespace {`

```cpp
namespace {
    InputCaptureWorker* activeCapture = nullptr;
```

**没有名字的 namespace**，意思是：这里面的东西**只属于当前这个 .cpp 文件**，别的文件看不到。

为什么需要它？因为 `activeCapture` 是一个**全局指针**。如果没有 `namespace {}` 包着，别的 .cpp 文件也可能定义一个叫 `activeCapture` 的变量，链接时会冲突。

> 类比：就像你们宿舍门口放了个鞋架，只给你们宿舍用。`namespace {}` 就是"宿舍内部物品"，别的宿舍（别的 .cpp）看不到。

---

## 第 2 行：`InputCaptureWorker* activeCapture = nullptr;`

这是一个**全局静态指针**，指向当前正在工作的 `InputCaptureWorker` 实例。

**为什么要搞这个全局指针？**

因为 `keyboardHookProc` 是一个**普通 C 函数**（后面会讲），它不属于任何类，没有 `this` 指针。但它又需要调用 `InputCaptureWorker` 的成员函数（比如 `handleKeyboardEvent`）。

所以用 `activeCapture` 做**桥梁**：
- `InputCaptureWorker::start()` 时：`activeCapture = this;`（指向我）
- `keyboardHookProc` 被 Windows 调用时：通过 `activeCapture` 找到对象，再调成员函数

> 类比：你（`InputCaptureWorker`）去酒店前台（Windows）登记了手机号（`activeCapture`），前台有事就通过这个手机号找你。

---

## 第 3 行：`LRESULT CALLBACK keyboardHookProc(...)`

```cpp
LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
```

这是 Windows 的**回调函数**。`SetWindowsHookExW` 安装钩子后，**每按一次键盘**，Windows 都会自动调用这个函数。

**参数含义**：
| 参数 | 类型 | 含义 |
|------|------|------|
| `code` | `int` | 消息类型码。`HC_ACTION` 表示"这是一个正常的按键消息" |
| `wParam` | `WPARAM` | 按键动作类型：按下/松开 |
| `lParam` | `LPARAM` | 按键详细信息（哪个键、是否软件注入等）的内存地址 |

---

## 第 4 行：`if (code == HC_ACTION && activeCapture != nullptr)`

```cpp
if (code == HC_ACTION && activeCapture != nullptr) {
```

**两个条件**：
1. `code == HC_ACTION`：确认是正常按键消息（不是系统内部的什么特殊通知）
2. `activeCapture != nullptr`：确认你的对象还活着（已经 `start()` 了）

如果 `activeCapture` 是 `nullptr`（还没 start 或者已经 stop 了），直接跳过，不处理。

---

## 第 5 行：`reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam)`

```cpp
const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
```

`lParam` 本质上是一个**内存地址**（指针），指向 Windows 准备好的一个结构体。但这个地址被 Windows 包装成了 `LPARAM` 类型（整数），你需要**强制转换**回真实的结构体指针。

`KBDLLHOOKSTRUCT` 是 Windows 定义的结构体，里面包含：
- `vkCode`：虚拟键码（比如 'A' 是 65）
- `flags`：标志位（比如是否软件注入）
- `time`：时间戳

> `reinterpret_cast` 是最"粗暴"的类型转换：告诉编译器"我确定这块内存就是这个结构体，你别管，按我说的解释"。

---

## 第 6 行：`LLKHF_INJECTED` 防死循环

```cpp
if ((info->flags & LLKHF_INJECTED) != 0) {
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
```

**这是最关键的一行，没有它程序会死循环。**

`LLKHF_INJECTED` 是 Windows 的一个标志位，表示这个按键是**软件模拟的**（不是人真正按的）。

**什么时候会出现软件模拟的按键？**
- Host 端执行了你的 `InputInjector::sendKeyDown()`，通过 `SendInput` 注入按键
- 如果 Guest 端（你的程序）又监听到这个注入的按键，再次发送给 Host
- Host 再注入，Guest 再监听……**无限循环**

所以：检测到 `LLKHF_INJECTED`，直接 `return CallNextHookEx(...)`，**放行，不处理**。

> 类比：你帮朋友代按电梯，电梯来了你进去，但你不会把"电梯到了"这个消息再转发给朋友让他再按一次。

---

## 第 7 行：`WM_KEYDOWN` / `WM_SYSKEYDOWN`

```cpp
const bool pressed = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
const bool released = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
```

`wParam` 告诉你这是什么动作：

| 值 | 含义 |
|----|------|
| `WM_KEYDOWN` | 普通键按下 |
| `WM_SYSKEYDOWN` | 系统键按下（Alt、F10 等，或者 Alt+Tab 这种组合） |
| `WM_KEYUP` | 普通键松开 |
| `WM_SYSKEYUP` | 系统键松开 |

**为什么要区分 `KEY` 和 `SYSKEY`？**

因为 `Alt` 键（以及 `F10`）在 Windows 里属于"系统键"，会触发系统菜单。如果你只监听 `WM_KEYDOWN`，按 `Alt` 时可能收不到消息。两个都监听才能覆盖所有按键。

---

## 第 8 行：`activeCapture->handleKeyboardEvent(...)`

```cpp
if (activeCapture->handleKeyboardEvent(info->vkCode, pressed)) {
    return 1;
}
```

- `info->vkCode`：按键的虚拟键码（Virtual-Key Code），比如 `A` 键是 65，`VK_LEFT` 是 37
- `pressed`：`true` 表示按下，`false` 表示松开
- `handleKeyboardEvent` 返回 `bool`：
  - `true`：这个按键我要了，**吃掉**（不让系统/游戏处理）
  - `false`：这个按键我不要，**放行**

如果返回 `true`，执行 `return 1;`，直接结束回调。**后面的程序（包括游戏）收不到这个按键**。

---

## 第 9 行：`return CallNextHookEx(...)`

```cpp
return CallNextHookEx(nullptr, code, wParam, lParam);
```

**两种情况会走到这里**：
1. `code != HC_ACTION`：不是正常按键消息，放行
2. `handleKeyboardEvent` 返回 `false`：你不要这个按键，放行

`CallNextHookEx` 的意思是："**我把这个消息传给下一个钩子**"。如果后面没有别的钩子了，就传给 Windows 默认处理，最终到达游戏/QQ/微信。

> 类比：快递到了，你（钩子）先看一眼。如果是你的包裹，你签收（`return 1`）。如果不是你的，你递给下一个人（`CallNextHookEx`），直到送到真正的收件人手里。

---

## 完整流程图

```
用户按了键盘 'A'
    │
    ▼
Windows 调用 keyboardHookProc
    │
    ├── code == HC_ACTION ? ──Yes──► 继续
    │
    ├── activeCapture != nullptr ? ──Yes──► 继续
    │
    ├── info->flags & LLKHF_INJECTED ? ──No──► 继续
    │
    ├── wParam == WM_KEYDOWN ? ──Yes──► pressed = true
    │
    ├── activeCapture->handleKeyboardEvent('A', true)
    │       │
    │       ├── controlActive_ == true ? ──Yes
    │       ├── 不是重复按下 ? ──Yes
    │       ├── 映射 'A' → VK_LEFT
    │       ├── 构造 InputSample
    │       ├── emit inputRawSampleReady(sample)  ──► 发给网络层
    │       └── return blockLocalInput_ (true)
    │
    └── return 1  ◄── 吃掉！游戏收不到 'A'
```

---

## 一句话总结

> `keyboardHookProc` 是 Windows 和你的程序之间的**门卫**。它用全局指针 `activeCapture` 找到你的对象，过滤掉软件注入的按键，区分按下/松开，决定是吃掉（`return 1`）还是放行（`CallNextHookEx`）。

还有哪一行需要再拆？比如 `SetWindowsHookExW` 的参数含义，或者 `KBDLLHOOKSTRUCT` 里还有哪些字段？