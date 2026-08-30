# P2Pplay 音频与同步实现文档

本文档描述当前项目统一后的实现方式：

- 音频统一使用 Opus
- `AudioSample` 不再携带编码方式字段
- 桌面音频默认按本进程过滤采集
- 视频样本不再使用 jpeg
- 音视频统一进入同步器，再分发到播放和渲染
- `source / encoder / decoder / playback` 不单独开线程，全部由各自 `Worker` 所在线程执行

本文只描述当前方案。

---

## 一、整体分层

新增或调整的核心模块：

```text
Media/Audio/AudioSample.h
Media/Audio/AudioSampleCodec.h/.cpp
Media/Audio/AudioOpusEncoder.h/.cpp
Media/Audio/AudioOpusDecoder.h/.cpp
Media/Audio/MicrophoneAudioSource.h/.cpp
Media/Audio/DesktopAudioSource.h/.cpp
Media/Audio/AudioPlayback.h/.cpp
Media/Audio/AudioMixer.h/.cpp
Media/Audio/AudioService.h/.cpp
Media/Audio/AudioServiceWorker.h/.cpp

Media/AvSync/AvSyncFrame.h
Media/AvSync/AvSyncWorker.h/.cpp
Media/AvSync/AvSyncService.h/.cpp
```

职责：

```text
MicrophoneAudioSource
    采集麦克风 PCM

DesktopAudioSource
    采集桌面系统声，默认过滤本进程树

AudioOpusEncoder
    PCM -> Opus

AudioOpusDecoder
    Opus -> PCM

AudioMixer
    将多路解码后的 PCM 混成一路播放 PCM

AudioPlayback
    把最终 PCM 写入 QAudioOutput

AudioServiceWorker
    管理音频采集、编码、解码、混音和播放

AvSyncWorker
    接收解码后的音视频帧，按开关和时间戳决定发送给渲染或播放
```

---

## 二、统一数据模型

### 2.1 音频样本

`AudioSample` 只表达“这是一段 Opus 音频”，不再存编码方式字段。

```cpp
// Media/Audio/AudioSample.h
#pragma once

#include <QtCore>

enum class AudioStreamKind : quint8 {
    Unknown = 0,
    Microphone = 1,
    Desktop = 2
};

struct AudioSample {
    quint32 seq = 0;
    quint64 captureTimeStampMs = 0;

    AudioStreamKind streamKind = AudioStreamKind::Unknown;

    quint16 sampleRate = 48000;
    quint8 channels = 1;
    quint16 frameDurationMs = 20;

    QByteArray data; // Opus payload
};
```

### 2.1.1 `AudioSampleCodec`

网络格式固定为：

```text
magic          4 bytes
version        quint8
streamKind     quint8
seq            quint32
capturePtsMs   quint64
sampleRate     quint16
channels       quint8
frameDuration  quint16
payloadSize    quint32
payload        Opus bytes
```

```cpp
// Media/Audio/AudioSampleCodec.h
#pragma once

#include "AudioSample.h"

class AudioSampleCodec {
public:
    static QByteArray encode(const AudioSample& sample);
    static bool decode(const QByteArray& bytes, AudioSample* sample);
};
```

```cpp
// Media/Audio/AudioSampleCodec.cpp
#include "AudioSampleCodec.h"

#include <QDataStream>

namespace {
constexpr quint32 kMagic = 0x50324155; // P2AU
constexpr quint8 kVersion = 1;
}

QByteArray AudioSampleCodec::encode(const AudioSample& sample)
{
    if (sample.data.isEmpty() ||
        sample.streamKind == AudioStreamKind::Unknown ||
        sample.sampleRate == 0 ||
        sample.channels == 0 ||
        sample.frameDurationMs == 0) {
        return {};
    }

    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_14);

    stream << kMagic
           << kVersion
           << static_cast<quint8>(sample.streamKind)
           << sample.seq
           << sample.captureTimeStampMs
           << sample.sampleRate
           << sample.channels
           << sample.frameDurationMs
           << static_cast<quint32>(sample.data.size());
    stream.writeRawData(sample.data.constData(), sample.data.size());
    return stream.status() == QDataStream::Ok ? bytes : QByteArray();
}

bool AudioSampleCodec::decode(const QByteArray& bytes, AudioSample* sample)
{
    if (sample == nullptr || bytes.isEmpty()) {
        return false;
    }

    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_5_14);

    quint32 magic = 0;
    quint8 version = 0;
    quint8 streamKind = 0;
    quint32 seq = 0;
    quint64 ptsMs = 0;
    quint16 sampleRate = 0;
    quint8 channels = 0;
    quint16 frameDurationMs = 0;
    quint32 payloadSize = 0;

    stream >> magic
           >> version
           >> streamKind
           >> seq
           >> ptsMs
           >> sampleRate
           >> channels
           >> frameDurationMs
           >> payloadSize;

    constexpr int kHeaderBytes =
            sizeof(quint32) + sizeof(quint8) + sizeof(quint8) +
            sizeof(quint32) + sizeof(quint64) + sizeof(quint16) +
            sizeof(quint8) + sizeof(quint16) + sizeof(quint32);

    if (stream.status() != QDataStream::Ok ||
        bytes.size() < kHeaderBytes ||
        magic != kMagic ||
        version != kVersion ||
        payloadSize == 0 ||
        payloadSize != static_cast<quint32>(bytes.size() - kHeaderBytes) ||
        (streamKind != static_cast<quint8>(AudioStreamKind::Microphone) &&
         streamKind != static_cast<quint8>(AudioStreamKind::Desktop)) ||
        sampleRate == 0 ||
        (channels != 1 && channels != 2) ||
        frameDurationMs == 0) {
        return false;
    }

    QByteArray payload(static_cast<int>(payloadSize), Qt::Uninitialized);
    if (stream.readRawData(payload.data(), payload.size()) != payload.size()) {
        return false;
    }

    AudioSample result;
    result.seq = seq;
    result.captureTimeStampMs = ptsMs;
    result.streamKind = static_cast<AudioStreamKind>(streamKind);
    result.sampleRate = sampleRate;
    result.channels = channels;
    result.frameDurationMs = frameDurationMs;
    result.data = payload;
    *sample = result;
    return true;
}
```

说明：

```text
sampleRate:
    编解码使用的采样率，默认 48000

channels:
    麦克风默认 1，桌面音频默认 2

frameDurationMs:
    默认 20ms

data:
    Opus 数据，不再区分 codec type
```

### 2.2 视频样本

视频样本不再带 jpeg 语义。编码链路统一输出 H264，样本只保留必要元信息。

```cpp
// Media/Video/VideoSample.h
#pragma once

#include <QtCore>

struct VideoSample {
    quint32 seq = 0;
    quint64 captureTimeStampMs = 0;

    int width = 0;
    int height = 0;
    QByteArray data; // H264
};
```

`VideoEncoderWorker` 只做 H264 编码，`VideoDecoderWorker` 只做 H264 解码。

---

## 三、配置

### 3.1 Opus 配置

把编码参数集中到配置里，不在编码器内部硬编码。

```cpp
// Config/AppConfig.h
struct OpusCodecConfig {
    int sampleRate = 48000;
    int channels = 1;
    int frameDurationMs = 20;
    int bitrateBps = 32000;
    int complexity = 5;
    bool inbandFecEnabled = false;
    bool dtxEnabled = false;
    int maxPacketBytes = 4000;
};
```

### 3.2 音频配置

```cpp
struct AudioConfig {
    bool microphoneEnabled = true;
    bool desktopAudioEnabled = true;
    bool playbackEnabled = true;
    bool avSyncEnabled = true;

    OpusCodecConfig microphoneCodec {
        48000, 1, 20, 32000, 5, false, false, 4000
    };

    OpusCodecConfig desktopCodec {
        48000, 2, 20, 128000, 5, false, false, 4000
    };

    int playbackBufferMs = 80;
    double microphonePlaybackGain = 1.0;
    double desktopPlaybackGain = 0.8;
    int mixerFrameDurationMs = 20;
    int mixerMaxQueuedFramesPerSource = 10;
};
```

### 3.3 配置校验

```cpp
static bool validateOpusCodecConfig(const OpusCodecConfig& config,
                                    QString* errorMessage)
{
    if (config.sampleRate != 8000 &&
        config.sampleRate != 12000 &&
        config.sampleRate != 16000 &&
        config.sampleRate != 24000 &&
        config.sampleRate != 48000) {
        if (errorMessage) *errorMessage = "unsupported sample rate";
        return false;
    }

    if (config.channels != 1 && config.channels != 2) {
        if (errorMessage) *errorMessage = "channels must be 1 or 2";
        return false;
    }

    if (config.frameDurationMs != 5 &&
        config.frameDurationMs != 10 &&
        config.frameDurationMs != 20 &&
        config.frameDurationMs != 40 &&
        config.frameDurationMs != 60) {
        if (errorMessage) *errorMessage = "unsupported frame duration";
        return false;
    }

    if (config.bitrateBps <= 0 ||
        config.complexity < 0 ||
        config.complexity > 10 ||
        config.maxPacketBytes <= 0) {
        if (errorMessage) *errorMessage = "invalid opus config";
        return false;
    }

    return true;
}
```

