//
// Created by ASUS on 2026/7/26.
//

#ifndef P2PPLAY_TCPFRAMECODEC_H
#define P2PPLAY_TCPFRAMECODEC_H


#include <QByteArray>
#include "TcpFrame.h"

class TcpFrameCodec {
public:
    static QByteArray encode(const TcpFrame& frame);
    static bool tryDecode(QByteArray& buffer, TcpFrame& frame);

private:
    static const quint32 Magic = 0x50325043;
    static const quint16 Version = 1;
    static const int HeaderSize = 12;
    static const quint32 MaxPayloadSize = 1024 * 1024;
};


#endif //P2PPLAY_TCPFRAMECODEC_H
