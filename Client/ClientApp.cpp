//
// Created by ASUS on 2026/8/1.
//

#include "ClientApp.h"
#include "TraceManager.h"

ClientApp::ClientApp()
    : signalingClient_(this),
      dispatcher_(this),
      p2pSession_(this),
      mediaService_(this),
      audioService_(this),
      avSyncService_(this),
      controlService_(this),
      hostRoleService_(this),
      guestRoleService_(this),
      videoSenderPipeline_(this),
      videoRecevierPipline_(this),
      videoWidget_(nullptr),
      inputService_(this),
      rateController_(this),
      receiverMonitor_(this) {
    qRegisterMetaType<InputSample>("InputSample");
    qRegisterMetaType<DecodedAudioFrame>("DecodedAudioFrame");
    qRegisterMetaType<DecodedVideoFrame>("DecodedVideoFrame");
    TraceManager::instance();
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

    mediaService_.setRole(Role::Host);
    audioService_.applyConfig(AppConfig::defaultHost());
    audioService_.setRole(Role::Host);
    avSyncService_.setRole(Role::Host);
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

    mediaService_.setRole(Role::Guest);
    audioService_.applyConfig(AppConfig::defaultGuest());
    audioService_.setRole(Role::Guest);
    avSyncService_.setRole(Role::Guest);

    return true;
}


bool ClientApp::startAsHost(const QString& clientId, const AppConfig& config) {
    clearRoleConnections();

    role_ = Role::Host;
    hostRoleService_.setClientId(clientId);
    mediaService_.setRole(Role::Host);
    audioService_.applyConfig(config);
    audioService_.setRole(Role::Host);
    avSyncService_.setRole(Role::Host);
    avSyncService_.setAudioEnabled(config.audio.playbackEnabled);
    avSyncService_.setVideoEnabled(true);
    avSyncService_.setAvSyncEnabled(config.audio.avSyncEnabled);
    controlService_.setRole(Role::Host);

    p2pSession_.applyConfig(config);
    p2pSession_.setServerUdpEndpoint(
            config.server.udpAddress,
            config.server.udpPort
            );

    videoSenderPipeline_.applyConfig(config);
    rateController_.applyConfig(config);
    if (!p2pSession_.bind(config.p2p.localUdpPort)) {
        return false;
    }

    connectRoleSignals();
    signalingClient_.connectToServer(
            config.server.tcpAddress,
            config.server.tcpPort
            );

    inputService_.setRole(Role::Host);
    return true;
}

bool ClientApp::startAsGuest(const QString& clientId,
                             const QString& roomId,
                             const AppConfig& config) {
    clearRoleConnections();

    role_ = Role::Guest;
    videoWidget_.show();
    guestRoleService_.setClientInfo(roomId, clientId);
    mediaService_.setRole(Role::Guest);
    audioService_.applyConfig(config);
    audioService_.setRole(Role::Guest);
    avSyncService_.setRole(Role::Guest);
    avSyncService_.setAudioEnabled(config.audio.playbackEnabled);
    avSyncService_.setVideoEnabled(true);
    avSyncService_.setAvSyncEnabled(config.audio.avSyncEnabled);
    controlService_.setRole(Role::Guest);
    p2pSession_.applyConfig(config);
    p2pSession_.setServerUdpEndpoint(
            config.server.udpAddress,
            config.server.udpPort
    );

    videoRecevierPipline_.applyConfig(config);

    receiverMonitor_.applyConfig(config);

    videoWidget_.applyConfig(config);

    if (!p2pSession_.bind(config.p2p.localUdpPort)) {
        return false;
    }

    connectRoleSignals();

    signalingClient_.connectToServer(
            config.server.tcpAddress,
            config.server.tcpPort
    );

    inputService_.setRole(Role::Guest);

    return true;
}