---

## 四、音频编解码

### 4.1 Opus Encoder

```cpp
// Media/Audio/AudioOpusEncoder.h
#pragma once

#include <QByteArray>

#include "AudioSample.h"
#include "AppConfig.h"

struct OpusEncoder;

class AudioOpusEncoder {
public:
    ~AudioOpusEncoder();

    bool open(const OpusCodecConfig& config, AudioStreamKind kind);
    QByteArray encodePcm16(const QByteArray& pcm);
    void close();
    bool isOpen() const;

private:
    ::OpusEncoder* encoderHandle_ = nullptr;
    OpusCodecConfig config_;
    AudioStreamKind kind_ = AudioStreamKind::Unknown;
};
```

```cpp
// Media/Audio/AudioOpusEncoder.cpp
#include "AudioOpusEncoder.h"

#include <opus/opus.h>

#include <algorithm>
#include <cstring>

AudioOpusEncoder::~AudioOpusEncoder()
{
    close();
}

bool AudioOpusEncoder::open(const OpusCodecConfig& config,
                            AudioStreamKind kind)
{
    close();

    if ((config.sampleRate != 8000 &&
         config.sampleRate != 12000 &&
         config.sampleRate != 16000 &&
         config.sampleRate != 24000 &&
         config.sampleRate != 48000) ||
        (config.channels != 1 && config.channels != 2) ||
        config.frameDurationMs <= 0 ||
        config.bitrateBps <= 0 ||
        config.maxPacketBytes <= 0 ||
        kind == AudioStreamKind::Unknown) {
        return false;
    }

    int error = OPUS_OK;
    encoderHandle_ = ::opus_encoder_create(
            config.sampleRate,
            config.channels,
            OPUS_APPLICATION_AUDIO,
            &error);
    if (encoderHandle_ == nullptr || error != OPUS_OK) {
        close();
        return false;
    }

    if (::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_BITRATE(config.bitrateBps)) != OPUS_OK ||
        ::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_COMPLEXITY(config.complexity)) != OPUS_OK ||
        ::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_INBAND_FEC(
                               config.inbandFecEnabled ? 1 : 0)) != OPUS_OK ||
        ::opus_encoder_ctl(encoderHandle_,
                           OPUS_SET_DTX(config.dtxEnabled ? 1 : 0)) != OPUS_OK) {
        close();
        return false;
    }

    config_ = config;
    kind_ = kind;
    return true;
}

QByteArray AudioOpusEncoder::encodePcm16(const QByteArray& pcm)
{
    if (!isOpen()) {
        return {};
    }

    const int frameSamples =
            config_.sampleRate * config_.frameDurationMs / 1000;
    const int expectedBytes =
            frameSamples * config_.channels * static_cast<int>(sizeof(opus_int16));
    if (frameSamples <= 0 || pcm.size() != expectedBytes) {
        return {};
    }

    QByteArray encoded(config_.maxPacketBytes, Qt::Uninitialized);
    const int encodedBytes = ::opus_encode(
            encoderHandle_,
            reinterpret_cast<const opus_int16*>(pcm.constData()),
            frameSamples,
            reinterpret_cast<unsigned char*>(encoded.data()),
            encoded.size());
    if (encodedBytes <= 0) {
        return {};
    }

    encoded.resize(encodedBytes);
    return encoded;
}

void AudioOpusEncoder::close()
{
    if (encoderHandle_ != nullptr) {
        ::opus_encoder_destroy(encoderHandle_);
        encoderHandle_ = nullptr;
    }
    kind_ = AudioStreamKind::Unknown;
}

bool AudioOpusEncoder::isOpen() const
{
    return encoderHandle_ != nullptr;
}
```

规则：

```text
open() 只接收完整配置
编码器内部不再拆散参数
不能在函数内部硬编码采样率、声道、码率、帧长
```

### 4.2 Opus Decoder

```cpp
// Media/Audio/AudioOpusDecoder.h
#pragma once

#include <QByteArray>

#include "AppConfig.h"

struct OpusDecoder;

class AudioOpusDecoder {
public:
    ~AudioOpusDecoder();

    bool open(const OpusCodecConfig& config);
    QByteArray decodeToPcm16(const QByteArray& opusBytes);
    void close();
    bool isOpen() const;

private:
    ::OpusDecoder* decoderHandle_ = nullptr;
    OpusCodecConfig config_;
};
```

解码器以最大 Opus 帧尺寸作为 `frame_size` 上限，避免和发送端帧长硬绑定。

```cpp
// Media/Audio/AudioOpusDecoder.cpp
#include "AudioOpusDecoder.h"

#include <opus/opus.h>

AudioOpusDecoder::~AudioOpusDecoder()
{
    close();
}

bool AudioOpusDecoder::open(const OpusCodecConfig& config)
{
    close();

    if ((config.sampleRate != 8000 &&
         config.sampleRate != 12000 &&
         config.sampleRate != 16000 &&
         config.sampleRate != 24000 &&
         config.sampleRate != 48000) ||
        (config.channels != 1 && config.channels != 2)) {
        return false;
    }

    int error = OPUS_OK;
    decoderHandle_ = ::opus_decoder_create(
            config.sampleRate,
            config.channels,
            &error);
    if (decoderHandle_ == nullptr || error != OPUS_OK) {
        close();
        return false;
    }

    config_ = config;
    return true;
}

QByteArray AudioOpusDecoder::decodeToPcm16(const QByteArray& opusBytes)
{
    if (!isOpen() || opusBytes.isEmpty()) {
        return {};
    }

    const int maxFrameSamples =
            config_.sampleRate * 120 / 1000;
    QByteArray pcm(maxFrameSamples * config_.channels *
                   static_cast<int>(sizeof(opus_int16)),
                   Qt::Uninitialized);

    const int decodedSamples = ::opus_decode(
            decoderHandle_,
            reinterpret_cast<const unsigned char*>(opusBytes.constData()),
            opusBytes.size(),
            reinterpret_cast<opus_int16*>(pcm.data()),
            maxFrameSamples,
            0);
    if (decodedSamples <= 0) {
        return {};
    }

    pcm.resize(decodedSamples * config_.channels *
               static_cast<int>(sizeof(opus_int16)));
    return pcm;
}

void AudioOpusDecoder::close()
{
    if (decoderHandle_ != nullptr) {
        ::opus_decoder_destroy(decoderHandle_);
        decoderHandle_ = nullptr;
    }
}

bool AudioOpusDecoder::isOpen() const
{
    return decoderHandle_ != nullptr;
}
```

---

## 五、音频采集

### 5.1 麦克风采集

```cpp
// Media/Audio/MicrophoneAudioSource.h
#pragma once

#include <QAudioInput>
#include <QByteArray>
#include <QIODevice>
#include <QObject>

class MicrophoneAudioSource : public QObject {
    Q_OBJECT
public:
    explicit MicrophoneAudioSource(QObject* parent = nullptr);
    ~MicrophoneAudioSource() override;

    bool start(int sampleRate, int channels, int frameDurationMs);
    void stop();
    bool isRunning() const;

signals:
    void pcmFrameReady(const QByteArray& pcm, quint64 captureTimeStampMs);
    void errorOccurred(const QString& reason);

private slots:
    void onReadyRead();

private:
    QAudioFormat buildFormat(int sampleRate, int channels) const;
    void emitCompleteFrames();

    QAudioInput* audioInput_ = nullptr;
    QIODevice* inputDevice_ = nullptr;
    QByteArray pendingPcm_;
    int sampleRate_ = 48000;
    int channels_ = 1;
    int frameDurationMs_ = 20;
    int frameBytes_ = 0;
    bool running_ = false;
};
```

麦克风固定输出 `PCM s16`，再按 20ms 切帧。

