#include "P2pUdpTransport.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QMetaObject>
#include <QUdpSocket>

#include <iostream>
#include <limits>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    quint16 localPort = 0;
    quint16 peerPort = 0;

    std::cout << "local port: ";
    std::cin >> localPort;

    std::cout << "peer port: ";
    std::cin >> peerPort;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    QUdpSocket socket;

    if (!socket.bind(QHostAddress::LocalHost, localPort)) {
        qDebug() << "bind failed:" << socket.errorString();
        return 1;
    }

    P2pUdpTransport transport(&socket, &app);
    transport.setPeerEndpoint(QHostAddress::LocalHost, peerPort);

    QObject::connect(&transport, &P2pUdpTransport::frameReady,
                     &app,
                     [](const UdpFrame& frame) {
                         qDebug() << "[recv]"
                                  << "channel =" << static_cast<quint16>(frame.channelType)
                                  << "type =" << static_cast<quint16>(frame.frameType)
                                  << "size =" << frame.payload.size()
                                  << "payload =" << QString::fromUtf8(frame.payload);
                     });

    QObject::connect(&transport, &P2pUdpTransport::frameDropped,
                     &app,
                     [](quint32 frameSeq) {
                         qDebug() << "[drop] frameSeq =" << frameSeq;
                     });

    QObject::connect(&transport, &P2pUdpTransport::errorOccurred,
                     &app,
                     [](const QString& reason) {
                         qDebug() << "[error]" << reason;
                     });

    qDebug() << "UDP test started.";
    qDebug() << "Type message and press Enter.";
    qDebug() << "Type quit to exit.";

    std::thread inputThread([&transport]() {
        while (true) {
            std::cout << "> " << std::flush;

            std::string line;
            if (!std::getline(std::cin, line)) {
                break;
            }

            QMetaObject::invokeMethod(
                    &transport,
                    [&transport, line]() {
                        if (line == "quit") {
                            QCoreApplication::quit();
                            return;
                        }

                        const QByteArray payload = QByteArray::fromStdString(line);

                        const bool ok = transport.sendFrame(
                                UdpChannelType::Media,
                                UdpFrameType::InputEvent,
                                payload
                        );

                        if (!ok) {
                            qDebug() << "send failed";
                        }
                    },
                    Qt::QueuedConnection);
        }
    });
    inputThread.detach();

    return app.exec();
}