void ClientApp::connectCommonSignals() {
    connect(&signalingClient_,
            &SignalingClient::messageReceived,
            &dispatcher_,
            &ClientDispatcher::onMessageReceived);

    connect(&dispatcher_,
            &ClientDispatcher::probePermitted,
            &p2pSession_,
            &P2pSession::onProbePermitted);

    connect(&dispatcher_,
            &ClientDispatcher::peerEndpoint,
            &p2pSession_,
            &P2pSession::onPeerEndpoint);

    connect(&dispatcher_,
            &ClientDispatcher::errorReceived,
            this,
            [this](const SignalingMessage& message) {
                emit errorOccurred(message.reason);
            });

    connect(&dispatcher_,
            &ClientDispatcher::unknownMessage,
            this,
            [this](const SignalingMessage& message) {
                emit errorOccurred(
                    QString("unknown signaling message: %1")
                            .arg(static_cast<quint16>(
                                         message.type)));
            });

    // MediaServiceWorker -> P2pSessionWorker
    connect(mediaService_.worker(),
            &MediaServiceWorker::udpMediaFrameToSend,
            p2pSession_.worker(),
            &P2pSessionWorker::sendMediaFrame);

    // P2pSessionWorker -> MediaServiceWorker
    connect(p2pSession_.worker(),
            &P2pSessionWorker::mediaFrameReceived,
            mediaService_.worker(),
            &MediaServiceWorker::onUdpMediaFrameReceived);

    // ControlServiceWorker -> P2pSessionWorker
    connect(controlService_.worker(),
            &ControlServiceWorker::controlFrameToSend,
            p2pSession_.worker(),
            &P2pSessionWorker::sendControlFrame);

    // P2pSessionWorker -> ControlServiceWorker
    connect(p2pSession_.worker(),
            &P2pSessionWorker::controlFrameReceived,
            controlService_.worker(),
            &ControlServiceWorker::onControlFrameReceived);

    // P2P建立完成后启动媒体和控制服务
    connect(p2pSession_.worker(),
            &P2pSessionWorker::p2pReady,
            mediaService_.worker(),
            &MediaServiceWorker::onP2pReady);

    // 音频数据只在 worker 之间传递，不经过 ClientApp 主线程。
    connect(audioService_.worker(),
            &AudioServiceWorker::audioSampleBytesReady,
            mediaService_.worker(),
            &MediaServiceWorker::sendAudioSampleBytes);

    connect(mediaService_.worker(),
            &MediaServiceWorker::audioSampleBytesReceived,
            audioService_.worker(),
            &AudioServiceWorker::onAudioSampleBytesReceived);

    connect(audioService_.worker(),
            &AudioServiceWorker::decodedAudioFrameReady,
            avSyncService_.worker(),
            &AvSyncWorker::onAudioFrameReady);

    connect(avSyncService_.worker(),
            &AvSyncWorker::audioFrameToPlay,
            audioService_.worker(),
            &AudioServiceWorker::onAudioFrameToPlay);

    connect(p2pSession_.worker(),
            &P2pSessionWorker::p2pReady,
            controlService_.worker(),
            &ControlServiceWorker::onP2pReady);

    connect(p2pSession_.worker(),
            &P2pSessionWorker::p2pReady,
            audioService_.worker(),
            &AudioServiceWorker::start);

    // 服务日志和错误
    connect(&mediaService_,
            &MediaService::logReceived,
            this,
            &ClientApp::logReceived);

    connect(&mediaService_,
            &MediaService::errorOccurred,
            this,
            &ClientApp::errorOccurred);

    // AudioService 只转发日志和错误；音频数据不经过此处。
    connect(&audioService_,
            &AudioService::logReceived,
            this,
            &ClientApp::logReceived);

    connect(&audioService_,
            &AudioService::errorOccurred,
            this,
            &ClientApp::errorOccurred);

    connect(&avSyncService_,
            &AvSyncService::logReceived,
            this,
            &ClientApp::logReceived);

    connect(&avSyncService_,
            &AvSyncService::errorOccurred,
            this,
            &ClientApp::errorOccurred);

    connect(&controlService_,
            &ControlChannelService::logReceived,
            this,
            &ClientApp::logReceived);

    connect(&controlService_,
            &ControlChannelService::errorOccurred,
            this,
            &ClientApp::errorOccurred);

    connect(&signalingClient_,
            &SignalingClient::errorOccurred,
            this,
            &ClientApp::errorOccurred);

    connect(&p2pSession_,
            &P2pSession::errorOccurred,
            this,
            &ClientApp::errorOccurred);

    connect(&p2pSession_,
            &P2pSession::logReceived,
            this,
            &ClientApp::logReceived);

    // P2P完成后启动输入服务
    connect(p2pSession_.worker(),
            &P2pSessionWorker::p2pReady,
            inputService_.worker(),
            &InputServiceWorker::start);

    connect(&videoWidget_,
            &VideoWidget::inputControlActiveChanged,
            &inputService_,
            &InputService::setControlActive);

    // InputServiceWorker -> ControlServiceWorker
    connect(inputService_.worker(),
            &InputServiceWorker::inputSampleBytesReady,
            controlService_.worker(),
            &ControlServiceWorker::sendInputEvent);

    // ControlServiceWorker -> InputServiceWorker
    connect(controlService_.worker(),
            &ControlServiceWorker::inputEventReceived,
            inputService_.worker(),
            &InputServiceWorker::onInputSampleBytesReceived);

    // InputServiceWorker -> ControlServiceWorker
    // Host发送Ack，Guest发送输入事件
    connect(inputService_.worker(),
            &InputServiceWorker::inputAckSampleBytesReady,
            controlService_.worker(),
            &ControlServiceWorker::sendInputEvent);


    // Guest 视频窗口鼠标事件 → InputServiceWorker
    connect(
            &videoWidget_,
            &VideoWidget::mouseInputSampleReady,
            inputService_.worker(),
            &InputServiceWorker::onMouseInputSampleReady
    );

    // Host 鼠标位置 → Guest 虚拟鼠标绘制
    connect(
            inputService_.worker(),
            &InputServiceWorker::hostMousePositionReceived,
            &videoWidget_,
            &VideoWidget::onHostMousePositionReceived
    );

    // 视频窗口控制状态 → 输入服务
    connect(
            &videoWidget_,
            &VideoWidget::inputControlActiveChanged,
            inputService_.worker(),
            &InputServiceWorker::setControlActive
    );
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
                &videoSenderPipeline_, &VideoSenderPipeline::start));

        roleConnections_.append(
                connect(videoSenderPipeline_.screenVideoSource(),
                &ScreenVideoSource::videoImageReady,
                videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::onVideoImageReady));

        roleConnections_.append(
                connect(videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::videoSampleBytesReady,
                mediaService_.worker(),
                &MediaServiceWorker::sendVideoSampleBytes));

        //---------------------自适应系统----------------------------
        // 通知控制器带宽受限
        roleConnections_.append(connect(
                p2pSession_.worker(),
                &P2pSessionWorker::sendBlock,
                &rateController_,
                &RateController::onSendBlock
                ));

        // 通知编码器调整码率
        roleConnections_.append(connect(
                &rateController_,
                &RateController::targetBitrateChanged,
                videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::onTargetBitrateChanged));

        // 通知发送方调节速率
        roleConnections_.append(connect(
                    videoSenderPipeline_.encoderWorker(),
                    &VideoEncoderWorker::changePacketsPerTick,
                    p2pSession_.worker(),
                    &P2pSessionWorker::onChangePacketsPerTick)
                );

        roleConnections_.append(connect(
                controlService_.worker(),
                &ControlServiceWorker::keyFrameRequestReceived,
                videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::requestKeyFrame));

        roleConnections_.append(connect(
                controlService_.worker(),
                &ControlServiceWorker::receiverReportReceived,
                &rateController_,
                &RateController::onReceiverReportBytesReceived));
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


        roleConnections_.append(connect(
                mediaService_.worker(),
                &MediaServiceWorker::videoSampleBytesReceived,
                videoRecevierPipline_.decoderWorker(),
                &VideoDecoderWorker::onVideoSampleBytesReceived));

        roleConnections_.append(connect(
                videoRecevierPipline_.decoderWorker(),
                &VideoDecoderWorker::videoImageReady,
                &videoWidget_,
                &VideoWidget::onVideoImageReady));

        //------------------自适应系统------------------
        roleConnections_.append(connect(
                mediaService_.worker(),
                &MediaServiceWorker::videoSampleBytesReceived,
                receiverMonitor_.worker(),
                &ReceiverMonitorWorker::onVideoSampleBytes
                ));

        roleConnections_.append(connect(
                receiverMonitor_.worker(),
                &ReceiverMonitorWorker::reportFrameBytesReady,
                controlService_.worker(),
                &ControlServiceWorker::sendReceiverReport));

        roleConnections_.append(connect(
                videoRecevierPipline_.decoderWorker(),
                &VideoDecoderWorker::keyFrameRequestNeeded,
                controlService_.worker(),
                &ControlServiceWorker::sendKeyFrameRequest));
    }
}

void ClientApp::clearRoleConnections() {
    for (const QMetaObject::Connection& connection : roleConnections_) {
        disconnect(connection);
    }
    roleConnections_.clear();
}
