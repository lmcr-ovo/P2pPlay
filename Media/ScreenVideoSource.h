//
// Created by ASUS on 2026/8/4.
//

#ifndef P2PPLAY_SCREENVIDEOSOURCE_H
#define P2PPLAY_SCREENVIDEOSOURCE_H

#include <QObject>
#include <QTimer>
#include "AppConfig.h"
#include <QImage>
#include "TraceManager.h"

class ScreenVideoSource : public QObject {
    Q_OBJECT
public:
    explicit ScreenVideoSource(QObject* parent);
    void applyConfig(const AppConfig& config);

    void start();
    void stop();

signals:
    void videoImageReady(const QImage& img,
            quint32 sampleSeq);

private:
    void screenShot();

private:
    QTimer timer_;
    quint32 nextSampleSeq = 0;
    quint16 intervalMs_ = 50;
    quint16 width_ = 640;
    quint16 height_ = 360;
};


#endif //P2PPLAY_SCREENVIDEOSOURCE_H
