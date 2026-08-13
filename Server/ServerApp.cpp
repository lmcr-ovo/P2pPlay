//
// Created by ASUS on 2026/7/30.
//

#include "ServerApp.h"

ServerApp::ServerApp()
    : roomService_(this),
    natProbeService_(this) {
    // 将收到的信令同一交给分发器分发
    connect(&signalingServer_, &SignalingServer::messageReceived,
            &signalingDispatcher_, &SignalingDispatcher::onMessageReceived);

    // 分发器发布信令
    //// 房间相关信令
    connect(&signalingDispatcher_, &SignalingDispatcher::registerRequest,
            &roomService_, &RoomService::onRegister);
    connect(&signalingDispatcher_, &SignalingDispatcher::createRoomRequest,
            &roomService_, &RoomService::onCreateRoom);
    connect(&signalingDispatcher_, &SignalingDispatcher::joinRoomRequest,
            &roomService_, &RoomService::onJoinRoom);

    //// 内网穿透相关信令
    connect(&signalingDispatcher_, &SignalingDispatcher::probeRequest,
            &roomService_, &RoomService::onProbeRequest);
    // 收到Probe
    connect(&natProbeService_, &NatProbeService::probeReceived,
            &roomService_, &RoomService::onSingleUdpEndpointReady);

    //// 错误传递
    connect(&natProbeService_, &NatProbeService::errorOccurred,
            this, &ServerApp::errorOccurred);
}
/*
bool ServerApp::start(const ServerConfig& config) {
    return start(config.tcpAddress,
                 config.tcpPort,
                 config.udpAddress,
                 config.udpPort);
}
*/
/*
 * 启动服务器开始工作
 * 启动tcp信令服务及内网穿透服务
 */
bool ServerApp::start(const QHostAddress& tcpAddress,
                      quint16 tcpPort,
                      const QHostAddress& udpAddress,
                      quint16 udpPort) {
    if (!signalingServer_.start(tcpAddress, tcpPort)) {
        emit errorOccurred("failed to start signaling server");
        return false;
    }

    if (!natProbeService_.start(udpAddress, udpPort)) {
        return false;
    }

    return true;
}
