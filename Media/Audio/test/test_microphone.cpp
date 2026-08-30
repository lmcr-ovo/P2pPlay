//
// Created by ASUS on 2026/8/29.
//
// ffmpeg -f s16le -ar 48000 -ac 1 -i mic_out.raw mic_out.wav
#include <QCoreApplication>
#include <QAudioInput>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QFile>
#include <QDebug>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(1);
    fmt.setSampleSize(16);
    fmt.setCodec("audio/pcm");
    fmt.setByteOrder(QAudioFormat::LittleEndian);
    fmt.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo dev = QAudioDeviceInfo::defaultInputDevice();
    if (!dev.isFormatSupported(fmt))
    {
        qWarning() << "设备不支持该格式，尝试就近适配";
        fmt = dev.nearestFormat(fmt);
        qDebug() << fmt.sampleRate() << fmt.channelCount() << fmt.sampleSize();
    }

    QFile outFile("mic_out.raw");
    if (!outFile.open(QIODevice::WriteOnly))
    {
        qCritical() << "无法打开输出文件";
        return -1;
    }

    QAudioInput audioIn(dev, fmt);
    QIODevice* io = audioIn.start();

    // 采集5秒后退出
    QTimer stopTimer;
    stopTimer.setSingleShot(true);
    stopTimer.setInterval(5000);
    QObject::connect(&stopTimer, &QTimer::timeout, [&](){
        audioIn.stop();
        outFile.close();
        qDebug()<<"采集结束";
        a.quit();
    });
    stopTimer.start();

    // 读到数据直接写入文件
    QObject::connect(io, &QIODevice::readyRead, [&](){
        QByteArray data = io->readAll();
        outFile.write(data);
    });

    qDebug()<<"开始采集麦克风，5秒后结束...";
    return a.exec();
}
