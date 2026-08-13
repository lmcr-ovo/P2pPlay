//
// Created by ASUS on 2026/8/13.
//

#include <QDebug>
#include <windows.h>
#include "InputInjector.h"

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