```cpp
// Media/Audio/MicrophoneAudioSource.cpp
#include "MicrophoneAudioSource.h"

#include <QAudioDeviceInfo>
#include <QDateTime>

MicrophoneAudioSource::MicrophoneAudioSource(QObject* parent)
    : QObject(parent)
{
}

MicrophoneAudioSource::~MicrophoneAudioSource()
{
    stop();
}

bool MicrophoneAudioSource::start(int sampleRate,
                                  int channels,
                                  int frameDurationMs)
{
    stop();

    if (sampleRate <= 0 || channels != 1 || frameDurationMs != 20) {
        emit errorOccurred("invalid microphone format");
        return false;
    }

    const QAudioFormat format = buildFormat(sampleRate, channels);
    const QAudioDeviceInfo inputInfo =
            QAudioDeviceInfo::defaultInputDevice();
    if (!inputInfo.isFormatSupported(format)) {
        emit errorOccurred("microphone format is not supported");
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;
    frameDurationMs_ = frameDurationMs;
    frameBytes_ = sampleRate_ * frameDurationMs_ / 1000 *
                  channels_ * static_cast<int>(sizeof(qint16));
    pendingPcm_.clear();

    audioInput_ = new QAudioInput(inputInfo, format, this);
    inputDevice_ = audioInput_->start();
    if (inputDevice_ == nullptr) {
        delete audioInput_;
        audioInput_ = nullptr;
        emit errorOccurred("failed to start microphone");
        return false;
    }

    connect(inputDevice_, &QIODevice::readyRead,
            this, &MicrophoneAudioSource::onReadyRead);
    running_ = true;
    return true;
}

void MicrophoneAudioSource::stop()
{
    running_ = false;
    pendingPcm_.clear();
    inputDevice_ = nullptr;

    if (audioInput_ != nullptr) {
        audioInput_->stop();
        delete audioInput_;
        audioInput_ = nullptr;
    }
}

bool MicrophoneAudioSource::isRunning() const
{
    return running_;
}

void MicrophoneAudioSource::onReadyRead()
{
    if (!running_ || inputDevice_ == nullptr) {
        return;
    }

    pendingPcm_.append(inputDevice_->readAll());
    emitCompleteFrames();
}

QAudioFormat MicrophoneAudioSource::buildFormat(int sampleRate,
                                                int channels) const
{
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
    return format;
}

void MicrophoneAudioSource::emitCompleteFrames()
{
    while (frameBytes_ > 0 && pendingPcm_.size() >= frameBytes_) {
        const QByteArray frame = pendingPcm_.left(frameBytes_);
        pendingPcm_.remove(0, frameBytes_);
        emit pcmFrameReady(
                frame,
                static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
    }
}
```

### 5.2 桌面音频采集

桌面音频默认按本进程过滤，目标是只采到 Host 想共享的系统声。

```cpp
// Media/Audio/DesktopAudioSource.h
#pragma once

#include <QByteArray>
#include <QLibrary>
#include <QObject>
#include <QTimer>

#include <windows.h>

enum WasapiCaptureMode {
    WASAPI_CAPTURE_SYSTEM_LOOPBACK = 0,
    WASAPI_CAPTURE_INCLUDE_PROCESS_TREE = 1,
    WASAPI_CAPTURE_EXCLUDE_PROCESS_TREE = 2
};

typedef void* WasapiCaptureHandle;

class DesktopAudioSource : public QObject {
    Q_OBJECT
public:
    explicit DesktopAudioSource(QObject* parent = nullptr);
    ~DesktopAudioSource() override;

    bool start(int sampleRate,
               int channels,
               int frameDurationMs,
               WasapiCaptureMode captureMode,
               DWORD targetProcessId);
    void stop();
    bool isRunning() const;

signals:
    void pcmFrameReady(const QByteArray& pcm, quint64 captureTimeStampMs);
    void errorOccurred(const QString& reason);

private slots:
    void poll();

private:
    bool loadLibrary();
    void unloadLibrary();
    void appendPcm(const QByteArray& pcm);
    void emitCompleteFrames();

    QTimer pollTimer_;
    QLibrary wasapiLibrary_;
    QByteArray pendingPcm_;

    WasapiCaptureHandle captureHandle_ = nullptr;
    int sampleRate_ = 48000;
    int channels_ = 2;
    int frameDurationMs_ = 20;
    int frameBytes_ = 0;
    bool running_ = false;

    using CreateFn = int (*)(WasapiCaptureHandle*);
    using DestroyFn = void (*)(WasapiCaptureHandle);
    using StartFn = int (*)(WasapiCaptureHandle,
                            WasapiCaptureMode,
                            DWORD);
    using StopFn = void (*)(WasapiCaptureHandle);
    using ReadFn = int (*)(WasapiCaptureHandle,
                           unsigned char*,
                           int,
                           int);

    CreateFn create_ = nullptr;
    DestroyFn destroy_ = nullptr;
    StartFn startCapture_ = nullptr;
    StopFn stopCapture_ = nullptr;
    ReadFn readCapture_ = nullptr;
};
```

默认启动方式：

```cpp
const DWORD selfPid = wasapi_capture_current_process_id();
desktopSource.start(48000, 2, 20,
                    WASAPI_CAPTURE_EXCLUDE_PROCESS_TREE,
                    selfPid);
```

如果要只抓某个进程声音，就改成：

```cpp
WASAPI_CAPTURE_INCLUDE_PROCESS_TREE
```

`DesktopAudioSource` 内部不负责复杂业务，只做：

```text
1. 调用 wasapi_dll
2. 读取 PCM
3. 没有数据时输出静音
4. 累积到 20ms 后发出 pcmFrameReady
```

```cpp
// Media/Audio/DesktopAudioSource.cpp
#include "DesktopAudioSource.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QLibrary>

#include <algorithm>

DesktopAudioSource::DesktopAudioSource(QObject* parent)
    : QObject(parent),
      pollTimer_(this)
{
    pollTimer_.setTimerType(Qt::PreciseTimer);
    connect(&pollTimer_, &QTimer::timeout,
            this, &DesktopAudioSource::poll);
}

DesktopAudioSource::~DesktopAudioSource()
{
    stop();
}

bool DesktopAudioSource::start(int sampleRate,
                               int channels,
                               int frameDurationMs,
                               WasapiCaptureMode captureMode,
                               DWORD targetProcessId)
{
    stop();

    if (sampleRate != 48000 ||
        channels != 2 ||
        frameDurationMs != 20 ||
        targetProcessId == 0 ||
        captureMode != WASAPI_CAPTURE_EXCLUDE_PROCESS_TREE) {
        emit errorOccurred("invalid desktop capture configuration");
        return false;
    }

    if (!loadLibrary()) {
        emit errorOccurred("failed to load wasapi_dll.dll");
        return false;
    }

    if (create_(&captureHandle_) != 0 || captureHandle_ == nullptr) {
        emit errorOccurred("wasapi capture create failed");
        unloadLibrary();
        return false;
    }

    if (startCapture_(captureHandle_, captureMode, targetProcessId) != 0) {
        emit errorOccurred("wasapi process loopback start failed");
        destroy_(captureHandle_);
        captureHandle_ = nullptr;
        unloadLibrary();
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;
    frameDurationMs_ = frameDurationMs;
    frameBytes_ = sampleRate_ * frameDurationMs_ / 1000 *
                  channels_ * static_cast<int>(sizeof(qint16));
    pendingPcm_.clear();
    running_ = true;
    pollTimer_.start(5);
    return true;
}

void DesktopAudioSource::stop()
{
    pollTimer_.stop();
    running_ = false;
    pendingPcm_.clear();

    if (captureHandle_ != nullptr) {
        stopCapture_(captureHandle_);
        destroy_(captureHandle_);
        captureHandle_ = nullptr;
    }

    unloadLibrary();
}

bool DesktopAudioSource::isRunning() const
{
    return running_;
}

bool DesktopAudioSource::loadLibrary()
{
    const QString libraryPath =
            QCoreApplication::applicationDirPath() + "/wasapi_dll.dll";
    wasapiLibrary_.setFileName(libraryPath);
    if (!wasapiLibrary_.load()) {
        return false;
    }

    create_ = reinterpret_cast<CreateFn>(
            wasapiLibrary_.resolve("wasapi_capture_create"));
    destroy_ = reinterpret_cast<DestroyFn>(
            wasapiLibrary_.resolve("wasapi_capture_destroy"));
    startCapture_ = reinterpret_cast<StartFn>(
            wasapiLibrary_.resolve("wasapi_capture_start"));
    stopCapture_ = reinterpret_cast<StopFn>(
            wasapiLibrary_.resolve("wasapi_capture_stop"));
    readCapture_ = reinterpret_cast<ReadFn>(
            wasapiLibrary_.resolve("wasapi_capture_read"));

    if (create_ == nullptr ||
        destroy_ == nullptr ||
        startCapture_ == nullptr ||
        stopCapture_ == nullptr ||
        readCapture_ == nullptr) {
        unloadLibrary();
        return false;
    }
    return true;
}

void DesktopAudioSource::unloadLibrary()
{
    create_ = nullptr;
    destroy_ = nullptr;
    startCapture_ = nullptr;
    stopCapture_ = nullptr;
    readCapture_ = nullptr;
    if (wasapiLibrary_.isLoaded()) {
        wasapiLibrary_.unload();
    }
}

void DesktopAudioSource::poll()
{
    if (!running_ || captureHandle_ == nullptr || readCapture_ == nullptr) {
        return;
    }

    unsigned char buffer[64 * 1024];
    const int bytesRead = readCapture_(
            captureHandle_, buffer, sizeof(buffer), 20);
    if (bytesRead < 0) {
        emit errorOccurred("wasapi capture read failed");
        stop();
        return;
    }

    if (bytesRead > 0) {
        appendPcm(QByteArray(
                reinterpret_cast<const char*>(buffer), bytesRead));
    }
}

void DesktopAudioSource::appendPcm(const QByteArray& pcm)
{
    if (pcm.isEmpty() || frameBytes_ <= 0) {
        return;
    }

    pendingPcm_.append(pcm);
    emitCompleteFrames();
}

void DesktopAudioSource::emitCompleteFrames()
{
    while (frameBytes_ > 0 && pendingPcm_.size() >= frameBytes_) {
        const QByteArray frame = pendingPcm_.left(frameBytes_);
        pendingPcm_.remove(0, frameBytes_);
        emit pcmFrameReady(
                frame,
                static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
    }
}
```

