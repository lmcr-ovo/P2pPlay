//
// Created by ASUS on 2026/8/1.
//

#include "ClientApp.h"

ClientApp::ClientApp()
    : signalingClient_(this),
      dispatcher_(this),
      p2pSession_(this),
      mediaService_(this),
      hostRoleService_(this),
      guestRoleService_(this),
      videoCapturer_(this),
      videoRender_(this) {
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
    p2pSession_.setServerUdpEndpoint(serverUdpAddress, serverUdpPort);

    if (!p2pSession_.bind(localUdpPort)) {
        return false;
    }

    connectRoleSignals();
    signalingClient_.connectToServer(serverTcpAddress, serverTcpPort);

    mediaService_.setRole(MediaRole::Host);
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
    p2pSession_.setServerUdpEndpoint(serverUdpAddress, serverUdpPort);

    if (!p2pSession_.bind(localUdpPort)) {
        return false;
    }

    connectRoleSignals();
    signalingClient_.connectToServer(serverTcpAddress, serverTcpPort);

    mediaService_.setRole(MediaRole::Guest);
    return true;
}


bool ClientApp::startAsHost(const QString& clientId, const AppConfig& config) {
    clearRoleConnections();

    role_ = Role::Host;
    hostRoleService_.setClientId(clientId);
    mediaService_.setRole(MediaRole::Host);

    p2pSession_.applyConfig(config.p2p);
    p2pSession_.setServerUdpEndpoint(
            config.server.udpAddress,
            config.server.udpPort
            );

    videoCapturer_.applyConfig(config.video);

    if (!p2pSession_.bind(config.p2p.localUdpPort)) {
        return false;
    }

    connectRoleSignals();
    signalingClient_.connectToServer(
            config.server.tcpAddress,
            config.server.tcpPort
            );
    return true;
}

bool ClientApp::startAsGuest(const QString& clientId,
                             const QString& roomId,
                             const AppConfig& config) {
    clearRoleConnections();

    role_ = Role::Guest;
    guestRoleService_.setClientInfo(roomId, clientId);
    mediaService_.setRole(MediaRole::Guest);

    p2pSession_.applyConfig(config.p2p);
    p2pSession_.setServerUdpEndpoint(
            config.server.udpAddress,
            config.server.udpPort
    );

    videoCapturer_.applyConfig(config.video);

    if (!p2pSession_.bind(config.p2p.localUdpPort)) {
        return false;
    }

    connectRoleSignals();

    signalingClient_.connectToServer(
            config.server.tcpAddress,
            config.server.tcpPort
    );

    return true;
}

void ClientApp::connectCommonSignals() {
    connect(&signalingClient_, &SignalingClient::messageReceived,
            &dispatcher_, &ClientDispatcher::onMessageReceived);

    connect(&dispatcher_, &ClientDispatcher::probePermitted,
            &p2pSession_, &P2pSession::onProbePermitted);

    connect(&dispatcher_, &ClientDispatcher::peerEndpoint,
            &p2pSession_, &P2pSession::onPeerEndpoint);

    connect(&dispatcher_, &ClientDispatcher::errorReceived,
            this, [this](const SignalingMessage& message) {
        emit errorOccurred(message.reason);
    });

    connect(&dispatcher_, &ClientDispatcher::unknownMessage,
            this, [this](const SignalingMessage& message) {
        emit errorOccurred(QString("unknown signaling message: %1")
                                   .arg(static_cast<quint16>(message.type)));
    });



    connect(&p2pSession_, &P2pSession::p2pReady,
            &mediaService_, &MediaService::onP2pReady);
    connect(&p2pSession_, &P2pSession::mediaFrameReceived,
            &mediaService_, &MediaService::onMediaFrameReceived);
    connect(&mediaService_, &MediaService::mediaFrameToSend,
            &p2pSession_, &P2pSession::sendMediaFrame);
    connect(&mediaService_, &MediaService::logReceived,
            this, &ClientApp::logReceived);
    connect(&mediaService_, &MediaService::errorOccurred,
            this, &ClientApp::errorOccurred);



    connect(&signalingClient_, &SignalingClient::errorOccurred,
            this, &ClientApp::errorOccurred);

    connect(&p2pSession_, &P2pSession::errorOccurred,
            this, &ClientApp::errorOccurred);

    connect(&p2pSession_, &P2pSession::logReceived,
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
                                        &p2pSession_, &P2pSession::setClientInfo));

        roleConnections_.append(connect(&p2pSession_, &P2pSession::p2pReady,
                                        &hostRoleService_, &HostRoleService::onP2pReady));

        roleConnections_.append(connect(&hostRoleService_, &HostRoleService::logReceived,
                this, &ClientApp::logReceived));

        roleConnections_.append(connect(&hostRoleService_, &HostRoleService::errorOccurred,
                this, &ClientApp::errorOccurred));



        // 测试
        roleConnections_.append(connect(&p2pSession_, &P2pSession::p2pReady,
                &videoCapturer_, &VideoCapturer::onP2pReady));
        roleConnections_.append(connect(&videoCapturer_, &VideoCapturer::videoFrameReady,
                &mediaService_, &MediaService::sendVideoFrame));
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
                                        &p2pSession_, &P2pSession::setClientInfo));

        roleConnections_.append(connect(&p2pSession_, &P2pSession::p2pReady,
                                        &guestRoleService_, &GuestRoleService::onP2pReady));

        roleConnections_.append(connect(&guestRoleService_, &GuestRoleService::logReceived,
                this, &ClientApp::logReceived));

        roleConnections_.append(connect(&guestRoleService_, &GuestRoleService::errorOccurred,
                this, &ClientApp::errorOccurred));



        roleConnections_.append(connect(&mediaService_, &MediaService::videoFrameReceived,
                &videoRender_, &VideoRender::onVideoFrameRecevied));
    }
}

void ClientApp::clearRoleConnections() {
    for (const QMetaObject::Connection& connection : roleConnections_) {
        disconnect(connection);
    }

    roleConnections_.clear();
}
