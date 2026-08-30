//
// Created by ASUS on 2026/8/29.
//

#include "DesktopAudioSource.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QLibrary>

DesktopAudioSource::DesktopAudioSource(QObject* parent)
        : QObject(parent),
          pollTimer_(this) {
    pollTimer_.setTimerType(Qt::PreciseTimer);
    connect(&pollTimer_, &QTimer::timeout,
            this, &DesktopAudioSource::poll);
}

DesktopAudioSource::~DesktopAudioSource() {
    stop();
}

bool DesktopAudioSource::start(int sampleRate,
                               int channels,
                               int frameDurationMs,
                               WasapiCaptureMode captureMode,
                               DWORD targetProcessId) {
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

void DesktopAudioSource::stop() {
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

bool DesktopAudioSource::isRunning() const {
    return running_;
}

bool DesktopAudioSource::loadLibrary() {
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

void DesktopAudioSource::unloadLibrary() {
    create_ = nullptr;
    destroy_ = nullptr;
    startCapture_ = nullptr;
    stopCapture_ = nullptr;
    readCapture_ = nullptr;
    if (wasapiLibrary_.isLoaded()) {
        wasapiLibrary_.unload();
    }
}

void DesktopAudioSource::poll() {
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

void DesktopAudioSource::appendPcm(const QByteArray& pcm) {
    if (pcm.isEmpty() || frameBytes_ <= 0) {
        return;
    }

    pendingPcm_.append(pcm);
    emitCompleteFrames();
}

void DesktopAudioSource::emitCompleteFrames() {
    while (frameBytes_ > 0 && pendingPcm_.size() >= frameBytes_) {
        const QByteArray frame = pendingPcm_.left(frameBytes_);
        pendingPcm_.remove(0, frameBytes_);
        emit pcmFrameReady(
                frame,
                static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
    }
}
