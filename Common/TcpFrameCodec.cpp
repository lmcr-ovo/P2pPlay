//
// Created by ASUS on 2026/7/26.
//

#include "TcpFrameCodec.h"
#include <QDataStream>

QByteArray TcpFrameCodec::encode(const TcpFrame& frame) {
    QByteArray data;
    data.reserve(HeaderSize + frame.payload.size());

    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << static_cast<quint32>(Magic);
    stream << static_cast<quint16>(Version);
    stream << static_cast<quint16>(frame.type);
    stream << static_cast<quint32>(frame.payload.size());

    data.append(frame.payload);
    return data;
}

bool TcpFrameCodec::tryDecode(QByteArray& buffer, TcpFrame& frame) {
    if (buffer.size() < HeaderSize) {
        return false;
    }

    QDataStream stream(buffer.left(HeaderSize));
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    quint16 version = 0;
    quint16 type = 0;
    quint32 length = 0;

    stream >> magic >> version >> type >> length;

    if (magic != Magic || version != Version || length > MaxPayloadSize) {
        buffer.clear();
        return false;
    }

    const int frameSize = HeaderSize + static_cast<int>(length);
    if (buffer.size() < frameSize) {
        return false;
    }

    frame.type = static_cast<SignalingType>(type);
    frame.payload = buffer.mid(HeaderSize, length);
    buffer.remove(0, frameSize);
    return true;
}
