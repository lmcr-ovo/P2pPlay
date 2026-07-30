//
// Created by ASUS on 2026/7/30.
//

#ifndef P2PPLAY_ROOMSTORE_H
#define P2PPLAY_ROOMSTORE_H

#include <QObject>
#include <QHash>
#include <QSet>
#include "Room.h"

class RoomStore : public QObject {
public:
    bool Rigster(const QString& clientId);
    QString createRoom(const QString& hostClientId);


private:
    QSet<QString> idTable_;
    QHash<QString, Room> roomsById_;
    QHash<QString, QString> roomIdByClientId_;
    QHash<SignalingConnection*, QString> clientIdByConnection;
};


#endif //P2PPLAY_ROOMSTORE_H
