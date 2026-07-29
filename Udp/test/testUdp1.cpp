#include "P2pUdpTransport.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>

#include <iostream>
#include <limits>
#include <string>

namespace {

QByteArray makeFilePayload(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "open file failed:" << file.errorString();
        return QByteArray();
    }

    const QFileInfo info(filePath);
    const QByteArray fileBytes = file.readAll();

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << info.fileName();
    out << fileBytes;
    return payload;
}

bool restoreFilePayload(const QByteArray& payload, QString* savedPath) {
    QDataStream in(payload);
    in.setByteOrder(QDataStream::BigEndian);

    QString fileName;
    QByteArray fileBytes;
    in >> fileName;
    in >> fileBytes;

    if (in.status() != QDataStream::Ok || fileName.isEmpty()) {
        qWarning() << "invalid file payload";
        return false;
    }

    const QString safeName = QFileInfo(fileName).fileName();
    const QString outputName = QString("recv_%1_%2")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
            .arg(safeName);
    const QString outputPath = QDir::current().absoluteFilePath(outputName);

    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly)) {
        qWarning() << "create output failed:" << output.errorString();
        return false;
    }

    if (output.write(fileBytes) != fileBytes.size()) {
        qWarning() << "write output failed:" << output.errorString();
        return false;
    }

    if (savedPath != nullptr) {
        *savedPath = outputPath;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    quint16 localPort = 0;
    quint16 peerPort = 0;

    std::cout << "local port: ";
    std::cin >> localPort;

    std::cout << "peer port: ";
    std::cin >> peerPort;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << "file path(empty means receive only): ";
    std::string filePathInput;
    std::getline(std::cin, filePathInput);

    QUdpSocket socket;
    socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                           8 * 1024 * 1024);
    if (!socket.bind(QHostAddress::LocalHost, localPort)) {
        qDebug() << "bind failed:" << socket.errorString();
        return 1;
    }

    P2pUdpTransport transport(&socket, &app);
    transport.setPeerEndpoint(QHostAddress::LocalHost, peerPort);

    QObject::connect(&transport, &P2pUdpTransport::frameReady,
                     &app,
                     [](const UdpFrame& frame) {
                         if (frame.channelType != UdpChannelType::Media
                             || frame.frameType != UdpFrameType::VideoFrame) {
                             qDebug() << "ignored frame"
                                      << static_cast<quint16>(frame.channelType)
                                      << static_cast<quint16>(frame.frameType);
                             return;
                         }

                         QString savedPath;
                         if (restoreFilePayload(frame.payload, &savedPath)) {
                             qDebug() << "file restored:" << savedPath;
                         }
                     });

    QObject::connect(&transport, &P2pUdpTransport::frameDropped,
                     &app,
                     [](quint32 frameSeq) {
                         qDebug() << "frame dropped:" << frameSeq;
                     });

    QObject::connect(&transport, &P2pUdpTransport::errorOccurred,
                     &app,
                     [](const QString& reason) {
                         qDebug() << "udp error:" << reason;
                     });

    const QString filePath = QString::fromLocal8Bit(filePathInput.c_str()).trimmed();
    if (!filePath.isEmpty()) {
        QTimer::singleShot(500, &app, [&transport, filePath]() {
            const QByteArray payload = makeFilePayload(filePath);
            if (payload.isEmpty()) {
                qDebug() << "nothing to send";
                return;
            }

            const bool ok = transport.sendFrame(
                    UdpChannelType::Media,
                    UdpFrameType::VideoFrame,
                    payload);
            qDebug() << "send file" << (ok ? "ok" : "failed")
                     << "payload size:" << payload.size();
        });
    }

    qDebug() << "UDP file test started.";
    return app.exec();
}
