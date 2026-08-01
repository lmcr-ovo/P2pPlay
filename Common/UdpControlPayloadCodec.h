//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_UDPCONTROLPAYLOADCODEC_H
#define P2PPLAY_UDPCONTROLPAYLOADCODEC_H

#include <QByteArray>
#include "UdpControlPayload.h"

class UdpControlPayloadCodec {
public:
    static QByteArray encodeProbe(const UdpProbePayload& payload);
    static bool decodeProbe(const QByteArray& bytes, UdpProbePayload* payload);

    static QByteArray encodeProbeAck(const UdpProbeAckPayload& payload);
    static bool decodeProbeAck(const QByteArray& bytes, UdpProbeAckPayload* payload);

    static QByteArray encodePunch(const UdpPunchPayload& payload);
    static bool decodePunch(const QByteArray& bytes, UdpPunchPayload* payload);

    static QByteArray encodePunchAck(const UdpPunchAckPayload& payload);
    static bool decodePunchAck(const QByteArray& bytes, UdpPunchAckPayload* payload);
};


#endif //P2PPLAY_UDPCONTROLPAYLOADCODEC_H
