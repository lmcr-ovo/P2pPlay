#include "SignalingConnection.h"

#include <iostream>
#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

namespace {

bool isCreateRoomMessage(const SignalingMessage& message) {
    return message.type == SignalingType::CreateRoom
           && message.roomId == "room_001"
           && message.clientId == "host_001";
}

bool isRoomCreatedMessage(const SignalingMessage& message) {
    return message.type == SignalingType::RoomCreated
           && message.roomId == "room_001"
           && message.clientId == "server"
           && message.success;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    QTcpServer server;
    QTcpSocket clientSocket;

    SignalingConnection* serverConnection = nullptr;
    SignalingConnection* clientConnection = nullptr;

    bool serverReceivedCreateRoom = false;
    bool clientReceivedRoomCreated = false;

    QObject::connect(&server, &QTcpServer::newConnection,
                     &app,
                     [&]() {
                         QTcpSocket* socket = server.nextPendingConnection();
                         serverConnection = new SignalingConnection(socket, &app);
                         socket->setParent(serverConnection);

                         QObject::connect(serverConnection, &SignalingConnection::messageReceived,
                                          &app,
                                          [&](const SignalingMessage& message) {
                                              qDebug() << "[server recv]"
                                                       << static_cast<quint16>(message.type)
                                                       << message.roomId
                                                       << message.clientId;

                                              if (!isCreateRoomMessage(message)) {
                                                  qDebug() << "server received unexpected message";
                                                  QCoreApplication::exit(2);
                                                  return;
                                              }

                                              serverReceivedCreateRoom = true;

                                              SignalingMessage response;
                                              response.type = SignalingType::RoomCreated;
                                              response.roomId = message.roomId;
                                              response.clientId = "server";
                                              response.success = true;

                                              if (!serverConnection->sendMessage(response)) {
                                                  qDebug() << "server send RoomCreated failed";
                                                  QCoreApplication::exit(3);
                                              }
                                          });

                         QObject::connect(serverConnection, &SignalingConnection::errorOccurred,
                                          &app,
                                          [](const QString& reason) {
                                              qDebug() << "[server error]" << reason;
                                              QCoreApplication::exit(4);
                                          });
                     });

    if (!server.listen(QHostAddress::LocalHost, 0)) {
        qDebug() << "server listen failed:" << server.errorString();
        return 1;
    }

    clientConnection = new SignalingConnection(&clientSocket, &app);

    QObject::connect(clientConnection, &SignalingConnection::messageReceived,
                     &app,
                     [&](const SignalingMessage& message) {
                         qDebug() << "[client recv]"
                                  << static_cast<quint16>(message.type)
                                  << message.roomId
                                  << message.clientId
                                  << message.success;

                         if (!isRoomCreatedMessage(message)) {
                             qDebug() << "client received unexpected message";
                             QCoreApplication::exit(5);
                             return;
                         }

                         clientReceivedRoomCreated = true;

                         if (serverReceivedCreateRoom && clientReceivedRoomCreated) {
                             qDebug() << "tcp signaling test passed";
                             QCoreApplication::quit();
                         }
                     });

    QObject::connect(clientConnection, &SignalingConnection::errorOccurred,
                     &app,
                     [](const QString& reason) {
                         qDebug() << "[client error]" << reason;
                         QCoreApplication::exit(6);
                     });

    QObject::connect(&clientSocket, &QTcpSocket::connected,
                     &app,
                     [&]() {
                         qDebug() << "client connected";

                         SignalingMessage request;
                         request.type = SignalingType::CreateRoom;
                         request.roomId = "room_001";
                         request.clientId = "host_001";

                         if (!clientConnection->sendMessage(request)) {
                             qDebug() << "client send CreateRoom failed";
                             QCoreApplication::exit(7);
                         }
                     });

    QTimer::singleShot(5000, &app, [&]() {
        qDebug() << "tcp signaling test timeout"
                 << "serverReceivedCreateRoom =" << serverReceivedCreateRoom
                 << "clientReceivedRoomCreated =" << clientReceivedRoomCreated;
        QCoreApplication::exit(8);
    });

    clientSocket.connectToHost(QHostAddress::LocalHost, server.serverPort());

    return app.exec();
}

