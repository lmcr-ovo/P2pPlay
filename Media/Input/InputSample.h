//
// Created by ASUS on 2026/8/12.
//

#ifndef P2PPLAY_INPUTSAMPLE_H
#define P2PPLAY_INPUTSAMPLE_H

#include <QtCore>

enum class InputSampleKind : quint8 {
    Unknown = 0,
    Request = 1,
    Ack = 2
};

enum class InputDevice : quint8 {
    Unknown = 0,
    Keyboard = 1,
    Mouse = 2
};

enum class InputAction : quint8 {
    Unknown = 0,
    KeyDown = 1,
    KeyUp = 2,

    MouseMove = 3,
    MouseUp = 4,
    MouseDown = 5,
    MouseWheel = 6,
};

enum class InputMouseButton : quint8 {
    Unknown = 0,
    Left = 1,
    Right = 2,
    Middle = 3,
    XButton1 = 4,
    XButton2 = 5
};

inline InputMouseButton mapMouseButtonFromQt(const Qt::MouseButton& button) {
    InputMouseButton result = InputMouseButton::Unknown;
    switch (button) {
        case Qt::LeftButton:  result = InputMouseButton::Left; break;
        case Qt::RightButton: result = InputMouseButton::Right; break;
        case Qt::MiddleButton: result = InputMouseButton::Middle; break;
        default: result = InputMouseButton::Unknown;
    }
    return result;
}

inline bool isReliableInputAction(InputAction action) {
    switch (action) {
        case InputAction::KeyDown:
        case InputAction::KeyUp:
        case InputAction::MouseDown:
        case InputAction::MouseUp:
        case InputAction::MouseWheel:
            return true;

        case InputAction::MouseMove:
            return false;
    }
    return true;
}

struct InputSample {
    InputSampleKind kind = InputSampleKind::Unknown;
    quint32 seq = 0;
    quint32 ackSeq = 0;

    InputDevice device = InputDevice::Unknown;
    InputAction action = InputAction::Unknown;

    quint32 vk = 0;

    // 鼠标坐标：归一化值，范围 0~65535
    qint32 x = 0;
    qint32 y = 0;
    qint32 wheelDelta = 0;
    InputMouseButton mouseButton = InputMouseButton::Unknown;

    quint64 timeStampMs = 0;
};

Q_DECLARE_METATYPE(InputSample)

#endif //P2PPLAY_INPUTSAMPLE_H
