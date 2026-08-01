//
// Created by ASUS on 2026/7/30.
//

#include "NatProbeService.h"
#include "UdpControlPayload.h"
#include "UdpControlPayloadCodec.h"

NatProbeService::NatProbeService(QObject* parent)
    : QObject(parent),
    sock_(this),
    transport_(&sock_, this) {
    transport_.setPeerFilterEnabled(false);
    connect(&transport_, &P2pUdpTransport::frameReady,
            this, &NatProbeService::onFrameReady);
    connect(&transport_, &P2pUdpTransport::errorOccurred,
            this, &NatProbeService::errorOccurred);
}

bool NatProbeService::start(const QHostAddress& address, quint16 port) {
    if (!sock_.bind(address, port)) {
        emit errorOccurred(sock_.errorString());
        return false;
    }
    return true;
}

void NatProbeService::onFrameReady(const UdpFrame &frame) {
    if (frame.channelType != UdpChannelType::Control
        || frame.frameType != UdpFrameType::Probe) {
        return;
    }

    UdpProbePayload payload;
    if (!UdpControlPayloadCodec::decodeProbe(frame.payload, &payload)) {
        emit errorOccurred("invalid udp robe payload");
        return;
    }

    if (payload.roomId.isEmpty() || payload.clientId.isEmpty()) {
        emit errorOccurred("empty room id  or client id in udp probe");
        return;
    }

    emit probeReceived(
            payload.roomId,
            payload.clientId,
            frame.senderAddress,
            frame.senderPort
    );

    UdpProbeAckPayload ack;
    ack.roomId = payload.roomId;
    ack.clientId = payload.clientId;
    ack.success = true;
    transport_.sendFrameTo(
            frame.senderAddress,
            frame.senderPort,
            UdpChannelType::Control,
            UdpFrameType::ProbeAck,
            UdpControlPayloadCodec::encodeProbeAck(ack)
            );
}