//
// Created by ASUS on 2026/8/1.
//

#include "ClientApp.h"

ClientApp::ClientApp()
    : signalingClient_(this),
      dispatcher_(this),
      natTraversalService_(this),
      hostRoleService_(this),
      guestRoleService_(this) {
    connectCommonSignals();
}

bool ClientApp::startAsHost(const QString& clientId,
                            const QHostAddress& serverTcpAddress,
                            quint16 serverTcpPort,
                            const QHostAddress& serverUdpAddress,
                            quint16 serverUdpPort,
                            quint16 localUdpPort) {
    clearRoleConnections();

    role_ = Role::Host;
    hostRoleService_.setClientId(clientId);
    natTraversalService_.setServerUdpEndpoint(serverUdpAddress, serverUdpPort);

    if (!natTraversalService_.bind(localUdpPort)) {
        return false;
    }

    connectRoleSignals();
    signalingClient_.connectToServer(serverTcpAddress, serverTcpPort);
    return true;
}

bool ClientApp::startAsGuest(const QString& clientId,
                             const QString& roomId,
                             const QHostAddress& serverTcpAddress,
                             quint16 serverTcpPort,
                             const QHostAddress& serverUdpAddress,
                             quint16 serverUdpPort,
                             quint16 localUdpPort) {
    clearRoleConnections();

    role_ = Role::Guest;
    guestRoleService_.setClientInfo(roomId, clientId);
    natTraversalService_.setServerUdpEndpoint(serverUdpAddress, serverUdpPort);

    if (!natTraversalService_.bind(localUdpPort)) {
        return false;
    }

    connectRoleSignals();
    signalingClient_.connectToServer(serverTcpAddress, serverTcpPort);
    return true;
}

void ClientApp::connectCommonSignals() {
    connect(&signalingClient_, &SignalingClient::messageReceived,
            &dispatcher_, &ClientDispatcher::onMessageReceived);

    connect(&dispatcher_, &ClientDispatcher::probePermitted,
            &natTraversalService_, &NatTraversalService::onProbePermitted);

    connect(&dispatcher_, &ClientDispatcher::peerEndpoint,
            &natTraversalService_, &NatTraversalService::onPeerEndpoint);

    connect(&dispatcher_, &ClientDispatcher::errorReceived,
            this, [this](const SignalingMessage& message) {
        emit errorOccurred(message.reason);
    });

    connect(&dispatcher_, &ClientDispatcher::unknownMessage,
            this, [this](const SignalingMessage& message) {
        emit errorOccurred(QString("unknown signaling message: %1")
                                   .arg(static_cast<quint16>(message.type)));
    });

    connect(&signalingClient_, &SignalingClient::errorOccurred,
            this, &ClientApp::errorOccurred);

    connect(&natTraversalService_, &NatTraversalService::errorOccurred,
            this, &ClientApp::errorOccurred);

    connect(&natTraversalService_, &NatTraversalService::logReceived,
            this, &ClientApp::logReceived);
}

void ClientApp::connectRoleSignals() {
    if (role_ == Role::Host) {
        roleConnections_.append(connect(&signalingClient_, &SignalingClient::connected,
                &hostRoleService_, &HostRoleService::onConnected));

        roleConnections_.append(connect(&dispatcher_, &ClientDispatcher::roomCreated,
                &hostRoleService_, &HostRoleService::onRoomCreated));

        roleConnections_.append(connect(&dispatcher_, &ClientDispatcher::peerJoined,
                &hostRoleService_, &HostRoleService::onPeerJoined));

        roleConnections_.append(connect(&hostRoleService_, &HostRoleService::sendMessage,
                &signalingClient_, &SignalingClient::sendMessage));

        roleConnections_.append(connect(&hostRoleService_, &HostRoleService::traversalContextReady,
                &natTraversalService_, &NatTraversalService::setClientInfo));

        roleConnections_.append(connect(&natTraversalService_, &NatTraversalService::p2pReady,
                &hostRoleService_, &HostRoleService::onP2pReady));

        roleConnections_.append(connect(&hostRoleService_, &HostRoleService::logReceived,
                this, &ClientApp::logReceived));

        roleConnections_.append(connect(&hostRoleService_, &HostRoleService::errorOccurred,
                this, &ClientApp::errorOccurred));
        return;
    }

    if (role_ == Role::Guest) {
        roleConnections_.append(connect(&signalingClient_, &SignalingClient::connected,
                &guestRoleService_, &GuestRoleService::onConnected));

        roleConnections_.append(connect(&dispatcher_, &ClientDispatcher::logReceived,
                &guestRoleService_, &GuestRoleService::onLogReceived));

        roleConnections_.append(connect(&guestRoleService_, &GuestRoleService::sendMessage,
                &signalingClient_, &SignalingClient::sendMessage));

        roleConnections_.append(connect(&guestRoleService_, &GuestRoleService::traversalContextReady,
                &natTraversalService_, &NatTraversalService::setClientInfo));

        roleConnections_.append(connect(&natTraversalService_, &NatTraversalService::p2pReady,
                &guestRoleService_, &GuestRoleService::onP2pReady));

        roleConnections_.append(connect(&guestRoleService_, &GuestRoleService::logReceived,
                this, &ClientApp::logReceived));

        roleConnections_.append(connect(&guestRoleService_, &GuestRoleService::errorOccurred,
                this, &ClientApp::errorOccurred));
    }
}

void ClientApp::clearRoleConnections() {
    for (const QMetaObject::Connection& connection : roleConnections_) {
        disconnect(connection);
    }

    roleConnections_.clear();
}
