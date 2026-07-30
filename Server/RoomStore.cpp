//
// Created by ASUS on 2026/7/30.
//

#include "RoomStore.h"


bool RoomStore::Rigster(const QString& clientId) {
    if (idTable_.contains(clientId)) {
        return false;
    } else {
        idTable_.insert(clientId);
        return true;
    }
}