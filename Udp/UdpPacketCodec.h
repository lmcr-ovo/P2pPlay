//
// Created by ASUS on 2026/7/27.
//

#ifndef P2PPLAY_UDPPACKETCODEC_H
#define P2PPLAY_UDPPACKETCODEC_H

#include <QtCore>
#include "UdpPacket.h"

class UdpPacketCodec {
public:
    static QByteArray encode(const UdpPacket& packet);
    static bool decode(const QByteArray& bytes, UdpPacket& packet);
};


#endif //P2PPLAY_UDPPACKETCODEC_H
