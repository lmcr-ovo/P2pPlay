//
// Created by ASUS on 2026/7/26.
//

#ifndef P2PPLAY_SIGNALINGCODEC_H
#define P2PPLAY_SIGNALINGCODEC_H

#include <QtCore>
#include "SignalingMessage.h"

class SignalingCodec {
public:
    static QByteArray encodePayload(const SignalingMessage& message);
    static bool decodePayload(SignalingType type, const QByteArray& payload,
                                SignalingMessage& message);
};


#endif //P2PPLAY_SIGNALINGCODEC_H
