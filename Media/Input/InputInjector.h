//
// Created by ASUS on 2026/8/13.
//

#ifndef P2PPLAY_INPUTINJECTOR_H
#define P2PPLAY_INPUTINJECTOR_H

#include <QtCore>

class InputInjector {
public:
    static void sendKeyDown(quint32 vk);
    static void sendKeyUp(quint32 vk);
};


#endif //P2PPLAY_INPUTINJECTOR_H