---

## 六、音频播放与混音

### 6.1 AudioPlayback

```cpp
// Media/Audio/AudioPlayback.h
#pragma once

#include <QAudioOutput>
#include <QByteArray>
#include <QObject>

class AudioPlayback : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayback(QObject* parent = nullptr);
    ~AudioPlayback() override;

    bool start(int sampleRate, int channels, int bufferMs);
    void stop();
    bool isRunning() const;
    void playPcm(const QByteArray& pcm);

signals:
    void errorOccurred(const QString& reason);

private:
    QAudioOutput* audioOutput_ = nullptr;
    QIODevice* outputDevice_ = nullptr;
    int sampleRate_ = 48000;
    int channels_ = 2;
    int bufferMs_ = 80;
};
```

`AudioPlayback` 只负责把最终 PCM 写给 `QAudioOutput`，不认识“麦克风”或“桌面音频”。

```cpp
// Media/Audio/AudioPlayback.cpp
#include "AudioPlayback.h"

#include <QAudioDeviceInfo>
#include <QAudioFormat>

AudioPlayback::AudioPlayback(QObject* parent)
    : QObject(parent)
{
}

AudioPlayback::~AudioPlayback()
{
    stop();
}

bool AudioPlayback::start(int sampleRate, int channels, int bufferMs)
{
    stop();

    if (sampleRate <= 0 ||
        (channels != 1 && channels != 2) ||
        bufferMs <= 0) {
        emit errorOccurred("invalid playback format");
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    const QAudioDeviceInfo outputInfo =
            QAudioDeviceInfo::defaultOutputDevice();
    if (!outputInfo.isFormatSupported(format)) {
        emit errorOccurred("playback format is not supported");
        return false;
    }

    audioOutput_ = new QAudioOutput(outputInfo, format, this);
    audioOutput_->setBufferSize(
            sampleRate * channels * 2 * bufferMs / 1000);
    outputDevice_ = audioOutput_->start();
    if (outputDevice_ == nullptr) {
        delete audioOutput_;
        audioOutput_ = nullptr;
        emit errorOccurred("failed to start audio output");
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;
    bufferMs_ = bufferMs;
    return true;
}

void AudioPlayback::stop()
{
    outputDevice_ = nullptr;
    if (audioOutput_ != nullptr) {
        audioOutput_->stop();
        delete audioOutput_;
        audioOutput_ = nullptr;
    }
}

bool AudioPlayback::isRunning() const
{
    return audioOutput_ != nullptr &&
           outputDevice_ != nullptr &&
           audioOutput_->state() != QAudio::StoppedState;
}

void AudioPlayback::playPcm(const QByteArray& pcm)
{
    if (!isRunning() || pcm.isEmpty()) {
        return;
    }

    const qint64 writable = outputDevice_->bytesFree();
    if (writable <= 0) {
        return;
    }

    const int bytesToWrite = static_cast<int>(
            qMin<qint64>(writable, pcm.size()));
    if (outputDevice_->write(pcm.constData(), bytesToWrite) != bytesToWrite) {
        emit errorOccurred("failed to write audio output");
    }
}
```

### 6.2 AudioMixer

Guest 端可能同时收到麦克风和桌面音频，需要先混成一路再播放。

```cpp
// Media/Audio/AudioMixer.h
#pragma once

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QTimer>

#include "AvSync/AvSyncFrame.h"

class AudioMixer : public QObject {
    Q_OBJECT
public:
    explicit AudioMixer(QObject* parent = nullptr);
    ~AudioMixer() override;

    void setGains(double microphoneGain, double desktopGain);
    void setMaxQueuedFramesPerSource(int maxQueuedFrames);

    bool start(int sampleRate, int frameDurationMs);
    void stop();
    void clear();
    void pushFrame(const DecodedAudioFrame& frame);

signals:
    void mixedPcmReady(const QByteArray& pcm);
    void error(const QString& message);

private slots:
    void mixOnce();

private:
    struct PendingFrame {
        QByteArray pcm;
        int sampleRate = 48000;
        int channels = 1;
        AudioStreamKind streamKind = AudioStreamKind::Unknown;
        quint64 ptsMs = 0;
    };

    static int frameCountOf(const PendingFrame& frame);
    static qint16 readSample(const PendingFrame& frame,
                             int frameIndex,
                             int channelIndex);
    static qint16 clampToS16(double value);

    QTimer mixTimer_;
    QQueue<PendingFrame> microphoneFrames_;
    QQueue<PendingFrame> desktopFrames_;
    int sampleRate_ = 48000;
    int frameDurationMs_ = 20;
    int maxQueuedFramesPerSource_ = 10;
    double microphoneGain_ = 1.0;
    double desktopGain_ = 0.8;
    bool running_ = false;
};
```

混音输出固定为 stereo s16，输入缺一路时自动补静音。

