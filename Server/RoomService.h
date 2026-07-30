//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_ROOMSERVICE_H
#define P2PPLAY_ROOMSERVICE_H

#include <QObject>
#include "RoomStore.h"

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

private:
    QSet<QString> idTable_;
    QHash<QString, Room> roomsById_;
    QHash<QString, QString> roomIdByClientId_;
    QHash<SignalingConnection*, QString> clientIdByConnection_;

    QString generateRoomId() const;
};


#endif //P2PPLAY_ROOMSERVICE_H
