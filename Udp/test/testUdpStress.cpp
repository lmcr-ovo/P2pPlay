#include "P2pUdpTransport.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>

namespace {

struct TestCase {
    quint32 testId = 0;
    int payloadSize = 0;
    int packetsPerTick = 0;
    int flushIntervalMs = 1;
    int round = 0;
};

struct TestResult {
    TestCase testCase;
    qint64 sendTimeMs = 0;
    qint64 receiveTimeMs = 0;
    qint64 elapsedMs = 0;
    bool received = false;
    bool hashOk = false;
};

QVector<int> parseIntList(const QString& text, const QVector<int>& fallback) {
    if (text.trimmed().isEmpty()) {
        return fallback;
    }

    QVector<int> values;
    std::stringstream stream(text.toStdString());
    std::string item;

    while (std::getline(stream, item, ',')) {
        if (item.empty()) {
            continue;
        }
        values.append(std::max(0, std::stoi(item)));
    }

    return values.isEmpty() ? fallback : values;
}

QString argValue(const QStringList& args, const QString& name) {
    const QString prefix = name + "=";
    for (const QString& arg : args) {
        if (arg.startsWith(prefix)) {
            return arg.mid(prefix.size());
        }
    }
    return QString();
}

int intArg(const QStringList& args, const QString& name, int fallback) {
    bool ok = false;
    const int value = argValue(args, name).toInt(&ok);
    return ok ? value : fallback;
}

QByteArray makePatternBytes(int size, quint32 testId) {
    QByteArray bytes;
    bytes.resize(size);

    for (int i = 0; i < size; ++i) {
        bytes[i] = static_cast<char>((i * 131 + testId * 17 + 31) & 0xff);
    }

    return bytes;
}

QByteArray makeStressPayload(const TestCase& testCase, qint64 sendTimeMs) {
    const QByteArray data = makePatternBytes(testCase.payloadSize, testCase.testId);
    const QByteArray sha256 = QCryptographicHash::hash(data, QCryptographicHash::Sha256);

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);

    out << testCase.testId;
    out << static_cast<quint32>(testCase.payloadSize);
    out << static_cast<quint32>(testCase.packetsPerTick);
    out << static_cast<quint32>(testCase.flushIntervalMs);
    out << static_cast<quint32>(testCase.round);
    out << sendTimeMs;
    out << sha256;
    out << data;

    return payload;
}

bool parseStressPayload(const QByteArray& payload, TestResult* result) {
    QDataStream in(payload);
    in.setByteOrder(QDataStream::BigEndian);

    quint32 testId = 0;
    quint32 payloadSize = 0;
    quint32 packetsPerTick = 0;
    quint32 flushIntervalMs = 0;
    quint32 round = 0;
    QByteArray expectedSha256;
    QByteArray data;

    in >> testId;
    in >> payloadSize;
    in >> packetsPerTick;
    in >> flushIntervalMs;
    in >> round;
    in >> result->sendTimeMs;
    in >> expectedSha256;
    in >> data;

    if (in.status() != QDataStream::Ok) {
        return false;
    }

    result->testCase.testId = testId;
    result->testCase.payloadSize = static_cast<int>(payloadSize);
    result->testCase.packetsPerTick = static_cast<int>(packetsPerTick);
    result->testCase.flushIntervalMs = static_cast<int>(flushIntervalMs);
    result->testCase.round = static_cast<int>(round);
    result->receiveTimeMs = QDateTime::currentMSecsSinceEpoch();
    result->elapsedMs = result->receiveTimeMs - result->sendTimeMs;
    result->received = true;

    if (data.size() != result->testCase.payloadSize) {
        result->hashOk = false;
        return false;
    }

    const QByteArray actualSha256 =
            QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    result->hashOk = (actualSha256 == expectedSha256);
    return result->hashOk;
}

