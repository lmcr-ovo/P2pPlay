//
// Created by ASUS on 2026/8/29.
//

#ifndef P2PPLAY_DESKTOPAUDIOSOURCE_H
#define P2PPLAY_DESKTOPAUDIOSOURCE_H


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

#endif //P2PPLAY_DESKTOPAUDIOSOURCE_H
