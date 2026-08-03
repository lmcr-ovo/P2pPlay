//
// Created by ASUS on 2026/8/1.
//

#ifndef P2PPLAY_CLIENTAPP_H
#define P2PPLAY_CLIENTAPP_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <VideoRender.h>
#include "SignalingClient.h"
#include "ClientDispatcher.h"
#include "P2pSession.h"
#include "HostRoleService.h"
#include "GuestRoleService.h"
#include "MediaService.h"
#include "VideoCapturer.h"

class ClientApp : public QObject {
    Q_OBJECT
public:
    enum class Role {
        Unknown,
        Host,
        Guest
    };

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

    VideoCapturer videoCapturer_;
    VideoRender videoRender_;
};


#endif //P2PPLAY_CLIENTAPP_H
