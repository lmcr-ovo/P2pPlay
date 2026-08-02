//
// Created by ASUS on 2026/8/1.
//

#include "NatTraversalService.h"
#include "UdpControlPayload.h"
#include "UdpControlPayloadCodec.h"

NatTraversalService::NatTraversalService(QObject* parent)
    : QObject(parent),
    sock_(this),
    transport_(&sock_, this),
    punchTimer_(this) {

    transport_.setPeerFilterEnabled(false);

    connect(&transport_, &P2pUdpTransport::frameReady,
            this, &NatTraversalService::onFrameReady);
    connect(&transport_, &P2pUdpTransport::errorOccurred,
            this, &NatTraversalService::errorOccurred);
    connect(&punchTimer_, &QTimer::timeout,
            this, &NatTraversalService::sendPunch);
    punchTimer_.setInterval(200);
}

bool NatTraversalService::bind(quint16 localPort) {
    if (localPort == 0) {
        emit errorOccurred("invalid local udp port");
        return false;
    }

    if (!sock_.bind(QHostAddress::AnyIPv4, localPort)) {
        emit errorOccurred(sock_.errorString());
        return false;
    }
    return true;
}

void NatTraversalService::setClientInfo(const QString &roomId, const QString &clientId) {
    roomId_ = roomId;
    clientId_ = clientId;
}

void NatTraversalService::setServerUdpEndpoint(const QHostAddress &address, quint16 port) {
    serverUdpAddress_ = address;
    serverUdpPort_ =  port;
}

void NatTraversalService::setPunchPortRange(quint16 range) {
    punchPortRange_ = range;
}

void NatTraversalService::onProbePermitted(const SignalingMessage &message) {
    if (!message.success) {
        emit errorOccurred(message.reason);
        return;
    }

    if (!message.roomId.isEmpty()) {
        roomId_ = message.roomId;
    }
    sendProbe();
}

void NatTraversalService::onPeerEndpoint(const SignalingMessage &message) {
    if (!message.success) {
        emit errorOccurred(message.reason);
        return;
    }

    if (message.endpointAddress.isNull() || message.endpointPort == 0) {
        emit errorOccurred("invalid peer endpoint");
        return;
    }

    peerAddress_ = message.endpointAddress;
    peerPort_ = message.endpointPort;

    transport_.setPeerEndpoint(peerAddress_, peerPort_);
    transport_.setPeerFilterEnabled(false);

    emit logReceived(QString("peer endpoint: %1:%2")
                                .arg(peerAddress_.toString())
                                .arg(peerPort_));

    sendPunch();

    if (!punchTimer_.isActive()) {
        punchTimer_.start();
    }
}

void NatTraversalService::sendProbe() {
    if (roomId_.isEmpty() || clientId_.isEmpty()) {
        emit errorOccurred("room id or client id is null");
        return;
    }

    if (serverUdpAddress_.isNull() || serverUdpPort_ == 0) {
        emit errorOccurred("server udp endpoint is not set");
        return;
    }

    UdpProbePayload payload;
    payload.roomId = roomId_;
    payload.clientId = clientId_;

    const bool ok = transport_.sendFrameTo(
            serverUdpAddress_,
            serverUdpPort_,
            UdpChannelType::Control,
            UdpFrameType::Probe,
            UdpControlPayloadCodec::encodeProbe(payload)
            );

    if (!ok) {
        emit errorOccurred("send udp probe failed");
        return;
    }

    emit logReceived("udp probe sent");
}

void NatTraversalService::sendPunch() {
    if (p2pReady_) {
        punchTimer_.stop();
        return;
    }

    if (peerAddress_.isNull() || peerPort_ == 0) {
        return;
    }

    UdpPunchPayload payload;
    payload.roomId = roomId_;
    payload.clientId = clientId_;

    const QByteArray bytes = UdpControlPayloadCodec::encodePunch(payload);
    const QList<quint16> ports = punchCandidatePorts();

    for (quint16 port : ports) {
        transport_.sendFrameTo(
                peerAddress_,
                port,
                UdpChannelType::Control,
                UdpFrameType::Punch,
                bytes
                );
    }
}

QList<quint16> NatTraversalService::punchCandidatePorts() const {
    QList<quint16> ports;

    if (peerPort_ == 0) {
        return ports;
    }

    ports.append(peerPort_);

    for (quint16 offset = 1; offset <= punchPortRange_; ++offset) {
        if (peerPort_ > offset) {
            ports.append(static_cast<quint16>(peerPort_ - offset));
        }

        const quint32 upper = static_cast<quint32>(peerPort_) + offset;
        if (upper <= 65535) {
            ports.append(static_cast<quint16>(upper));
        }
    }

    return ports;
}

void NatTraversalService::onFrameReady(const UdpFrame &frame) {
    if (frame.channelType != UdpChannelType::Control) {
        return;
    }

    switch (frame.frameType) {
        case UdpFrameType::ProbeAck :
            emit logReceived("udp probe ack received");
            break;
        case UdpFrameType::Punch :
            handlePunch(frame);
            break;
        case UdpFrameType::PunchAck :
            handlePunchAck(frame);
            break;

        default:
            break;
    }
}

void NatTraversalService::handlePunch(const UdpFrame &frame) {
    UdpPunchPayload payload;
    if (!UdpControlPayloadCodec::decodePunch(frame.payload, &payload)) {
        emit errorOccurred("invalid punch payload");
        return;
    }

    if (payload.roomId != roomId_) {
        return;
    }

    peerAddress_ = frame.senderAddress;
    peerPort_ = frame.senderPort;

    transport_.setPeerEndpoint(peerAddress_, peerPort_);
    transport_.setPeerFilterEnabled(true);

    UdpPunchAckPayload ack;
    ack.roomId = roomId_;
    ack.clientId = clientId_;

    transport_.sendFrame(
            UdpChannelType::Control,
            UdpFrameType::PunchAck,
            UdpControlPayloadCodec::encodePunchAck(ack)
            );
    markP2pReady(peerAddress_, peerPort_);
}

void NatTraversalService::handlePunchAck(const UdpFrame &frame) {
    UdpPunchAckPayload payload;
    if (!UdpControlPayloadCodec::decodePunchAck(frame.payload, &payload)) {
        emit errorOccurred("invalid punch ack payload");
        return;
    }

    if (payload.roomId != roomId_) {
        return;
    }

    markP2pReady(frame.senderAddress, frame.senderPort);
}

void NatTraversalService::markP2pReady(const QHostAddress &address, quint16 port) {
    if (p2pReady_) {
        return;
    }
    p2pReady_ = true;
    punchTimer_.stop();

    peerAddress_ = address;
    peerPort_ = port;

    transport_.setPeerEndpoint(peerAddress_, peerPort_);
    transport_.setPeerFilterEnabled(true);

    emit p2pReady(peerAddress_, peerPort_);
}
