//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_UDPCONTROLPAYLOAD_H
#define P2PPLAY_UDPCONTROLPAYLOAD_H

#include <QString>

struct UdpProbePayload {
    QString roomId;
    QString clientId;
};

struct UdpProbeAckPayload {
    QString roomId;
    QString clientId;
    bool success = false;
    QString reason;
};

struct UdpPunchPayload {
    QString roomId;
    QString clientId;
};

struct UdpPunchAckPayload {
    QString roomId;
    QString clientId;
};


#endif //P2PPLAY_UDPCONTROLPAYLOAD_H
