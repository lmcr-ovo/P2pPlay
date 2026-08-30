# 你现在的现状

业务架构、数据流、线程分层你已经完全看懂了，但是底层库API不熟：

1. **Opus C库API**（C语言接口）
2. **Qt5 Multimedia 的 QAudioInput / QAudioOutput**
3. **Windows WASAPI COM接口（`IMMDevice`、`IAudioClient`、`IAudioCaptureClient`）**

这三块都是C/C++原生C‑style API，没有面向对象封装，坑特别多，参数、返回值、内存释放、错误码很容易搞错。

> 
> 好在：文档已经把**封装层全部写好了**：`AudioOpusEncoder`、`AudioOpusDecoder`、`MicrophoneAudioSource`、`DesktopAudioSource`、`AudioPlayback`。
> **你业务代码只调用我们封装后的C++类，尽量少直接裸调底层API。**

下面把每一块底层API做极简扫盲，搞懂核心概念，不用死记，写代码的时候回来对照。

---

## 一、Opus API（纯C库）

头文件：`#include <opus/opus.h>`

> 
> 注意：是C库，类型是 `OpusEncoder*`、`OpusDecoder*`，指针，需要手动销毁，没有RAII。

### 编码器

```
// 创建编码器
OpusEncoder* opus_encoder_create(
    int fs,          // 采样率，只支持 8000/12000/16000/24000/48000，我们固定48000
    int channels,    // 声道 1 / 2
    int application, // OPUS_APPLICATION_VOIP 人声；OPUS_APPLICATION_AUDIO音乐
    int *error       // 输出错误码，OPUS_OK代表成功
);

// 编码 PCM16 → opus压缩二进制
int opus_encode(
    OpusEncoder *st,
    const opus_int16 *pcm, // 输入：PCM16数组
    int frame_size,       // **一帧有多少个采样点（单声道采样点数量，不是字节！！！重点坑）**
    unsigned char *data,  // 输出缓冲区
    int max_data_bytes    //输出缓冲区最大大小
);
// 返回值：>0 代表编码后字节长度；<=0 错误。

// 设置参数控制
opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate));

// 销毁，必须调用，内存释放
void opus_encoder_destroy(OpusEncoder *st);
```

> 
> ⚠️大坑：`frame_size` = `sampleRate * frameDurationMs / 1000`
> 48000，20ms → `48000 * 20 /1000 = 960`。
> **不管是单声道还是双声道，frame_size永远填960，不是字节数！不是乘以声道！**

### 解码器

```
OpusDecoder* opus_decoder_create(int fs, int channels, int *error);

// opus码流 → PCM16
int opus_decode(
    OpusDecoder *st,
    const unsigned char *data, int len,
    opus_int16 *pcm,
    int frame_size, //输出缓冲区最多能存多少采样点
    int decode_fec
);

void opus_decoder_destroy(OpusDecoder *st);
```

### 错误码

`opus_strerror(error)` 把数字错误码转字符串，调试非常好用。

> 
> ✨对你：**业务层不要直接调用上面C函数，全部使用 `AudioOpusEncoder / AudioOpusDecoder`。**
> 封装类内部已经处理：create、destroy、参数设置、帧大小校验、错误判断。
> 你只调用：`open()`、`encodePcm16()`、`close()`。

---

## 二、Qt5 Multimedia：`QAudioInput` 采集、`QAudioOutput`播放

> 
> Qt5.14的多媒体，不是跨平台完美，Windows底层封装MME。

### QAudioFormat 音频格式（最重要结构体）

```
QAudioFormat format;
format.setSampleRate(48000);
format.setChannelCount(1);
format.setSampleSize(16);                 // 16bit
format.setCodec("audio/pcm");
format.setByteOrder(QAudioFormat::LittleEndian); //小端 PCM16
format.setSampleType(QAudioFormat::SignedInt);  //有符号16位
```

### 麦克风采集 QAudioInput

1. `QAudioDeviceInfo::defaultInputDevice()` 获取默认麦克风设备
2. `QAudioInput audioInput(deviceInfo, format)`
3. `QIODevice* dev = audioInput.start();`> 
> 返回io设备，从这个io `readAll()` 读到PCM原始字节。
4. `connect(dev, &QIODevice::readyRead, this, &xxx::onReadyRead);` 有数据就触发。

> 
> 大坑：read拿到的字节是碎片，不是刚好20ms完整帧！需要自己在缓冲区缓存，攒够一帧字节再输出。也就是代码里面的`pendingPcm_`缓冲区。

### 播放 QAudioOutput

```
QAudioOutput audioOutput(deviceInfo, format);
QIODevice* dev = audioOutput.start();
dev->write(pcmBytes); //直接写pcm字节就播放
```

> 
> 注意：`start()`返回的`QIODevice`不要手动delete，QAudioOutput销毁会自动处理。

> 
> ✨对你：业务代码不要裸用，直接用封装好的 `MicrophoneAudioSource` / `AudioPlayback`。
> 对外接口：`start(samplerate,channels,frameMs)`；信号输出pcm帧。