```cpp
// Media/Audio/AudioMixer.cpp
#include "AudioMixer.h"

#include <algorithm>
#include <cmath>
#include <limits>

AudioMixer::AudioMixer(QObject* parent)
    : QObject(parent)
{
    mixTimer_.setTimerType(Qt::PreciseTimer);
    connect(&mixTimer_, &QTimer::timeout,
            this, &AudioMixer::mixOnce);
}

AudioMixer::~AudioMixer()
{
    stop();
}

void AudioMixer::setGains(double microphoneGain, double desktopGain)
{
    microphoneGain_ = std::max(0.0, microphoneGain);
    desktopGain_ = std::max(0.0, desktopGain);
}

void AudioMixer::setMaxQueuedFramesPerSource(int maxQueuedFrames)
{
    maxQueuedFramesPerSource_ = std::max(1, maxQueuedFrames);
}

bool AudioMixer::start(int sampleRate, int frameDurationMs)
{
    if (sampleRate <= 0 || frameDurationMs <= 0) {
        emit error("invalid mixer format");
        return false;
    }

    sampleRate_ = sampleRate;
    frameDurationMs_ = frameDurationMs;
    clear();
    running_ = true;
    mixTimer_.start(frameDurationMs_);
    return true;
}

void AudioMixer::stop()
{
    mixTimer_.stop();
    running_ = false;
    clear();
}

void AudioMixer::clear()
{
    microphoneFrames_.clear();
    desktopFrames_.clear();
}

void AudioMixer::pushFrame(const DecodedAudioFrame& frame)
{
    if (!running_ || frame.pcm.isEmpty() ||
        frame.sampleRate != sampleRate_ ||
        (frame.channels != 1 && frame.channels != 2)) {
        return;
    }

    PendingFrame pending;
    pending.pcm = frame.pcm;
    pending.sampleRate = frame.sampleRate;
    pending.channels = frame.channels;
    pending.streamKind = frame.streamKind;
    pending.ptsMs = frame.ptsMs;

    QQueue<PendingFrame>* queue = nullptr;
    if (frame.streamKind == AudioStreamKind::Microphone) {
        queue = &microphoneFrames_;
    } else if (frame.streamKind == AudioStreamKind::Desktop) {
        queue = &desktopFrames_;
    }
    if (queue == nullptr) {
        return;
    }

    queue->enqueue(pending);
    while (queue->size() > maxQueuedFramesPerSource_) {
        queue->dequeue();
    }
}

void AudioMixer::mixOnce()
{
    if (!running_ ||
        (microphoneFrames_.isEmpty() && desktopFrames_.isEmpty())) {
        return;
    }

    PendingFrame microphone;
    PendingFrame desktop;
    const PendingFrame* microphonePtr = nullptr;
    const PendingFrame* desktopPtr = nullptr;

    if (!microphoneFrames_.isEmpty()) {
        microphone = microphoneFrames_.dequeue();
        microphonePtr = &microphone;
    }
    if (!desktopFrames_.isEmpty()) {
        desktop = desktopFrames_.dequeue();
        desktopPtr = &desktop;
    }

    const int microphoneCount = microphonePtr
            ? frameCountOf(*microphonePtr) : 0;
    const int desktopCount = desktopPtr
            ? frameCountOf(*desktopPtr) : 0;
    const int frameCount = std::max(microphoneCount, desktopCount);
    if (frameCount <= 0) {
        return;
    }

    QByteArray output(frameCount * 2 * static_cast<int>(sizeof(qint16)),
                      Qt::Uninitialized);
    auto* destination = reinterpret_cast<qint16*>(output.data());
    for (int i = 0; i < frameCount; ++i) {
        for (int channel = 0; channel < 2; ++channel) {
            double value = 0.0;
            if (microphonePtr) {
                value += readSample(*microphonePtr, i, channel) *
                         microphoneGain_;
            }
            if (desktopPtr) {
                value += readSample(*desktopPtr, i, channel) *
                         desktopGain_;
            }
            destination[i * 2 + channel] = clampToS16(value);
        }
    }

    emit mixedPcmReady(output);
}

int AudioMixer::frameCountOf(const PendingFrame& frame)
{
    if (frame.channels <= 0) {
        return 0;
    }
    return frame.pcm.size() /
           static_cast<int>(sizeof(qint16)) /
           frame.channels;
}

qint16 AudioMixer::readSample(const PendingFrame& frame,
                              int frameIndex,
                              int channelIndex)
{
    const int frameCount = frameCountOf(frame);
    if (frameIndex < 0 || frameIndex >= frameCount) {
        return 0;
    }

    const auto* samples =
            reinterpret_cast<const qint16*>(frame.pcm.constData());
    if (frame.channels == 1) {
        return samples[frameIndex];
    }

    const int channel = std::max(0, std::min(channelIndex,
                                               frame.channels - 1));
    return samples[frameIndex * frame.channels + channel];
}

qint16 AudioMixer::clampToS16(double value)
{
    const double minimum =
            static_cast<double>(std::numeric_limits<qint16>::min());
    const double maximum =
            static_cast<double>(std::numeric_limits<qint16>::max());
    return static_cast<qint16>(std::lrint(
            std::max(minimum, std::min(maximum, value))));
}
```

---

## 七、同步帧

```cpp
// Media/AvSync/AvSyncFrame.h
#pragma once

#include <QtCore>
#include <QImage>

#include "Audio/AudioSample.h"

struct DecodedAudioFrame {
    AudioStreamKind streamKind = AudioStreamKind::Unknown;
    quint64 ptsMs = 0;
    int sampleRate = 48000;
    int channels = 1;
    QByteArray pcm;
};

struct DecodedVideoFrame {
    quint64 ptsMs = 0;
    int width = 0;
    int height = 0;
    QImage image;
};
```

时间戳统一使用 `captureTimeStampMs` 或解码后的 `ptsMs`，不要在编码器里重新生成。

---

## 八、AudioServiceWorker

```cpp
// Media/Audio/AudioServiceWorker.h
#pragma once

#include <QObject>

#include "AppConfig.h"
#include "Role.h"
#include "AudioSample.h"
#include "AudioSampleCodec.h"
#include "AudioOpusEncoder.h"
#include "AudioOpusDecoder.h"
#include "MicrophoneAudioSource.h"
#include "DesktopAudioSource.h"
#include "AudioMixer.h"
#include "AudioPlayback.h"
#include "AvSync/AvSyncFrame.h"

class AudioServiceWorker : public QObject {
    Q_OBJECT
public:
    explicit AudioServiceWorker(QObject* parent = nullptr);
    ~AudioServiceWorker() override;

public slots:
    void applyConfig(const AppConfig& config);
    void setRole(Role role);
    void start();
    void stop();

    void setMicrophoneEnabled(bool enabled);
    void setDesktopAudioEnabled(bool enabled);
    void setPlaybackEnabled(bool enabled);

    void onAudioSampleBytesReceived(const QByteArray& bytes);
    void onAudioFrameToPlay(const DecodedAudioFrame& frame);

signals:
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private slots:
    void onMicrophonePcmFrame(const QByteArray& pcm,
                              quint64 captureTimeStampMs);
    void onDesktopPcmFrame(const QByteArray& pcm,
                           quint64 captureTimeStampMs);

private:
    bool ensureMicrophoneEncoder();
    bool ensureDesktopEncoder();
    bool ensureDecoder(AudioStreamKind kind);
    void startSources();
    void stopSources();

    Role role_ = Role::Unknown;
    bool running_ = false;
    bool microphoneEnabled_ = true;
    bool desktopAudioEnabled_ = true;
    bool playbackEnabled_ = true;

    AudioConfig config_;
    quint32 nextMicrophoneSeq_ = 0;
    quint32 nextDesktopSeq_ = 0;

    MicrophoneAudioSource microphoneSource_;
    DesktopAudioSource desktopSource_;
    AudioOpusEncoder microphoneEncoder_;
    AudioOpusEncoder desktopEncoder_;
    AudioOpusDecoder microphoneDecoder_;
    AudioOpusDecoder desktopDecoder_;
    AudioMixer audioMixer_;
    AudioPlayback audioPlayback_;
};
```

内部成员：

```cpp
MicrophoneAudioSource microphoneSource_;
DesktopAudioSource desktopSource_;
AudioOpusEncoder microphoneEncoder_;
AudioOpusEncoder desktopEncoder_;
AudioOpusDecoder microphoneDecoder_;
AudioOpusDecoder desktopDecoder_;
AudioMixer audioMixer_;
AudioPlayback audioPlayback_;
AudioConfig config_;
```

关键流程：

```text
Host 麦克风:
    MicrophoneAudioSource -> OpusEncoder -> AudioSample -> 网络发送

Host 桌面音频:
    DesktopAudioSource -> OpusEncoder -> AudioSample -> 网络发送

Guest 收到音频:
    网络 -> AudioSampleCodec -> OpusDecoder -> AvSyncWorker
    -> AudioMixer -> AudioPlayback
```

### 8.1 采集后编码

```cpp
void AudioServiceWorker::onMicrophonePcmFrame(const QByteArray& pcm,
                                              quint64 captureTimeStampMs)
{
    if (!running_ || !microphoneEnabled_) {
        return;
    }

    if (!ensureMicrophoneEncoder()) {
        emit errorOccurred("microphone encoder unavailable");
        return;
    }

    const QByteArray opusBytes = microphoneEncoder_.encodePcm16(pcm);
    if (opusBytes.isEmpty()) {
        return;
    }

    AudioSample sample;
    sample.seq = nextMicrophoneSeq_++;
    sample.captureTimeStampMs = captureTimeStampMs;
    sample.streamKind = AudioStreamKind::Microphone;
    sample.sampleRate = static_cast<quint16>(config_.microphoneCodec.sampleRate);
    sample.channels = static_cast<quint8>(config_.microphoneCodec.channels);
    sample.frameDurationMs = static_cast<quint16>(config_.microphoneCodec.frameDurationMs);
    sample.data = opusBytes;

    emit audioSampleBytesReady(AudioSampleCodec::encode(sample));
}
```

```cpp
void AudioServiceWorker::onDesktopPcmFrame(const QByteArray& pcm,
                                           quint64 captureTimeStampMs)
{
    if (!running_ || role_ != Role::Host || !desktopAudioEnabled_) {
        return;
    }

    if (!ensureDesktopEncoder()) {
        emit errorOccurred("desktop encoder unavailable");
        return;
    }

    const QByteArray opusBytes = desktopEncoder_.encodePcm16(pcm);
    if (opusBytes.isEmpty()) {
        return;
    }

    AudioSample sample;
    sample.seq = nextDesktopSeq_++;
    sample.captureTimeStampMs = captureTimeStampMs;
    sample.streamKind = AudioStreamKind::Desktop;
    sample.sampleRate = static_cast<quint16>(config_.desktopCodec.sampleRate);
    sample.channels = static_cast<quint8>(config_.desktopCodec.channels);
    sample.frameDurationMs = static_cast<quint16>(config_.desktopCodec.frameDurationMs);
    sample.data = opusBytes;

    emit audioSampleBytesReady(AudioSampleCodec::encode(sample));
}
```

### 8.2 收到网络音频