void printResults(const QVector<TestResult>& results) {
    qDebug().noquote() << "";
    qDebug().noquote() << "================ UDP stress result ================";
    qDebug().noquote()
            << QString("%1 %2 %3 %4 %5 %6 %7 %8")
               .arg("id", 4)
               .arg("ok", 5)
               .arg("size(bytes)", 12)
               .arg("tick", 6)
               .arg("intv", 6)
               .arg("round", 6)
               .arg("recv_ms", 10)
               .arg("MB/s", 8);

    QVector<TestResult> sorted = results;
    std::sort(sorted.begin(), sorted.end(), [](const TestResult& a, const TestResult& b) {
        return a.testCase.testId < b.testCase.testId;
    });

    struct Summary {
        int total = 0;
        int ok = 0;
        qint64 elapsedSum = 0;
        double mbpsSum = 0.0;
    };

    QMap<QString, Summary> summaries;

    int okCount = 0;
    for (const TestResult& result : sorted) {
        const bool ok = result.received && result.hashOk;
        if (ok) {
            ++okCount;
        }

        const double mb = result.testCase.payloadSize / 1024.0 / 1024.0;
        const double seconds = result.elapsedMs > 0 ? result.elapsedMs / 1000.0 : 0.0;
        const double mbps = seconds > 0.0 ? mb / seconds : 0.0;
        const QString summaryKey = QString("%1|%2|%3")
                .arg(result.testCase.payloadSize)
                .arg(result.testCase.packetsPerTick)
                .arg(result.testCase.flushIntervalMs);

        Summary& summary = summaries[summaryKey];
        ++summary.total;
        if (ok) {
            ++summary.ok;
            summary.elapsedSum += result.elapsedMs;
            summary.mbpsSum += mbps;
        }

        qDebug().noquote()
                << QString("%1 %2 %3 %4 %5 %6 %7 %8")
                   .arg(result.testCase.testId, 4)
                   .arg(ok ? "yes" : "no", 5)
                   .arg(result.testCase.payloadSize, 12)
                   .arg(result.testCase.packetsPerTick, 6)
                   .arg(result.testCase.flushIntervalMs, 6)
                   .arg(result.testCase.round, 6)
                   .arg(result.received ? QString::number(result.elapsedMs) : "timeout", 10)
                   .arg(result.received ? QString::number(mbps, 'f', 2) : "-", 8);
    }

    qDebug().noquote() << "success:" << okCount << "/" << sorted.size();
    qDebug().noquote() << "";
    qDebug().noquote() << "---------------- average by combination ----------------";
    qDebug().noquote()
            << QString("%1 %2 %3 %4 %5 %6")
               .arg("size(bytes)", 12)
               .arg("tick", 6)
               .arg("intv", 6)
               .arg("success", 10)
               .arg("avg_recv_ms", 12)
               .arg("avg_MB/s", 10);

    for (auto it = summaries.cbegin(); it != summaries.cend(); ++it) {
        const QStringList parts = it.key().split('|');
        const Summary& summary = it.value();
        const double avgElapsed = summary.ok > 0
                ? static_cast<double>(summary.elapsedSum) / summary.ok
                : 0.0;
        const double avgMbps = summary.ok > 0
                ? summary.mbpsSum / summary.ok
                : 0.0;

        qDebug().noquote()
                << QString("%1 %2 %3 %4 %5 %6")
                   .arg(parts.value(0), 12)
                   .arg(parts.value(1), 6)
                   .arg(parts.value(2), 6)
                   .arg(QString("%1/%2").arg(summary.ok).arg(summary.total), 10)
                   .arg(summary.ok > 0 ? QString::number(avgElapsed, 'f', 1) : "-", 12)
                   .arg(summary.ok > 0 ? QString::number(avgMbps, 'f', 2) : "-", 10);
    }

    qDebug().noquote() << "===================================================";
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();

    const quint16 senderPort = static_cast<quint16>(intArg(args, "--sender-port", 9000));
    const quint16 receiverPort = static_cast<quint16>(intArg(args, "--receiver-port", 9001));
    const int flushIntervalMs = intArg(args, "--interval", 1);
    const int rounds = intArg(args, "--rounds", 1);
    const int timeoutMs = intArg(args, "--timeout", 30000);
    const int gapMs = intArg(args, "--gap", 500);

    const QVector<int> sizes = parseIntList(
            argValue(args, "--sizes"),
            QVector<int>{128 * 1024, 512 * 1024, 1024 * 1024, 3 * 1024 * 1024});
    const QVector<int> ticks = parseIntList(
            argValue(args, "--ticks"),
            QVector<int>{8, 16, 32, 64});

    QUdpSocket senderSocket;
    QUdpSocket receiverSocket;

    senderSocket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                                 64 * 1024 * 1024);
    senderSocket.setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,
                                 64 * 1024 * 1024);
    receiverSocket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                                   64 * 1024 * 1024);
    receiverSocket.setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,
                                   64 * 1024 * 1024);

    if (!senderSocket.bind(QHostAddress::LocalHost, senderPort)) {
        qDebug() << "sender bind failed:" << senderSocket.errorString();
        return 1;
    }

    if (!receiverSocket.bind(QHostAddress::LocalHost, receiverPort)) {
        qDebug() << "receiver bind failed:" << receiverSocket.errorString();
        return 1;
    }

    P2pUdpTransport sender(&senderSocket, &app);
    P2pUdpTransport receiver(&receiverSocket, &app);
    sender.setPeerEndpoint(QHostAddress::LocalHost, receiverPort);
    receiver.setPeerEndpoint(QHostAddress::LocalHost, senderPort);

    QVector<TestCase> cases;
    quint32 testId = 0;
    for (int size : sizes) {
        for (int tick : ticks) {
            for (int round = 0; round < rounds; ++round) {
                TestCase testCase;
                testCase.testId = testId++;
                testCase.payloadSize = size;
                testCase.packetsPerTick = tick;
                testCase.flushIntervalMs = flushIntervalMs;
                testCase.round = round;
                cases.append(testCase);
            }
        }
    }

    QVector<TestResult> results;
    QSet<quint32> completedIds;
    int currentIndex = -1;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    std::function<void()> runNext;

    QObject::connect(&sender, &P2pUdpTransport::errorOccurred,
                     &app,
                     [](const QString& reason) {
                         qDebug() << "[sender error]" << reason;
                     });

    QObject::connect(&receiver, &P2pUdpTransport::errorOccurred,
                     &app,
                     [](const QString& reason) {
                         qDebug() << "[receiver error]" << reason;
                     });

    QObject::connect(&receiver, &P2pUdpTransport::frameReady,
                     &app,
                     [&](const UdpFrame& frame) {
                         if (frame.channelType != UdpChannelType::Media
                             || frame.frameType != UdpFrameType::VideoFrame) {
                             return;
                         }

                         TestResult result;
                         parseStressPayload(frame.payload, &result);

                         if (completedIds.contains(result.testCase.testId)) {
                             return;
                         }

                         completedIds.insert(result.testCase.testId);
                         results.append(result);
                         timeoutTimer.stop();

                         qDebug().noquote()
                                 << QString("[recv] id=%1 ok=%2 size=%3 tick=%4 elapsed=%5ms")
                                    .arg(result.testCase.testId)
                                    .arg(result.hashOk ? "yes" : "no")
                                    .arg(result.testCase.payloadSize)
                                    .arg(result.testCase.packetsPerTick)
                                    .arg(result.elapsedMs);

                         QTimer::singleShot(gapMs, &app, runNext);
                     });

    QObject::connect(&timeoutTimer, &QTimer::timeout,
                     &app,
                     [&]() {
                         if (currentIndex >= 0 && currentIndex < cases.size()) {
                             TestResult result;
                             result.testCase = cases[currentIndex];
                             result.received = false;
                             result.hashOk = false;
                             results.append(result);
                             completedIds.insert(result.testCase.testId);

                             qDebug().noquote()
                                     << QString("[timeout] id=%1 size=%2 tick=%3")
                                        .arg(result.testCase.testId)
                                        .arg(result.testCase.payloadSize)
                                        .arg(result.testCase.packetsPerTick);
                         }

                         QTimer::singleShot(gapMs, &app, runNext);
                     });

    runNext = [&]() {
        ++currentIndex;

        if (currentIndex >= cases.size()) {
            printResults(results);
            QCoreApplication::quit();
            return;
        }

        const TestCase testCase = cases[currentIndex];
        sender.setTick(testCase.packetsPerTick, testCase.flushIntervalMs);

        const qint64 sendTimeMs = QDateTime::currentMSecsSinceEpoch();
        const QByteArray payload = makeStressPayload(testCase, sendTimeMs);
        const bool queued = sender.sendFrame(
                UdpChannelType::Media,
                UdpFrameType::VideoFrame,
                payload);

        qDebug().noquote()
                << QString("[send] id=%1 queued=%2 size=%3 tick=%4 interval=%5")
                   .arg(testCase.testId)
                   .arg(queued ? "yes" : "no")
                   .arg(testCase.payloadSize)
                   .arg(testCase.packetsPerTick)
                   .arg(testCase.flushIntervalMs);

        timeoutTimer.start(timeoutMs);
    };

    qDebug().noquote() << "UDP stress auto test started";
    qDebug().noquote() << "senderPort:" << senderPort << "receiverPort:" << receiverPort;
    qDebug().noquote() << "sizes:" << argValue(args, "--sizes")
                       << "ticks:" << argValue(args, "--ticks")
                       << "rounds:" << rounds
                       << "timeoutMs:" << timeoutMs
                       << "gapMs:" << gapMs;
    qDebug().noquote() << "args example:";
    qDebug().noquote() << "testUdpStress.exe --sizes=131072,1048576,3145728 --ticks=8,16,32,64 --rounds=2";

    QTimer::singleShot(0, &app, runNext);
    return app.exec();
}
