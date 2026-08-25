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
      hostRoleService_(this),
      guestRoleService_(this),
      videoSenderPipeline_(this),
      videoRecevierPipline_(this),
      videoWidget_(nullptr),
      //inputCapture_(this),
      //inputSender_(this),
      //inputReceiver_(this),
      inputService_(this),
      rateController_(this),
      receiverMonitor_(this) {
    qRegisterMetaType<InputSample>("InputSample");
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

    return true;
}


bool ClientApp::startAsHost(const QString& clientId, const AppConfig& config) {
    clearRoleConnections();

    role_ = Role::Host;
    hostRoleService_.setClientId(clientId);
    mediaService_.setRole(Role::Host);

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

//////////////////////////////////////////////////////////////////////////////////

    // 多媒体业务向p2pSession请求发送媒体帧
    connect(mediaService_.worker(),
            &MediaServiceWorker::udpMediaFrameToSend,
            p2pSession_.worker(),
            &P2pSessionWorker::sendMediaFrame);

    connect(mediaService_.worker(),
            &MediaServiceWorker::udpControlFrameToSend,
            p2pSession_.worker(),
            &P2pSessionWorker::sendControlFrame);

    connect(&p2pSession_, &P2pSession::p2pReady,
            mediaService_.worker(),
            &MediaServiceWorker::onP2pReady);
    connect(&p2pSession_, &P2pSession::mediaFrameReceived,
            mediaService_.worker(),
            &MediaServiceWorker::onUdpMediaFrameReceived);

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

    // 通知输入服务开始启动
    connect(&p2pSession_, &P2pSession::p2pReady,
            &inputService_, &InputService::start);

    connect(&videoWidget_, &VideoWidget::inputControlActiveChanged,
            &inputService_, &InputService::setControlActive);

    // 输入服务向多媒体服务请求发送InputSampleBytes
    connect(inputService_.worker(),
            &InputServiceWorker::inputSampleBytesReady,
            mediaService_.worker(),
            &MediaServiceWorker::sendInputSampleBytes);

    // 处理host发回的Ack
    connect(mediaService_.worker(),
            &MediaServiceWorker::inputSampleBytesReceived,
            inputService_.worker(),
            &InputServiceWorker::onInputSampleBytesReceived
            );

    // 接收方回复Ack
    connect(inputService_.worker(),
            &InputServiceWorker::inputAckSampleBytesReady,
            mediaService_.worker(),
            &MediaServiceWorker::sendInputSampleBytes);
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

        //roleConnections_.append(connect(&videoSenderPipeline_, &VideoSenderPipeline::videoSampleBytesReady,
                //&mediaService_, &MediaService::sendVideoSampleBytes));
        roleConnections_.append(connect(videoSenderPipeline_.screenVideoSource(),
                &ScreenVideoSource::videoImageReady,
                videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::onVideoImageReady));
        roleConnections_.append(connect(videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::videoSampleBytesReady,
                mediaService_.worker(),
                &MediaServiceWorker::sendVideoSampleBytes));
        roleConnections_.append(connect(mediaService_.worker(),
                &MediaServiceWorker::keyFrameRequestReceived,
                videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::requestKeyFrame));


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
                p2pSession_.worker(),
                &P2pSessionWorker::keyFrameRequestReceived,
                videoSenderPipeline_.encoderWorker(),
                &VideoEncoderWorker::requestKeyFrame
                ));

        roleConnections_.append(connect(
                p2pSession_.worker(),
                &P2pSessionWorker::receiverReportBytesReceived,
                &rateController_,
                &RateController::onReceiverReportBytesReceived
                ));
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


        // 测试
        roleConnections_.append(connect(
                mediaService_.worker(),
                &MediaServiceWorker::videoSampleBytesReceived,
                videoRecevierPipline_.decoderWorker(),
                &VideoDecoderWorker::onVideoSampleBytesReceived));
        roleConnections_.append(connect(
                videoRecevierPipline_.decoderWorker(),
                &VideoDecoderWorker::videoImageReady,
                &videoWidget_, &VideoWidget::onVideoImageReady));

        roleConnections_.append(connect(
                videoRecevierPipline_.decoderWorker(),
                &VideoDecoderWorker::keyFrameRequestNeeded,
                mediaService_.worker(),
                &MediaServiceWorker::sendKeyFrameRequest
                ));
/*
        roleConnections_.append(connect(
                videoRecevierPipline_.decoderWorker(),
                &VideoDecoderWorker::keyFrameRequestNeeded,
                mediaService_.worker(),
                [worker = mediaService_.worker()] {
                    qDebug() << "[关键帧请求] 调用 sendKeyFrameRequest";
                    worker->sendKeyFrameRequest(QByteArray());
                },
                Qt::QueuedConnection));*/

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
                p2pSession_.worker(),
                [worker = p2pSession_.worker()] (const QByteArray& bytes) {
                    worker->sendControlFrame(UdpFrameType::ReceiverReport, bytes);
                },
                Qt::QueuedConnection));
    }
}

void ClientApp::clearRoleConnections() {
    for (const QMetaObject::Connection& connection : roleConnections_) {
        disconnect(connection);
    }

    roleConnections_.clear();

}
