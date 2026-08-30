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
    static void handleMouse(const InputSample& sample);
    static void handleSample(const InputSample& sample);
    void sendAck(const quint32 ackSeq);
    void tryExecutePending();

private:
    quint32 expectedSeq_ = 0;
    QMap<quint32, InputSample> pendingSamples_;
};


#endif //P2PPLAY_INPUTRECEIVER_H
