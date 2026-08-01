//
// Created by ASUS on 2026/7/27.
//

#ifndef P2PPLAY_UDPFRAMEREASSEMBLER_H
#define P2PPLAY_UDPFRAMEREASSEMBLER_H

#include <QtCore>
#include <QObject>
#include <QHostAddress>
#include "UdpPacket.h"

struct FrameKey {
    QHostAddress senderAddress;
    quint16 senderPort = 0;
    quint32 frameSeq = 0;

    bool operator<(const FrameKey& other) const {
        const QString selfAddress = senderAddress.toString();
        const QString otherAddress = other.senderAddress.toString();

        if (selfAddress != otherAddress) {
            return selfAddress < otherAddress;
        }

        if (senderPort != other.senderPort) {
            return senderPort < other.senderPort;
        }

        return frameSeq < other.frameSeq;
    }
};

class UdpFrameReassembler : public QObject {
    Q_OBJECT

public:
    explicit UdpFrameReassembler(QObject* parent = nullptr);
    void pushPacket(const UdpPacket& packet,
            const QHostAddress& senderAddress,
            quint16 senderPort);

signals:
    void frameReady(const UdpFrame& frame);
    void frameDropped(quint32 frameSeq);

private:
    struct FrameBuffer {
        UdpChannelType channelType = UdpChannelType::Unknown;
        UdpFrameType frameType = UdpFrameType::Unknown;

        QHostAddress senderAddress;
        quint16 senderPort = 0;

        quint16 fragmentCount = 0;
        QMap<quint16, QByteArray> fragments;

        bool isComplete() const {
            //return fragmentCount > 0 && fragmentCount == fragments.size();
            return fragmentCount > 0
                   && fragments.size() == static_cast<int>(fragmentCount);
        }
    };

private:
    void emitCompleteFrame(const FrameBuffer& buffer);
    void dropFramesOlderThan(const QHostAddress& senderAddress,
            quint16 senderPort,
            quint32 frameSeq);

private:
    QMap<FrameKey, FrameBuffer> pendingFrame_;
};


#endif //P2PPLAY_UDPFRAMEREASSEMBLER_H
