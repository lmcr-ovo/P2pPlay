//
// Created by ASUS on 2026/7/27.
//

#ifndef P2PPLAY_UDPFRAMEREASSEMBLER_H
#define P2PPLAY_UDPFRAMEREASSEMBLER_H

#include <QtCore>
#include <QObject>
#include <QHostAddress>
#include <QHash>
#include "UdpPacket.h"

/*
 * 每组ip port 的组合键
 */
struct PeerKey {
    QString addressText;
    quint16 port = 0;

    bool operator==(const PeerKey& other) const {
        return addressText == other.addressText
            && port == other.port;
    }

    static PeerKey makePeerKey(const QHostAddress& address, quint16 port) {
        PeerKey key;
        key.addressText = address.toString();
        key.port = port;
        return key;
    }
};

inline uint qHash(const PeerKey& key, uint seed = 0) {
    uint h1 = qHash(key.addressText, seed);
    uint h2 = qHash(key.port, h1);
    return h2;
}


struct FrameBuffer {
    UdpChannelType channelType = UdpChannelType::Unknown;
    UdpFrameType frameType = UdpFrameType::Unknown;

    QHostAddress senderAddress;
    quint16 senderPort = 0;

    quint32 frameSeq = 0;
    quint16 fragmentCount = 0;

    QByteArray payload;
    QBitArray received;

    quint16 receivedCount = 0;
    quint32 totalSize = 0;

    bool isComplete() const {
        return fragmentCount > 0
            && receivedCount == fragmentCount;
    }
};

/*
 * 管理每个peer的多个帧
 */
struct PeerState {
    QMap<quint32, FrameBuffer> pendingFrame_;
    PeerKey key_;
    static const quint16 MaxFrameLag = 8;
    static const quint16 MaxPendingFramesPerPeer = 16;
    quint32 latestSeenSeq = 0;
    bool hasLatestSeenSeq = false;
    explicit PeerState(const PeerKey& key) : key_(key) {}
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
private:
    QHash<PeerKey, PeerState> peers_;
};


#endif //P2PPLAY_UDPFRAMEREASSEMBLER_H
