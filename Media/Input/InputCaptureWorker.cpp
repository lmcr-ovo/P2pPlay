//
// Created by ASUS on 2026/8/14.
//
#include "InputCaptureWorker.h"

#include <QDateTime>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
    InputCaptureWorker* activeCapture = nullptr;

    LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
        if (code == HC_ACTION && activeCapture != nullptr) {
            const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

            if ((info->flags & LLKHF_INJECTED) != 0) {
                return CallNextHookEx(nullptr, code, wParam, lParam);
            }

            const bool pressed =
                    wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;

            const bool released =
                    wParam == WM_KEYUP || wParam == WM_SYSKEYUP;

            if (pressed || released) {
                if (activeCapture->handleKeyboardEvent(info->vkCode, pressed)) {
                    return 1;
                }
            }
        }

        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
}

InputCaptureWorker::InputCaptureWorker(QObject* parent)
        : QObject(parent) {
    // Client uses the upper-row controls; Host receives the lower-row keys.
    setKeyMapping('A', VK_LEFT);
    setKeyMapping('D', VK_RIGHT);
    setKeyMapping('W', VK_UP);
    setKeyMapping('S', VK_DOWN);

    // 攻击、跳跃、无双、法宝 -> Host numeric keypad
    setKeyMapping('J', VK_NUMPAD1);
    setKeyMapping('K', VK_NUMPAD2);
    setKeyMapping(VK_SPACE, VK_NUMPAD0);
    setKeyMapping('H', VK_NUMPAD7);

    // 技能 -> Host numeric keypad
    setKeyMapping('U', VK_NUMPAD3);
    setKeyMapping('I', VK_NUMPAD4);
    setKeyMapping('O', VK_NUMPAD5);
    setKeyMapping('Y', VK_NUMPAD6);
    setKeyMapping('L', VK_NUMPAD8);

    // 召唤坐骑 -> Host numeric keypad
    setKeyMapping('Z', VK_NUMPAD9);
}

InputCaptureWorker::~InputCaptureWorker() {
    stop();
}

bool InputCaptureWorker::start() {
    if (keyboardHook_ != nullptr) {
        return true;
    }

    activeCapture = this;

    keyboardHook_ = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            keyboardHookProc,
            GetModuleHandleW(nullptr),
            0);

    if (keyboardHook_ == nullptr) {
        if (activeCapture == this) {
            activeCapture = nullptr;
        }

        emit errorOccurred("failed to install keyboard hook");
        return false;
    }

    emit logReceived("input capture started");
    return true;
}

void InputCaptureWorker::stop() {
    if (keyboardHook_ != nullptr) {
        UnhookWindowsHookEx(static_cast<HHOOK>(keyboardHook_));
        keyboardHook_ = nullptr;
    }

    if (activeCapture == this) {
        activeCapture = nullptr;
    }

    pressedKeys_.clear();
    activeMappings_.clear();

    emit logReceived("input capture stopped");
}

void InputCaptureWorker::setControlActive(bool active) {
    controlActive_ = active;

    if (!controlActive_) {
        pressedKeys_.clear();
        activeMappings_.clear();
    }
}

void InputCaptureWorker::setKeyMapping(quint32 fromVk, quint32 toVk) {
    keyMappings_.insert(fromVk, toVk);
}

void InputCaptureWorker::clearKeyMappings() {
    keyMappings_.clear();
    activeMappings_.clear();
}

bool InputCaptureWorker::handleKeyboardEvent(quint32 vk, bool pressed) {
    if (!controlActive_) {
        return false;
    }

    if (vk == VK_ESCAPE && pressed) {
        setControlActive(false);
        return true;
    }

    if (!controlActive_) {
        return false;
    }

    if (pressed) {
        if (pressedKeys_.contains(vk)) {
            return blockLocalInput_;
        }

        pressedKeys_.insert(vk);
    } else {
        if (!pressedKeys_.remove(vk)) {
            return blockLocalInput_;
        }
    }

    const quint32 mappedVk = mapKey(vk, pressed);

    InputSample sample;
    sample.kind = InputSampleKind::Request;
    sample.device = InputDevice::Keyboard;
    sample.action = pressed ? InputAction::KeyDown : InputAction::KeyUp;
    sample.vk = mappedVk;
    sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();

    emit inputRawSampleReady(sample);

    return blockLocalInput_;
}

quint32 InputCaptureWorker::mapKey(quint32 vk, bool pressed) {
    if (pressed) {
        const quint32 mappedVk = keyMappings_.value(vk, vk);
        activeMappings_.insert(vk, mappedVk);
        return mappedVk;
    }

    const quint32 mappedVk = activeMappings_.take(vk);
    if (mappedVk != 0) {
        return mappedVk;
    }

    return keyMappings_.value(vk, vk);
}