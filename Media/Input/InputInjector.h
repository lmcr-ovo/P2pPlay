//
// Created by ASUS on 2026/8/13.
//

#ifndef P2PPLAY_INPUTINJECTOR_H
#define P2PPLAY_INPUTINJECTOR_H

#include <QtCore>
#include "InputSample.h"
class InputInjector {
public:
    static void sendKeyDown(quint32 vk);
    static void sendKeyUp(quint32 vk);

    // 参数为 0~65535 的归一化坐标
    static void sendMouseMove(qint32 normalizedX,qint32 normalizedY);
    static void sendMouseDown(InputMouseButton button);
    static void sendMouseUp(InputMouseButton button);
    static void sendMouseWheel(qint32 delta);
};


#endif //P2PPLAY_INPUTINJECTOR_H
