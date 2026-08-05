#include <QApplication>
#include <QDebug>

#include "ClientApp.h"

namespace {

void printUsage() {
    qDebug().noquote()
            << "usage:\n"
            << "  testV host\n"
            << "  testV guest <roomId>";
}

}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QStringList args = app.arguments();

    if (args.size() < 2) {
        printUsage();
        return 1;
    }

    const QString role = args.at(1).toLower();

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
        AppConfig config = AppConfig::defaultHost();
        const QString clientId = "Alice";

        started = client.startAsHost(clientId, config);

        qDebug().noquote() << "media host started, clientId:" << clientId;
    } else if (role == "guest") {
        if (args.size() < 3) {
            printUsage();
            return 1;
        }

        const QString roomId = args.at(2);
        AppConfig config = AppConfig::defaultGuest();
        const QString clientId = "Bob";

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

    return app.exec();
}
