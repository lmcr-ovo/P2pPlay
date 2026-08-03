//
// Created by ASUS on 2026/8/2.
//

#ifndef P2PPLAY_VIDEORENDER_H
#define P2PPLAY_VIDEORENDER_H

#include <QObject>
#include <QHostAddress>

class VideoRender : public QObject {
Q_OBJECT
public:
    explicit VideoRender(QObject* parent);

public slots:
    void onVideoFrameRecevied(const QByteArray& payload);
};


#endif //P2PPLAY_VIDEORENDER_H
