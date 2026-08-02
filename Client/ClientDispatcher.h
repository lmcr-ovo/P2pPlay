#ifndef P2PPLAY_CLIENTDISPATCHER_H
#define P2PPLAY_CLIENTDISPATCHER_H

#include <QObject>

#include "SignalingMessage.h"

class ClientDispatcher : public QObject {
Q_OBJECT

public:
    explicit ClientDispatcher(QObject* parent = nullptr);

signals:
    void roomCreated(const SignalingMessage& message);
    void peerJoined(const SignalingMessage& message);
    void probePermitted(const SignalingMessage& message);
    void peerEndpoint(const SignalingMessage& message);
    void logReceived(const SignalingMessage& message);
    void errorReceived(const SignalingMessage& message);
    void unknownMessage(const SignalingMessage& message);

public slots:
    void onMessageReceived(const SignalingMessage& message);
};

#endif // P2PPLAY_CLIENTDISPATCHER_H