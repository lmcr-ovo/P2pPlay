//
// Created by ASUS on 2026/7/27.
//

#ifndef P2PPLAY_UDPPACKETCODEC_H
#define P2PPLAY_UDPPACKETCODEC_H

#include <QtCore>
#include "UdpPacket.h"

class UdpPacketCodec {
public:
    static QByteArray encode(UdpPacket& packet);
    static bool decode(const QByteArray& bytes, UdpPacket& packet);

private:
    static const quint16 MTU = 1200;
};


#endif //P2PPLAY_UDPPACKETCODEC_H
