#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QTimer>

#include "ClientApp.h"

namespace {

void printUsage() {
    qDebug().noquote()
            << "usage:\n"
            << "  testV host  <clientId> [serverAddress] [tcpPort] [udpPort] [localUdpPort]\n"
            << "  testV guest <clientId> <roomId> [serverAddress] [tcpPort] [udpPort] [localUdpPort]";
}

quint16 toPort(const QStringList& args, int index, quint16 defaultValue) {
    if (args.size() <= index) {
        return defaultValue;
    }

    bool ok = false;
    const ushort port = args.at(index).toUShort(&ok);
    return ok ? port : defaultValue;
}

}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();

    if (args.size() < 3) {
        printUsage();
        return 1;
    }

    const QString role = args.at(1).toLower();
    const QString clientId = args.at(2);

    ClientApp client;
    QObject::connect(&client, &ClientApp::logReceived,
                     [](const QString& message) {
        qDebug().noquote() << message;
    });

    QObject::connect(&client, &ClientApp::errorOccurred,
                     [](const QString& reason) {
        if (reason == "Connection reset by peer") {
            qDebug().noquote() << "udp ignored:" << reason;
            return;
        }

        qDebug().noquote() << "client error:" << reason;
    });

    bool started = false;
    if (role == "host") {
        const QHostAddress serverAddress = args.size() > 3
                ? QHostAddress(args.at(3))
                : QHostAddress::LocalHost;
        const quint16 tcpPort = toPort(args, 4, 9000);
        const quint16 udpPort = toPort(args, 5, 9001);
        const quint16 localUdpPort = toPort(args, 6, 10000);

        AppConfig config = AppConfig::defaultHost();
        config.server.tcpAddress = serverAddress;
        config.server.udpAddress = serverAddress;
        config.server.tcpPort = tcpPort;
        config.server.udpPort = udpPort;
        config.p2p.localUdpPort = localUdpPort;

        started = client.startAsHost(clientId, config);

        qDebug().noquote() << "media host started, clientId:" << clientId;
    } else if (role == "guest") {
        if (args.size() < 4) {
            printUsage();
            return 1;
        }

        const QString roomId = args.at(3);
        const QHostAddress serverAddress = args.size() > 4
                ? QHostAddress(args.at(4))
                : QHostAddress::LocalHost;
        const quint16 tcpPort = toPort(args, 5, 9000);
        const quint16 udpPort = toPort(args, 6, 9001);
        const quint16 localUdpPort = toPort(args, 7, 10001);

        AppConfig config = AppConfig::defaultGuest();
        config.server.tcpAddress = serverAddress;
        config.server.udpAddress = serverAddress;
        config.server.tcpPort = tcpPort;
        config.server.udpPort = udpPort;
        config.p2p.localUdpPort = localUdpPort;

        started = client.startAsGuest(clientId, roomId, config);

        qDebug().noquote() << "media guest started, clientId:" << clientId
                           << "roomId:" << roomId;
    } else {
        printUsage();
        return 1;
    }

    if (!started) {
        return 1;
    }

    QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    return app.exec();
}
