//
// Created by ASUS on 2026/8/13.
//

#ifndef P2PPLAY_INPUTRECEIVER_H
#define P2PPLAY_INPUTRECEIVER_H

#include <QObject>
#include "InputSample.h"

class InputReceiver : public QObject {
    Q_OBJECT
public:
    explicit InputReceiver(QObject* parent = nullptr);

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


#endif //P2PPLAY_INPUTRECEIVER_H
