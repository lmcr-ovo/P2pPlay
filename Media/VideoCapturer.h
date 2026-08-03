//
// Created by ASUS on 2026/8/2.
//

#ifndef P2PPLAY_VIDEOCAPTURER_H
#define P2PPLAY_VIDEOCAPTURER_H

#include <QObject>
#include <QTimer>
#include <QHostAddress>
#include "AppConfig.h"

class VideoCapturer : public QObject {
    Q_OBJECT
public:
    explicit VideoCapturer(QObject* parent);
    void applyConfig(const VideoConfig& config);

signals:
    void videoFrameReady(const QByteArray& payload);

public slots:
    void onP2pReady(const QHostAddress& address, quint16 port);

private:
    QTimer timer_;
    int frameIntervalMs_ = 20;
    int width_ = 1280;
    int height_ = 720;
    int jpegQuality_ = 50;
};


#endif //P2PPLAY_VIDEOCAPTURER_H
