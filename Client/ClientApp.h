//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_CLIENTAPP_H
#define P2PPLAY_CLIENTAPP_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include "Role.h"
#include "AppConfig.h"
#include "SignalingClient.h"
#include "ClientDispatcher.h"
#include "P2pSession.h"
#include "HostRoleService.h"
#include "GuestRoleService.h"
#include "MediaService.h"
#include "ControlService.h"
#include "Video/VideoSenderPipeline.h"
#include "Video/VideoReceiverPipeline.h"
#include "VideoWidget.h"
#include "Input/InputCapture.h"
#include "Input/InputSender.h"
#include "Input/InputReceiver.h"
#include "Input/InputService.h"
#include "RateController/RateController.h"
#include "RateController/ReceiverMonitor.h"

class ClientApp : public QObject {
    Q_OBJECT
public:
    explicit ClientApp();

    // 提供给测试或ui的接口
    bool startAsHost(const QString& clientId,
                     const QHostAddress& serverTcpAddress,
                     quint16 serverTcpPort,
                     const QHostAddress& serverUdpAddress,
                     quint16 serverUdpPort,
                     quint16 localUdpPort);

    bool startAsGuest(const QString& clientId,
                      const QString& roomId,
                      const QHostAddress& serverTcpAddress,
                      quint16 serverTcpPort,
                      const QHostAddress& serverUdpAddress,
                      quint16 serverUdpPort,
                      quint16 localUdpPort);

    // 新增接口
    bool startAsHost(const QString& clientId, const AppConfig& config);

    bool startAsGuest(const QString& clientId,
                      const QString& roomId,
                      const AppConfig& config);
signals:
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    void connectCommonSignals();
    void connectRoleSignals();
    void clearRoleConnections();

private:
    Role role_ = Role::Unknown;
    QList<QMetaObject::Connection> roleConnections_;

    SignalingClient signalingClient_;
    ClientDispatcher dispatcher_;
    P2pSession p2pSession_;

    HostRoleService hostRoleService_;
    GuestRoleService guestRoleService_;

    MediaService mediaService_;
    ControlService controlService_;

    VideoSenderPipeline videoSenderPipeline_;
    VideoReceiverPipeline videoRecevierPipline_;
    VideoWidget videoWidget_;

    InputService inputService_;

    RateController rateController_;
    ReceiverMonitor receiverMonitor_;
};


#endif //P2PPLAY_CLIENTAPP_H
