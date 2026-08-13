//
// Created by ASUS on 2026/8/13.
//

#ifndef P2PPLAY_INPUTRECEIVERWORKER_H
#define P2PPLAY_INPUTRECEIVERWORKER_H

#include <QObject>
#include "InputSample.h"

class InputReceiverWorker : public QObject {
    Q_OBJECT
public:
    explicit InputReceiverWorker(QObject* parent = nullptr);

signals:
    void inputAckSampleBytesReady(const QByteArray& bytes);

public slots:
    void onInputSampleBytesReady(const QByteArray& bytes);

private:
    static void handleKeyBoard(const InputSample& sample);
    void sendAck(const quint32 ackSeq);

private:
    quint32 expectedSeq = 0;
};


#endif //P2PPLAY_INPUTRECEIVERWORKER_H
