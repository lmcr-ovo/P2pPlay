#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>

#include "ServerApp.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QStringList args = app.arguments();
    const QHostAddress tcpAddress = args.size() > 1
            ? QHostAddress(args.at(1))
            : QHostAddress::AnyIPv4;
    const quint16 tcpPort = args.size() > 2
            ? args.at(2).toUShort()
            : 9000;
    const QHostAddress udpAddress = args.size() > 3
            ? QHostAddress(args.at(3))
            : QHostAddress::AnyIPv4;
    const quint16 udpPort = args.size() > 4
            ? args.at(4).toUShort()
            : 9001;

    ServerApp server;
    QObject::connect(&server, &ServerApp::errorOccurred,
                     [](const QString& reason) {
        qDebug() << "server error:" << reason;
    });

    if (!server.start(tcpAddress, tcpPort, udpAddress, udpPort)) {
        return 1;
    }

    qDebug() << "P2P server started.";
    qDebug() << "tcp:" << tcpAddress.toString() << tcpPort;
    qDebug() << "udp:" << udpAddress.toString() << udpPort;

    return app.exec();
}
