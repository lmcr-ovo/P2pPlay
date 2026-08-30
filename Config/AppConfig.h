//
// Created by ASUS on 2026/8/3.
//

#ifndef P2PPLAY_APPCONFIG_H
#define P2PPLAY_APPCONFIG_H

#include <QString>
#include <QHostAddress>
#include "Video/VideoSample.h"

struct ServerConfig {
    QHostAddress tcpAddress = QHostAddress("118.25.51.231");
    //QHostAddress tcpAddress = QHostAddress(QHostAddress::LocalHost);
    quint16 tcpPort = 9000;

    QHostAddress udpAddress = QHostAddress("118.25.51.231");
    //QHostAddress udpAddress = QHostAddress(QHostAddress::LocalHost);
    quint16 udpPort = 9001;
};

struct P2pConfig {
    quint16 localUdpPort = 10000;

    quint16 punchPortRange = 32;
    int punchIntervalMs = 200;

    int udpPacketsPerTick = 64;
    int udpFlushIntervalMs = 1;

    quint16 kPace = 30; // 发送速率 = kPace × R
};

struct VideoConfig {
    quint16 fps = 60;

    quint16 width = 1920;
    quint16 height = 1080;

    quint16 jpegQuality = 50;
    VideoSampleCodecType codecType = VideoSampleCodecType::H264;

    int h264BitrateKbps = 2500;
    int h264GopFrames = 5 * fps;
    int h264EncoderThreads = 1;
    //QString h264Preset = "ultrafast";
    QString h264Preset = "veryfast";
    QString h264Tune = "zerolatency";
    QString h264Profile = "baseline";
    bool h264RepeatHeaders = true;
    bool h264ForceIdr = true;
    int h264KeyFrameRequestIntervalMs = 50;

    double bppMin = 0.06;
    double bppMax = 0.2;

    // 丢包率
    double lossHigh = 0.1;
    double lossLow = 0.01;

    // 码率升降系数
    double upRate = 1.1;
    double downRate = 0.7;

    // 冷却时间
    quint16 coolingPeriodMs = 1000;

    double monitorPeriod = 1000;

    quint16 checkIdlePeriod = 3000;

    quint16 frameIntervalMs() const {
        return fps > 0 ? 1000 / fps : 20;
    }

    quint32 getBitrateKbpsMax() const {
        return bppMax * width * height * fps / 1000;
    }

    quint32 getBitrateKbpsMin() const {
        return bppMin * width * height * fps / 1000;
    }

};

struct OpusCodecConfig {
    int sampleRate = 48000;
    int channels = 1;
    int frameDurationMs = 20;
    int bitrateBps = 32000;
    int complexity = 5;
    bool inbandFecEnabled = false;
    bool dtxEnabled = false;
    int maxPacketBytes = 4000;

    static bool validateOpusCodecConfig(const OpusCodecConfig& config,
                                        QString* errorMessage);
};

struct AudioConfig {
    bool microphoneEnabled = true;
    bool desktopAudioEnabled = true;
    bool playbackEnabled = true;
    bool avSyncEnabled = true;

    OpusCodecConfig microphoneCodec {
            48000, 1, 20,
            32000, 5, false,
            false, 4000
    };

    OpusCodecConfig desktopCodec {
            48000, 2, 20,
            128000, 5, false,
            false, 4000
    };

    int playbackBufferMs = 80;
    double microphonePlaybackGain = 1.0;
    double desktopPlaybackGain = 0.8;
    int mixerFrameDurationMs = 20;
    int mixerMaxQueuedFramesPerSource = 10;
};

struct AppConfig {
    ServerConfig server;
    P2pConfig p2p;
    VideoConfig video;
    AudioConfig audio;

    static AppConfig defaultHost();
    static AppConfig defaultGuest();
};


#endif //P2PPLAY_APPCONFIG_H
