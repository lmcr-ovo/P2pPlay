//
// Created by ASUS on 2026/8/13.
//

#ifndef P2PPLAY_INPUTSENDER_H
#define P2PPLAY_INPUTSENDER_H

#include <QObject>
#include <QTimer>
#include <QQueue>
#include "InputSample.h"

class InputSender : public QObject {
    Q_OBJECT
public:
    explicit InputSender(QObject* parent = nullptr);

signals:
    void inputSampleBytesReady(const QByteArray& bytes);

public slots:
    void onInputRawSampleReady(const InputSample& rawSample);

    // 处理ack包
    void onInputAckSampleBytesReady(const QByteArray& bytes);

private slots:
    void checkRepost();

private:
    QTimer timer_;
    QQueue<InputSample> waitToBeAck_;
    quint32 nextSeq_ = 0;
    quint16 intervalMs_ = 200;
    quint16 checkPerTick_ = 50;
    quint16 expireMs_ = 200;
};


#endif //P2PPLAY_INPUTSENDER_H
