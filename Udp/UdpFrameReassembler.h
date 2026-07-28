//
// Created by ASUS on 2026/7/27.
//

#ifndef P2PPLAY_UDPFRAMEREASSEMBLER_H
#define P2PPLAY_UDPFRAMEREASSEMBLER_H

#include <QtCore>
#include <QObject>
#include "UdpPacket.h"

class UdpFrameReassembler : public QObject {
    Q_OBJECT

public:
    explicit UdpFrameReassembler(QObject* parent);
    void pushPacket(const UdpPacket& packet);

signals:
    void frameReady(const UdpFrame& frame);
    void frameDropped(quint32 frameSeq);

private:
    struct FrameBuffer {
        UdpChannelType channelType = UdpChannelType::Unknown;
        UdpFrameType frameType = UdpFrameType::Unknown;
        quint16 fragmentCount = 0;
        QMap<quint16, QByteArray> fragments;

        bool isComplete() const {
            return fragmentCount > 0 && fragmentCount == fragments.size();
        }
    };

private:
    void emitCompleteFrame(const FrameBuffer& buffer);
    void dropFramesOlderThan(quint32 frameSeq);
private:
    QMap<quint32, FrameBuffer> pendingFrame_;
};


#endif //P2PPLAY_UDPFRAMEREASSEMBLER_H
