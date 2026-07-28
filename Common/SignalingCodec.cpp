//
// Created by ASUS on 2026/7/26.
//

#include "SignalingCodec.h"

QByteArray SignalingCodec::encodePayload(const SignalingMessage& message) {
    QByteArray data;

    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << static_cast<quint16>(message.type);
    stream << message.roomId;
    stream << message.clientId;
    stream << message.reason;
    stream << message.endpointAddress;
    stream << message.endpointPort;
    stream << message.success;

    return data;
}

bool SignalingCodec::decodePayload(SignalingType type, const QByteArray& payload,
                   SignalingMessage& message) {
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 payloadType = 0;
    stream >> payloadType
           >> message.roomId
           >> message.clientId
           >> message.reason
           >> message.endpointAddress
           >> message.endpointPort
           >> message.success;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }

    message.type = static_cast<SignalingType>(payloadType);
    return message.type == type;
}