```cpp
void AudioServiceWorker::onAudioSampleBytesReceived(const QByteArray& bytes)
{
    AudioSample sample;
    if (!AudioSampleCodec::decode(bytes, &sample)) {
        return;
    }

    if (sample.streamKind == AudioStreamKind::Microphone) {
        if (!microphoneDecoder_.isOpen()) {
            microphoneDecoder_.open(config_.microphoneCodec);
        }
        const QByteArray pcm = microphoneDecoder_.decodeToPcm16(sample.data);
        if (!pcm.isEmpty()) {
            emit decodedAudioFrameReady(
                DecodedAudioFrame{
                    AudioStreamKind::Microphone,
                    sample.captureTimeStampMs,
                    config_.microphoneCodec.sampleRate,
                    config_.microphoneCodec.channels,
                    pcm
                });
        }
        return;
    }

    if (sample.streamKind == AudioStreamKind::Desktop) {
        if (!desktopDecoder_.isOpen()) {
            desktopDecoder_.open(config_.desktopCodec);
        }
        const QByteArray pcm = desktopDecoder_.decodeToPcm16(sample.data);
        if (!pcm.isEmpty()) {
            emit decodedAudioFrameReady(
                DecodedAudioFrame{
                    AudioStreamKind::Desktop,
                    sample.captureTimeStampMs,
                    config_.desktopCodec.sampleRate,
                    config_.desktopCodec.channels,
                    pcm
                });
        }
    }
}
```

### 8.3 播放

```cpp
void AudioServiceWorker::onAudioFrameToPlay(const DecodedAudioFrame& frame)
{
    if (!running_ || !playbackEnabled_) {
        return;
    }

    audioMixer_.pushFrame(frame);
}
```

`AudioMixer` 输出混合 PCM 后，再交给 `AudioPlayback`。

```cpp
// Media/Audio/AudioServiceWorker.cpp
#include "AudioServiceWorker.h"

#include <QCoreApplication>
#include <windows.h>

AudioServiceWorker::AudioServiceWorker(QObject* parent)
    : QObject(parent),
      microphoneSource_(this),
      desktopSource_(this),
      audioMixer_(this),
      audioPlayback_(this)
{
    connect(&microphoneSource_,
            &MicrophoneAudioSource::pcmFrameReady,
            this,
            &AudioServiceWorker::onMicrophonePcmFrame);
    connect(&desktopSource_,
            &DesktopAudioSource::pcmFrameReady,
            this,
            &AudioServiceWorker::onDesktopPcmFrame);

    connect(&microphoneSource_,
            &MicrophoneAudioSource::errorOccurred,
            this,
            &AudioServiceWorker::errorOccurred);
    connect(&desktopSource_,
            &DesktopAudioSource::errorOccurred,
            this,
            &AudioServiceWorker::errorOccurred);
    connect(&audioMixer_,
            &AudioMixer::mixedPcmReady,
            this,
            [this](const QByteArray& pcm) {
                if (!running_ || !playbackEnabled_) {
                    return;
                }
                if (!audioPlayback_.isRunning() &&
                    !audioPlayback_.start(
                        config_.desktopCodec.sampleRate,
                        2,
                        config_.playbackBufferMs)) {
                    emit errorOccurred("failed to start audio playback");
                    return;
                }
                audioPlayback_.playPcm(pcm);
            });
    connect(&audioPlayback_,
            &AudioPlayback::errorOccurred,
            this,
            &AudioServiceWorker::errorOccurred);
}

AudioServiceWorker::~AudioServiceWorker()
{
    stop();
}

void AudioServiceWorker::applyConfig(const AppConfig& config)
{
    QString error;
    if (!validateOpusCodecConfig(config.audio.microphoneCodec, &error) ||
        !validateOpusCodecConfig(config.audio.desktopCodec, &error)) {
        emit errorOccurred(QString("invalid audio config: %1").arg(error));
        return;
    }

    const bool wasRunning = running_;
    if (wasRunning) {
        stop();
    }

    config_ = config.audio;
    microphoneEnabled_ = config_.microphoneEnabled;
    desktopAudioEnabled_ = config_.desktopAudioEnabled;
    playbackEnabled_ = config_.playbackEnabled;

    audioMixer_.setGains(config_.microphonePlaybackGain,
                         config_.desktopPlaybackGain);
    audioMixer_.setMaxQueuedFramesPerSource(
            config_.mixerMaxQueuedFramesPerSource);

    if (wasRunning) {
        start();
    }
}

void AudioServiceWorker::setRole(Role role)
{
    if (running_ && role_ != role) {
        stop();
    }
    role_ = role;
}

void AudioServiceWorker::start()
{
    if (running_) {
        return;
    }
    if (role_ == Role::Unknown) {
        emit errorOccurred("audio role is unknown");
        return;
    }

    running_ = true;
    if (!audioMixer_.start(config_.desktopCodec.sampleRate,
                           config_.mixerFrameDurationMs)) {
        running_ = false;
        return;
    }
    startSources();
    emit logReceived("audio service started");
}

void AudioServiceWorker::stop()
{
    if (!running_) {
        stopSources();
        audioMixer_.stop();
        audioPlayback_.stop();
        return;
    }

    running_ = false;
    stopSources();
    audioMixer_.stop();
    audioPlayback_.stop();
    microphoneEncoder_.close();
    desktopEncoder_.close();
    microphoneDecoder_.close();
    desktopDecoder_.close();
    emit logReceived("audio service stopped");
}

void AudioServiceWorker::setMicrophoneEnabled(bool enabled)
{
    microphoneEnabled_ = enabled;
    if (!running_) {
        return;
    }
    if (enabled) {
        microphoneSource_.start(
                config_.microphoneCodec.sampleRate,
                config_.microphoneCodec.channels,
                config_.microphoneCodec.frameDurationMs);
    } else {
        microphoneSource_.stop();
    }
}

void AudioServiceWorker::setDesktopAudioEnabled(bool enabled)
{
    desktopAudioEnabled_ = enabled;
    if (!running_ || role_ != Role::Host) {
        return;
    }
    if (enabled) {
        const DWORD selfPid = GetCurrentProcessId();
        desktopSource_.start(
                config_.desktopCodec.sampleRate,
                config_.desktopCodec.channels,
                config_.desktopCodec.frameDurationMs,
                WASAPI_CAPTURE_EXCLUDE_PROCESS_TREE,
                selfPid);
    } else {
        desktopSource_.stop();
    }
}

void AudioServiceWorker::setPlaybackEnabled(bool enabled)
{
    playbackEnabled_ = enabled;
    if (!enabled) {
        audioPlayback_.stop();
        audioMixer_.clear();
    }
}

void AudioServiceWorker::onMicrophonePcmFrame(
        const QByteArray& pcm,
        quint64 captureTimeStampMs)
{
    if (!running_ || !microphoneEnabled_ ||
        !ensureMicrophoneEncoder()) {
        return;
    }

    const QByteArray opus = microphoneEncoder_.encodePcm16(pcm);
    if (opus.isEmpty()) {
        return;
    }

    AudioSample sample;
    sample.seq = nextMicrophoneSeq_++;
    sample.captureTimeStampMs = captureTimeStampMs;
    sample.streamKind = AudioStreamKind::Microphone;
    sample.sampleRate = static_cast<quint16>(
            config_.microphoneCodec.sampleRate);
    sample.channels = static_cast<quint8>(
            config_.microphoneCodec.channels);
    sample.frameDurationMs = static_cast<quint16>(
            config_.microphoneCodec.frameDurationMs);
    sample.data = opus;
    emit audioSampleBytesReady(AudioSampleCodec::encode(sample));
}

void AudioServiceWorker::onDesktopPcmFrame(
        const QByteArray& pcm,
        quint64 captureTimeStampMs)
{
    if (!running_ || role_ != Role::Host ||
        !desktopAudioEnabled_ || !ensureDesktopEncoder()) {
        return;
    }

    const QByteArray opus = desktopEncoder_.encodePcm16(pcm);
    if (opus.isEmpty()) {
        return;
    }

    AudioSample sample;
    sample.seq = nextDesktopSeq_++;
    sample.captureTimeStampMs = captureTimeStampMs;
    sample.streamKind = AudioStreamKind::Desktop;
    sample.sampleRate = static_cast<quint16>(
            config_.desktopCodec.sampleRate);
    sample.channels = static_cast<quint8>(
            config_.desktopCodec.channels);
    sample.frameDurationMs = static_cast<quint16>(
            config_.desktopCodec.frameDurationMs);
    sample.data = opus;
    emit audioSampleBytesReady(AudioSampleCodec::encode(sample));
}

void AudioServiceWorker::onAudioSampleBytesReceived(
        const QByteArray& bytes)
{
    AudioSample sample;
    if (!AudioSampleCodec::decode(bytes, &sample) ||
        !ensureDecoder(sample.streamKind)) {
        return;
    }

    AudioOpusDecoder* decoder =
            sample.streamKind == AudioStreamKind::Microphone
            ? &microphoneDecoder_
            : &desktopDecoder_;
    const QByteArray pcm = decoder->decodeToPcm16(sample.data);
    if (pcm.isEmpty()) {
        return;
    }

    const OpusCodecConfig& codecConfig =
            sample.streamKind == AudioStreamKind::Microphone
            ? config_.microphoneCodec
            : config_.desktopCodec;

    emit decodedAudioFrameReady(
            DecodedAudioFrame{
                sample.streamKind,
                sample.captureTimeStampMs,
                codecConfig.sampleRate,
                codecConfig.channels,
                pcm
            });
}

void AudioServiceWorker::onAudioFrameToPlay(
        const DecodedAudioFrame& frame)
{
    if (running_ && playbackEnabled_) {
        audioMixer_.pushFrame(frame);
    }
}

bool AudioServiceWorker::ensureMicrophoneEncoder()
{
    return microphoneEncoder_.isOpen() ||
           microphoneEncoder_.open(
                   config_.microphoneCodec,
                   AudioStreamKind::Microphone);
}

bool AudioServiceWorker::ensureDesktopEncoder()
{
    return desktopEncoder_.isOpen() ||
           desktopEncoder_.open(
                   config_.desktopCodec,
                   AudioStreamKind::Desktop);
}

bool AudioServiceWorker::ensureDecoder(AudioStreamKind kind)
{
    if (kind == AudioStreamKind::Microphone) {
        return microphoneDecoder_.isOpen() ||
               microphoneDecoder_.open(config_.microphoneCodec);
    }
    if (kind == AudioStreamKind::Desktop) {
        return desktopDecoder_.isOpen() ||
               desktopDecoder_.open(config_.desktopCodec);
    }
    return false;
}

void AudioServiceWorker::startSources()
{
    if (microphoneEnabled_) {
        microphoneSource_.start(
                config_.microphoneCodec.sampleRate,
                config_.microphoneCodec.channels,
                config_.microphoneCodec.frameDurationMs);
    }

    if (role_ == Role::Host && desktopAudioEnabled_) {
        const DWORD selfPid = GetCurrentProcessId();
        desktopSource_.start(
                config_.desktopCodec.sampleRate,
                config_.desktopCodec.channels,
                config_.desktopCodec.frameDurationMs,
                WASAPI_CAPTURE_EXCLUDE_PROCESS_TREE,
                selfPid);
    }
}

void AudioServiceWorker::stopSources()
{
    microphoneSource_.stop();
    desktopSource_.stop();
}
```

