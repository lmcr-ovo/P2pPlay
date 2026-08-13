//
// Created by ASUS on 2026/8/13.
//

#include <QDateTime>
#include <QDebug>
#include "InputReceiverWorker.h"
#include "InputSample.h"
#include "InputSampleCodec.h"
#include "InputInjector.h"

InputReceiverWorker::InputReceiverWorker(QObject* parent)
    : QObject(parent) {
}


void InputReceiverWorker::onInputSampleBytesReady(const QByteArray &bytes) {
    qDebug() << "recved input sample";
    InputSample sample;
    if (!InputSampleCodec::decode(bytes, sample)) {
        return;
    }
    qDebug() << "kind " << static_cast<quint8>(sample.kind);
    qDebug() << "device" << static_cast<quint8>(sample.device);

    if (sample.kind != InputSampleKind::Request) {
        return;
    }

    if (sample.seq < expectedSeq) {
        // 重复包，不执行，但最好回 ACK
        sendAck(expectedSeq - 1);
        return;
    }

    if (sample.seq > expectedSeq) {
        // 中间缺包，不能执行当前包
        sendAck(expectedSeq == 0 ? 0 : expectedSeq - 1);
        return;
    }

    switch (sample.device) {
        case InputDevice::Keyboard : {
            handleKeyBoard(sample);
            expectedSeq += 1;
            sendAck(expectedSeq - 1);
            break;
        }
        default:
            break;
    }
}

void InputReceiverWorker::handleKeyBoard(const InputSample &sample) {
    qDebug() << "recv " << sample.vk;
    qDebug() << sample.seq;
    if (sample.action == InputAction::KeyDown) {
        InputInjector::sendKeyDown(sample.vk);
    }
    if (sample.action == InputAction::KeyUp) {
        InputInjector::sendKeyUp(sample.vk);
    }
}

void InputReceiverWorker::sendAck(const quint32 ackSeq) {
    InputSample ackSample;
    ackSample.kind = InputSampleKind::Ack;
    ackSample.ackSeq = ackSeq;
    emit inputAckSampleBytesReady(InputSampleCodec::encode(ackSample));
}