---

## 三、Windows WASAPI COM API（DesktopAudioSource 系统回环采集）

这一块是最劝退的，纯Windows COM，C接口，一堆接口指针，**必须手动Release释放，漏一个就内存泄漏**。

关键对象（全部是接口指针）

- `IMMDeviceEnumerator`：枚举声卡设备
- `IMMDevice`：一个音频端点设备（扬声器）
- `IAudioClient`：音频客户端，初始化流
- `IAudioCaptureClient`：回环捕获，拿音频数据

### 核心流程伪代码

1. `CoInitializeEx(nullptr, COINIT_MULTITHREADED)` 初始化COM库
2. `CoCreateInstance(__uuidof(MMDeviceEnumerator), ...)` 创建设备枚举器
3. `GetDefaultAudioEndpoint(eRender, eConsole, &device_)` 获取默认扬声器（用于loopback）
4. `device_->Activate(__uuidof(IAudioClient),..., &audioClient_)` 拿到AudioClient
5. `IAudioClient::Initialize(..., AUDCLNT_STREAMFLAGS_LOOPBACK,...)`> 
> `AUDCLNT_STREAMFLAGS_LOOPBACK` 标志！开启系统回环捕获，抓取扬声器输出。
6. `audioClient_->GetService(__uuidof(IAudioCaptureClient), &captureClient_)` 获取捕获接口
7. `audioClient_->Start()` 开始采集
8. 定时器轮询：`IAudioCaptureClient::GetNextPacketSize()`、`GetBuffer()`拿pcm数据、`ReleaseBuffer()`释放包。
9. 用完所有接口必须调用 `->Release()`；`CoUninitialize()`

> 
> 重点坑：

1. COM接口指针，每一个`xxx->Create / Activate`得到的指针，用完都要`Release()`，否则内存泄漏。
2. 不要跨线程随便传递COM接口指针，COM有线程模型。
3. loopback只能抓**输出设备（扬声器）**，不能抓麦克风。

> 
> ✨对你：这一坨全部封装在`DesktopAudioSource`里面。业务层你只调用 `start() / stop()`，完全不要碰这些COM指针。

---

# 你的正确使用姿势（非常关键）

> 
> 底层Opus / QtMultimedia / WASAPI 这些API你**不需要精通，不需要背**。
> 文档给出来的 `AudioOpusEncoder`、`MicrophoneAudioSource`、`DesktopAudioSource`、`AudioPlayback`，是一层**薄的C++ RAII封装**。

### 业务层只和这些类打交道：

1. `AudioOpusEncoder`

```
auto enc = AudioOpusEncoder();
enc.open(48000,1,32000,AudioStreamKind::Microphone);
QByteArray opusData = enc.encodePcm16(pcmBytes);
enc.close();
```

内部已经处理`opus_encoder_create/destroy`，异常返回空字节。

2. `MicrophoneAudioSource`

```
micSource.start(48000,1,20);
//信号 pcmFrameReady(const QByteArray &pcm, quint64 ts) 输出完整20ms PCM
```

3. `DesktopAudioSource`

```
desktopSource.start(48000,2,20);
// pcmFrameReady信号输出20ms立体声PCM
```

4. `AudioPlayback`

```
playback.start(48000,2,80);
playback.playPcm(pcmByteArray);
```

所有底层C API错误、指针释放、缓冲区碎片、COM Release全部在封装类内部消化。

---

# 调试排错手段（API不熟的时候救命）

1. **Opus**
调用open之后，打日志确认是否`isOpen()`；如果encode返回空，打印日志确认输入pcm字节数是不是严格等于一帧大小。

> 
> Opus非常苛刻，输入不是完整20ms帧直接返回空，不自动截断补齐。
2. **Qt QAudioInput / QAudioOutput**
启动之后判断是否isRunning；如果没有pcm数据打印`errorOccurred`信号的错误字符串。很多时候麦克风被占用、格式不兼容直接报错。
3. **WASAPI DesktopAudioSource**
打开失败看`errorOccurred`字符串。
常见：声卡不支持共享模式48k‑16bit，某些虚拟声卡loopback不支持。

> 
> 所有底层库的错误，封装类都会通过`errorOccurred(QString)`信号抛上来，**业务层监听这个信号打印日志，就不用去啃底层返回值**。

---

# 学习路线建议

1. 先**使用封装类把整个音频链路跑通**，不要一开始钻到底层C API源码。
2. 跑通之后，遇到bug再钻进去看封装类内部怎么调用底层API。
3. 有时间再回头系统学Opus、WASAPI。

> 
> 就像你使用FFmpeg，一开始你也是先调用封装好的VideoEncoder，而不是直接手撕avcodec的全部C API。现在音频这边也是一模一样的模式。

如果你后续写代码遇到某个底层API看不懂，直接贴那一小段代码，我给你拆解每一个参数含义。