---

## 九、AudioService

`AudioService` 是线程外壳，所有实际音频操作都通过 `AudioServiceWorker` 完成。

```cpp
// Media/Audio/AudioService.h
#pragma once

#include <QObject>
#include <QThread>

#include "AppConfig.h"
#include "AvSync/AvSyncFrame.h"
#include "AudioServiceWorker.h"

class AudioService : public QObject {
    Q_OBJECT
public:
    explicit AudioService(QObject* parent = nullptr);
    ~AudioService() override;

    AudioServiceWorker* worker() const;

public slots:
    void applyConfig(const AppConfig& config);
    void setRole(Role role);
    void start();
    void stop();
    void setMicrophoneEnabled(bool enabled);
    void setDesktopAudioEnabled(bool enabled);
    void setPlaybackEnabled(bool enabled);

signals:
    void audioSampleBytesReady(const QByteArray& bytes);
    void decodedAudioFrameReady(const DecodedAudioFrame& frame);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    QThread workerThread_;
    AudioServiceWorker* worker_ = nullptr;
};
```

```cpp
// Media/Audio/AudioService.cpp
#include "AudioService.h"

AudioService::AudioService(QObject* parent)
    : QObject(parent),
      worker_(new AudioServiceWorker())
{
    worker_->moveToThread(&workerThread_);

    connect(&workerThread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    connect(worker_, &AudioServiceWorker::logReceived,
            this, &AudioService::logReceived);
    connect(worker_, &AudioServiceWorker::errorOccurred,
            this, &AudioService::errorOccurred);

    workerThread_.start();
}

AudioService::~AudioService()
{
    if (workerThread_.isRunning()) {
        QMetaObject::invokeMethod(
                worker_, "stop", Qt::BlockingQueuedConnection);
        workerThread_.quit();
        workerThread_.wait();
    }
    worker_ = nullptr;
}

AudioServiceWorker* AudioService::worker() const
{
    return worker_;
}

void AudioService::applyConfig(const AppConfig& config)
{
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, config] { worker->applyConfig(config); },
            Qt::QueuedConnection);
}

void AudioService::setRole(Role role)
{
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, role] { worker->setRole(role); },
            Qt::QueuedConnection);
}

void AudioService::start()
{
    QMetaObject::invokeMethod(worker_, "start", Qt::QueuedConnection);
}

void AudioService::stop()
{
    QMetaObject::invokeMethod(worker_, "stop", Qt::QueuedConnection);
}

void AudioService::setMicrophoneEnabled(bool enabled)
{
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, enabled] { worker->setMicrophoneEnabled(enabled); },
            Qt::QueuedConnection);
}

void AudioService::setDesktopAudioEnabled(bool enabled)
{
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, enabled] { worker->setDesktopAudioEnabled(enabled); },
            Qt::QueuedConnection);
}

void AudioService::setPlaybackEnabled(bool enabled)
{
    QMetaObject::invokeMethod(
            worker_,
            [worker = worker_, enabled] { worker->setPlaybackEnabled(enabled); },
            Qt::QueuedConnection);
}
```

---

## 十、AvSyncWorker

`AvSyncWorker` 只负责“什么时候发给播放/渲染”，不负责编码和混音。

```cpp
// Media/AvSync/AvSyncWorker.h
#pragma once

#include <QObject>

#include "Role.h"
#include "AvSyncFrame.h"

class AvSyncWorker : public QObject {
    Q_OBJECT
public:
    explicit AvSyncWorker(QObject* parent = nullptr);

public slots:
    void setRole(Role role);
    void setVideoEnabled(bool enabled);
    void setAudioEnabled(bool enabled);
    void setAvSyncEnabled(bool enabled);

    void onVideoFrameReady(const DecodedVideoFrame& frame);
    void onAudioFrameReady(const DecodedAudioFrame& frame);

signals:
    void videoFrameToRender(const DecodedVideoFrame& frame);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    Role role_ = Role::Unknown;
    bool videoEnabled_ = true;
    bool audioEnabled_ = true;
    bool avSyncEnabled_ = true;
    quint64 lastDesktopAudioPtsMs_ = 0;
    bool desktopClockValid_ = false;
};
```

规则：

```text
videoEnabled = true:
    允许视频帧进入渲染链路

audioEnabled = true:
    允许音频帧进入播放链路

avSyncEnabled = true:
    桌面音频可作为视频同步参考

avSyncEnabled = false:
    视频直接渲染，不做等待或对齐
```

```cpp
// Media/AvSync/AvSyncWorker.cpp
#include "AvSyncWorker.h"

AvSyncWorker::AvSyncWorker(QObject* parent)
    : QObject(parent)
{
}

void AvSyncWorker::setRole(Role role)
{
    role_ = role;
}

void AvSyncWorker::setVideoEnabled(bool enabled)
{
    videoEnabled_ = enabled;
}

void AvSyncWorker::setAudioEnabled(bool enabled)
{
    audioEnabled_ = enabled;
    if (!enabled) {
        desktopClockValid_ = false;
    }
}

void AvSyncWorker::setAvSyncEnabled(bool enabled)
{
    avSyncEnabled_ = enabled;
    if (!enabled) {
        desktopClockValid_ = false;
    }
}

void AvSyncWorker::onAudioFrameReady(
        const DecodedAudioFrame& frame)
{
    if (!audioEnabled_) {
        return;
    }

    if (frame.streamKind == AudioStreamKind::Desktop) {
        lastDesktopAudioPtsMs_ = frame.ptsMs;
        desktopClockValid_ = true;
    }

    emit audioFrameToPlay(frame);
}

void AvSyncWorker::onVideoFrameReady(
        const DecodedVideoFrame& frame)
{
    if (!videoEnabled_) {
        return;
    }

    if (avSyncEnabled_ && desktopClockValid_) {
        const qint64 deltaMs =
                static_cast<qint64>(frame.ptsMs) -
                static_cast<qint64>(lastDesktopAudioPtsMs_);
        if (deltaMs < -80 || deltaMs > 40) {
            return;
        }
    }

    emit videoFrameToRender(frame);
}
```

