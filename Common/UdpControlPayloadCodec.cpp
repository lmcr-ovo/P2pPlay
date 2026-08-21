//
// Created by ASUS on 2026/8/1.
//

#include "UdpControlPayloadCodec.h"

#include <QDataStream>

namespace {

    QDataStream::ByteOrder payloadByteOrder()
    {
        return QDataStream::BigEndian;
    }

    QDataStream::Version payloadStreamVersion()
    {
        return QDataStream::Qt_5_14;
    }

    bool streamOk(const QDataStream& stream)
    {
        return stream.status() == QDataStream::Ok;
    }

}

QByteArray UdpControlPayloadCodec::encodeProbe(const UdpProbePayload& payload)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(payloadByteOrder());
    out.setVersion(payloadStreamVersion());

    out << payload.roomId
        << payload.clientId;

    return bytes;
}

bool UdpControlPayloadCodec::decodeProbe(const QByteArray& bytes, UdpProbePayload* payload)
{
    if (payload == nullptr) {
        return false;
    }

    QDataStream in(bytes);
    in.setByteOrder(payloadByteOrder());
    in.setVersion(payloadStreamVersion());

    UdpProbePayload result;
    in >> result.roomId
       >> result.clientId;

    if (!streamOk(in)) {
        return false;
    }

    *payload = result;
    return true;
}

QByteArray UdpControlPayloadCodec::encodeProbeAck(const UdpProbeAckPayload& payload)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(payloadByteOrder());
    out.setVersion(payloadStreamVersion());

    out << payload.roomId
        << payload.clientId
        << payload.success
        << payload.reason;

    return bytes;
}

bool UdpControlPayloadCodec::decodeProbeAck(const QByteArray& bytes, UdpProbeAckPayload* payload)
{
    if (payload == nullptr) {
        return false;
    }

    QDataStream in(bytes);
    in.setByteOrder(payloadByteOrder());
    in.setVersion(payloadStreamVersion());

    UdpProbeAckPayload result;
    in >> result.roomId
       >> result.clientId
       >> result.success
       >> result.reason;

    if (!streamOk(in)) {
        return false;
    }

    *payload = result;
    return true;
}

QByteArray UdpControlPayloadCodec::encodePunch(const UdpPunchPayload& payload)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(payloadByteOrder());
    out.setVersion(payloadStreamVersion());

    out << payload.roomId
        << payload.clientId;

    return bytes;
}

bool UdpControlPayloadCodec::decodePunch(const QByteArray& bytes, UdpPunchPayload* payload)
{
    if (payload == nullptr) {
        return false;
    }

    QDataStream in(bytes);
    in.setByteOrder(payloadByteOrder());
    in.setVersion(payloadStreamVersion());

    UdpPunchPayload result;
    in >> result.roomId
       >> result.clientId;

    if (!streamOk(in)) {
        return false;
    }

    *payload = result;
    return true;
}

QByteArray UdpControlPayloadCodec::encodePunchAck(const UdpPunchAckPayload& payload)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(payloadByteOrder());
    out.setVersion(payloadStreamVersion());

    out << payload.roomId
        << payload.clientId;

    return bytes;
}

bool UdpControlPayloadCodec::decodePunchAck(const QByteArray& bytes, UdpPunchAckPayload* payload)
{
    if (payload == nullptr) {
        return false;
    }

    QDataStream in(bytes);
    in.setByteOrder(payloadByteOrder());
    in.setVersion(payloadStreamVersion());

    UdpPunchAckPayload result;
    in >> result.roomId
       >> result.clientId;

    if (!streamOk(in)) {
        return false;
    }

    *payload = result;
    return true;
}

