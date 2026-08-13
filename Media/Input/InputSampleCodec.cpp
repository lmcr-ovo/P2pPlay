//
// Created by ASUS on 2026/8/12.
//

#include "InputSampleCodec.h"
/*
struct InputSample {
    InputSampleKind kind = InputSampleKind::Unknown;
    quint32 seq = 0;
    quint32 ackSeq = 0;

    InputDevice device = InputDevice::Unknown;
    InputAction action = InputAction::Unknown;

    quint32 vk = 0;

    quint32 x = 0;
    quint32 y = 0;
    quint32 wheelDelta = 0;
    InputMouseButton mouseButton;

    quint64 timeStampMs = 0;
};
 */
QByteArray InputSampleCodec::encode(const InputSample& sample) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);


    stream << static_cast<quint8>(sample.kind)
            << sample.seq
            << sample.ackSeq
            << static_cast<quint8>(sample.device)
            << static_cast<quint8>(sample.action)
            << sample.vk
            << sample.x
            << sample.y
            << sample.wheelDelta
            << static_cast<quint8>(sample.mouseButton)
            << sample.timeStampMs;
    return bytes;
}

bool InputSampleCodec::decode(const QByteArray& bytes, InputSample& sample) {
    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::BigEndian);

    quint8 kind = 0;
    quint32 seq = 0;
    quint32 ackSeq = 0;
    quint8 device = 0;
    quint8 action = 0;
    qint32 vk = 0;
    qint32 x = 0;
    qint32 y = 0;
    qint32 wheelDelta = 0;
    quint8 mouseButton = 0;
    quint64 timeStampMs = 0;

    stream  >> kind
            >> seq
            >> ackSeq
            >> device
            >> action
            >> vk
            >> x
            >> y
            >> wheelDelta
            >> mouseButton
            >> timeStampMs;

    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    sample.kind = static_cast<InputSampleKind>(kind);
    sample.seq = seq;
    sample.ackSeq = ackSeq;
    sample.device = static_cast<InputDevice>(device);
    sample.action = static_cast<InputAction>(action);
    sample.vk = vk;
    sample.x = x;
    sample.y = y;
    sample.wheelDelta = wheelDelta;
    sample.mouseButton = static_cast<InputMouseButton>(mouseButton);
    sample.timeStampMs = timeStampMs;

    return true;
}