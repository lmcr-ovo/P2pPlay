//
// Created by ASUS on 2026/8/12.
//

#ifndef P2PPLAY_INPUTSAMPLECODEC_H
#define P2PPLAY_INPUTSAMPLECODEC_H

#include <QByteArray>
#include "InputSample.h"

class InputSampleCodec {
public:
    static QByteArray encode(const InputSample& sample);
    static bool decode(const QByteArray& bytes, InputSample& sample);
};


#endif //P2PPLAY_INPUTSAMPLECODEC_H
