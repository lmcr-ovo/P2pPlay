//
// Created by ASUS on 2026/8/3.
//

#ifndef P2PPLAY_APPCONFIG_H
#define P2PPLAY_APPCONFIG_H

#include <QString>
#include <QHostAddress>
#include "Video/VideoSample.h"

struct ServerConfig {
    QHostAddress tcpAddress = QHostAddress("118.195.142.123");
    //QHostAddress tcpAddress = QHostAddress(QHostAddress::LocalHost);
    quint16 tcpPort = 9000;

    QHostAddress udpAddress = QHostAddress("118.195.142.123");
    //QHostAddress udpAddress = QHostAddress(QHostAddress::LocalHost);
    quint16 udpPort = 9001;
};

struct P2pConfig {
    quint16 localUdpPort = 10000;

    quint16 punchPortRange = 32;
    int punchIntervalMs = 200;

    int udpPacketsPerTick = 2;
    int udpFlushIntervalMs = 1;
};

struct VideoConfig {
    quint16 fps = 100;

    quint16 width = 1280;
    quint16 height = 720;

    quint16 jpegQuality = 50;
    VideoSampleCodecType codecType = VideoSampleCodecType::H264;

    int h264BitrateKbps = 1400;
    int h264GopFrames = 15;
    int h264EncoderThreads = 1;
    QString h264Preset = "ultrafast";
    QString h264Tune = "zerolatency";
    QString h264Profile = "baseline";
    bool h264RepeatHeaders = true;
    bool h264ForceIdr = true;
    int h264KeyFrameRequestIntervalMs = 200;

    quint16 frameIntervalMs() const {
        return fps > 0 ? 1000 / fps : 20;
    }
};

struct AppConfig {
    ServerConfig server;
    P2pConfig p2p;
    VideoConfig video;

    static AppConfig defaultHost();
    static AppConfig defaultGuest();
};


#endif //P2PPLAY_APPCONFIG_H
