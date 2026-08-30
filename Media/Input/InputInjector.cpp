//
// Created by ASUS on 2026/8/13.
//

#include <QDebug>
#include <windows.h>
#include "InputInjector.h"
#include "InputCoordinate.h"

void InputInjector::sendKeyDown(quint32 vk) {
    qDebug() << "inject keydown" << vk;
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    // dwFlags = 0 代表按键按下
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::sendKeyUp(quint32 vk) {
    qDebug() << "inject keyup" << vk;
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = KEYEVENTF_KEYUP; // 标记松开
    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::sendMouseMove(qint32 normalizedX, qint32 normalizedY) {
    normalizedX = InputCoordinate::clamp(normalizedX);
    normalizedY = InputCoordinate::clamp(normalizedY);

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(normalizedX);
    input.mi.dy = static_cast<LONG>(normalizedY);
    input.mi.dwFlags =
            MOUSEEVENTF_ABSOLUTE |
            MOUSEEVENTF_VIRTUALDESK |
            MOUSEEVENTF_MOVE;

    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::sendMouseDown(InputMouseButton button) {
    INPUT input{};
    input.type = INPUT_MOUSE;

    switch (button) {
        case InputMouseButton::Left:
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            break;
        case InputMouseButton::Right:
            input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            break;
        case InputMouseButton::Middle:
            input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
            break;
        default:
            return;
    }

    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::sendMouseUp(InputMouseButton button) {
    INPUT input{};
    input.type = INPUT_MOUSE;

    switch (button) {
        case InputMouseButton::Left:
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            break;
        case InputMouseButton::Right:
            input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            break;
        case InputMouseButton::Middle:
            input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            break;
        default:
            return;
    }

    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::sendMouseWheel(qint32 delta) {
    if (delta == 0) {
        return;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.mouseData = delta;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;

    SendInput(1, &input, sizeof(INPUT));
}