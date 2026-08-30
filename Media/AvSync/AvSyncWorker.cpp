//
// Created by ASUS on 2026/8/29.
//

#include "AvSyncWorker.h"

AvSyncWorker::AvSyncWorker(QObject* parent)
        : QObject(parent) {
}

void AvSyncWorker::setRole(Role role) {
    role_ = role;
}

void AvSyncWorker::setVideoEnabled(bool enabled) {
    videoEnabled_ = enabled;
}

void AvSyncWorker::setAudioEnabled(bool enabled) {
    audioEnabled_ = enabled;
    if (!enabled) {
        desktopClockValid_ = false;
    }
}

void AvSyncWorker::setAvSyncEnabled(bool enabled) {
    avSyncEnabled_ = enabled;
    if (!enabled) {
        desktopClockValid_ = false;
    }
}

void AvSyncWorker::onAudioFrameReady(
        const DecodedAudioFrame& frame) {
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
        const DecodedVideoFrame& frame) {
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