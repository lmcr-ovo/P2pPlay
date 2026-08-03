//
// Created by ASUS on 2026/8/2.
//

#ifndef P2PPLAY_VIDEOCAPTURER_H
#define P2PPLAY_VIDEOCAPTURER_H

#include <QObject>
#include <QTimer>
#include <QHostAddress>

class VideoCapturer : public QObject {
    Q_OBJECT
public:
    explicit VideoCapturer(QObject* parent);

signals:
    void videoFrameReady(const QByteArray& payload);

public slots:
    void onP2pReady(const QHostAddress& address, quint16 port);

private:
    QTimer timer_;
};


#endif //P2PPLAY_VIDEOCAPTURER_H
