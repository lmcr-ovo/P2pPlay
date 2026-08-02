//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_ROOMSERVICE_H
#define P2PPLAY_ROOMSERVICE_H

#include <QObject>
#include "SignalingConnection.h"
#include "Room.h"

class RoomService : public QObject {
    Q_OBJECT
public:
    explicit RoomService(QObject* parent);

public slots:
    void onRegister(SignalingConnection* connection,
            const SignalingMessage& message);
    void onCreateRoom(SignalingConnection* connection,
            const SignalingMessage& message);
    void onJoinRoom(SignalingConnection* connection,
            const SignalingMessage& message);
    void onProbeRequest(SignalingConnection* connection,
            const SignalingMessage& message);
    // 保存单个用户的公网ip及port
    void onSingleUdpEndpointReady(const QString& roomId,
                                  const QString& clientId,
                                  const QHostAddress& address,
                                  quint16 port);

private:
    void tryNotifyPeerEndpoints(Room& room);

private:
    QSet<QString> idTable_;
    QHash<QString, Room> roomsById_;
    QHash<QString, QString> roomIdByClientId_;
    QHash<SignalingConnection*, QString> clientIdByConnection_;

    QString generateRoomId() const;
};


#endif //P2PPLAY_ROOMSERVICE_H
