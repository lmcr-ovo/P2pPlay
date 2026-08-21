//
// Created by ASUS on 2026/8/20.
//

#ifndef P2PPLAY_RECEIVERMONITORWORKER_H
#define P2PPLAY_RECEIVERMONITORWORKER_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include "AppConfig.h"


class ReceiverMonitorWorker : public QObject {
    Q_OBJECT
public:
    explicit ReceiverMonitorWorker(QObject* parent = nullptr);
    ~ReceiverMonitorWorker() override;
    void applyConfig(const AppConfig& config);

signals:
    void reportFrameBytesReady(const QByteArray& bytes);

public slots:
    void onVideoSampleBytes(const QByteArray& bytes);

private slots:
    void onSettleTick();

private:
    QTimer timer_;

    quint16 monitorPeriod_ = 10;

    bool hasFirstSeq_ = false;
    quint32 seqMin_ = 0;
    quint32 seqMax_ = 0;
    quint16 receivedCount_ = 0;
};


#endif //P2PPLAY_RECEIVERMONITORWORKER_H
