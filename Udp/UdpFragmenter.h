//
// Created by ASUS on 2026/7/28.
//

#ifndef P2PPLAY_UDPFRAGMENTER_H
#define P2PPLAY_UDPFRAGMENTER_H

#include <QList>
#include "UdpPacket.h"

class UdpFragmenter {
public:
    static QList<UdpPacket> fragment(const UdpFrame& frame, quint32 frameSeq);
};


#endif //P2PPLAY_UDPFRAGMENTER_H