### 10.1 AvSyncService

```cpp
// Media/AvSync/AvSyncService.h
#pragma once

#include <QObject>
#include <QThread>

#include "AvSyncWorker.h"

class AvSyncService : public QObject {
    Q_OBJECT
public:
    explicit AvSyncService(QObject* parent = nullptr);
    ~AvSyncService() override;

    AvSyncWorker* worker() const;

public slots:
    void setRole(Role role);
    void setVideoEnabled(bool enabled);
    void setAudioEnabled(bool enabled);
    void setAvSyncEnabled(bool enabled);

signals:
    void videoFrameToRender(const DecodedVideoFrame& frame);
    void audioFrameToPlay(const DecodedAudioFrame& frame);
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    QThread workerThread_;
    AvSyncWorker* worker_ = nullptr;
};
```

```cpp
// Media/AvSync/AvSyncService.cpp
#include "AvSyncService.h"

AvSyncService::AvSyncService(QObject* parent)
    : QObject(parent),
      worker_(new AvSyncWorker())
{
    worker_->moveToThread(&workerThread_);

    connect(&workerThread_, &QThread::finished,
            worker_, &QObject::deleteLater);
    connect(worker_, &AvSyncWorker::videoFrameToRender,
            this, &AvSyncService::videoFrameToRender);
    connect(worker_, &AvSyncWorker::logReceived,
            this, &AvSyncService::logReceived);
    connect(worker_, &AvSyncWorker::errorOccurred,
            this, &AvSyncService::errorOccurred);

    workerThread_.start();
}

AvSyncService::~AvSyncService()
{
    if (workerThread_.isRunning()) {
        workerThread_.quit();
        workerThread_.wait();
    }
    worker_ = nullptr;
}

AvSyncWorker* AvSyncService::worker() const
{
    return worker_;
}

void AvSyncService::setRole(Role role)
{
    QMetaObject::invokeMethod(
            worker_,
            [this, role] { worker_->setRole(role); },
            Qt::QueuedConnection);
}

void AvSyncService::setVideoEnabled(bool enabled)
{
    QMetaObject::invokeMethod(
            worker_,
            [this, enabled] { worker_->setVideoEnabled(enabled); },
            Qt::QueuedConnection);
}

void AvSyncService::setAudioEnabled(bool enabled)
{
    QMetaObject::invokeMethod(
            worker_,
            [this, enabled] { worker_->setAudioEnabled(enabled); },
            Qt::QueuedConnection);
}

void AvSyncService::setAvSyncEnabled(bool enabled)
{
    QMetaObject::invokeMethod(
            worker_,
            [this, enabled] { worker_->setAvSyncEnabled(enabled); },
            Qt::QueuedConnection);
}
```

---

## 十一、ClientApp 连接

`ClientApp` 只负责持有服务、传递配置、设置 role 和建立连接。音频数据不能通过
`AudioService` 或 `AvSyncService` 外壳转发，因为两个外壳对象属于主线程。

### 11.1 `ClientApp.h`

```cpp
#include "Audio/AudioService.h"
#include "AvSync/AvSyncService.h"
```

```cpp
AudioService audioService_;
AvSyncService avSyncService_;
```

构造函数初始化：

```cpp
ClientApp::ClientApp()
    : signalingClient_(this),
      dispatcher_(this),
      p2pSession_(this),
      mediaService_(this),
      audioService_(this),
      avSyncService_(this),
      controlService_(this)
{
    qRegisterMetaType<DecodedAudioFrame>("DecodedAudioFrame");
    qRegisterMetaType<DecodedVideoFrame>("DecodedVideoFrame");
}
```

`DecodedAudioFrame` 会跨 `AudioServiceWorker`、`AvSyncWorker` 和
`AudioServiceWorker` 的线程边界传递，因此必须在建立连接前注册元类型。

### 11.2 配置和 role

Host 启动时：

```cpp
audioService_.applyConfig(config);
audioService_.setRole(Role::Host);

avSyncService_.setRole(Role::Host);
avSyncService_.setAudioEnabled(config.audio.playbackEnabled);
avSyncService_.setVideoEnabled(true);
avSyncService_.setAvSyncEnabled(config.audio.avSyncEnabled);
```

Guest 启动时：

```cpp
audioService_.applyConfig(config);
audioService_.setRole(Role::Guest);

avSyncService_.setRole(Role::Guest);
avSyncService_.setAudioEnabled(config.audio.playbackEnabled);
avSyncService_.setVideoEnabled(true);
avSyncService_.setAvSyncEnabled(config.audio.avSyncEnabled);
```

### 11.3 音频数据直连

下面四条连接必须使用 worker 对象：

```cpp
connect(audioService_.worker(),
        &AudioServiceWorker::audioSampleBytesReady,
        mediaService_.worker(),
        &MediaServiceWorker::sendAudioSampleBytes);

connect(mediaService_.worker(),
        &MediaServiceWorker::audioSampleBytesReceived,
        audioService_.worker(),
        &AudioServiceWorker::onAudioSampleBytesReceived);

connect(audioService_.worker(),
        &AudioServiceWorker::decodedAudioFrameReady,
        avSyncService_.worker(),
        &AvSyncWorker::onAudioFrameReady);

connect(avSyncService_.worker(),
        &AvSyncWorker::audioFrameToPlay,
        audioService_.worker(),
        &AudioServiceWorker::onAudioFrameToPlay);
```

连接后的线程关系：

```text
AudioServiceWorker
    -> MediaServiceWorker
    -> AudioServiceWorker
    -> AvSyncWorker
    -> AudioServiceWorker
```

每次跨线程传递使用 Qt queued connection，但主线程不参与音频数据传输。

### 11.4 P2P 建立后启动

```cpp
connect(p2pSession_.worker(),
        &P2pSessionWorker::p2pReady,
        audioService_.worker(),
        &AudioServiceWorker::start);
```

### 11.5 只把日志和错误送到主线程

```cpp
connect(&audioService_,
        &AudioService::logReceived,
        this,
        &ClientApp::logReceived);

connect(&audioService_,
        &AudioService::errorOccurred,
        this,
        &ClientApp::errorOccurred);

connect(&avSyncService_,
        &AvSyncService::logReceived,
        this,
        &ClientApp::logReceived);

connect(&avSyncService_,
        &AvSyncService::errorOccurred,
        this,
        &ClientApp::errorOccurred);
```

以下数据连接不能写成外壳连接：

```cpp
// 不允许
connect(&audioService_, ...);
connect(&avSyncService_, ...);
```

主线程只接收日志、错误和最终的视频渲染请求。音频 PCM、Opus 数据和音频帧都留在
worker 链路中。

---

## 十二、CMake

### 10.1 Media CMake

```cmake
find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Network Multimedia)

find_path(OPUS_INCLUDE_DIR
        NAMES opus/opus.h
        PATHS "${OPUS_ROOT}/include"
        NO_DEFAULT_PATH)

find_library(OPUS_LIBRARY
        NAMES libopus.dll.a opus libopus
        PATHS "${OPUS_ROOT}/lib"
        NO_DEFAULT_PATH)

target_include_directories(Media PUBLIC
        "${OPUS_INCLUDE_DIR}")

target_link_libraries(Media PUBLIC
        Qt5::Multimedia
        "${OPUS_LIBRARY}"
        ole32
        mmdevapi
        avrt)
```

### 10.2 wasapi_dll

如果项目引入 `wasapi_dll`，桌面音频采集建议直接使用它的 C 接口。

运行时加载：

```text
LoadLibrary
GetProcAddress
wasapi_capture_start
wasapi_capture_read
```

---

## 十三、接入顺序

建议按这个顺序改：

1. 先把 `AudioSample` 改成纯 Opus 载体，删掉编码方式字段
2. 把 `AudioSampleCodec` 调整为只序列化 Opus 音频头
3. 调整 `AudioOpusEncoder / Decoder` 为配置驱动
4. 把 `DesktopAudioSource` 改成默认本进程过滤
5. 把 `AudioServiceWorker` 接上 `AudioMixer`
6. 把 `AvSyncWorker` 接上解码后的音视频帧
7. 把 `VideoSample` 改成 H264-only，不再出现 jpeg

---

## 十四、边界要求

1. 音频样本不再携带 codec type。
2. 桌面音频默认过滤当前 P2Pplay 进程树。
3. 麦克风和桌面音频都使用 Opus。
4. 视频样本不再使用 jpeg。
5. 编码器只接受配置对象，不在内部硬编码参数。
6. `AudioServiceWorker` 和 `AvSyncWorker` 都必须在自己的线程里工作。
7. `AudioPlayback` 只负责播放，不参与混音决策。
