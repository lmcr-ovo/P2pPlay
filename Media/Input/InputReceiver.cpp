//
// Created by ASUS on 2026/8/13.
//

#include <QDateTime>
#include <QDebug>
#include "InputReceiver.h"
#include "InputSample.h"
#include "InputSampleCodec.h"
#include "InputInjector.h"

InputReceiver::InputReceiver(QObject* parent)
    : QObject(parent) {
}


void InputReceiver::onInputSampleBytesReady(const QByteArray &bytes) {
    InputSample sample;
    if (!InputSampleCodec::decode(bytes, sample)) {
        return;
    }

    if (sample.kind != InputSampleKind::Request) {
        return;
    }

    // MouseMove 为高频不可靠事件，直接执行
    if (!isReliableInputAction(sample.action)) {
        handleSample(sample);
        return;
    }

    if (sample.seq < expectedSeq_) {
        // 重复包，回复最近一次已执行序号
        sendAck(expectedSeq_ - 1);
        return;
    }

    if (sample.seq == expectedSeq_) {
        handleSample(sample);
        ++expectedSeq_;
        sendAck(expectedSeq_ - 1);
        tryExecutePending();
        return;
    }

    if (sample.seq > expectedSeq_) {
        // 中间缺包，不能执行当前包
        pendingSamples_.insert(sample.seq, sample);
        sendAck(expectedSeq_ == 0 ? 0 : expectedSeq_ - 1);
        return;
    }
}

void InputReceiver::handleKeyBoard(const InputSample &sample) {
    if (sample.action == InputAction::KeyDown) {
        InputInjector::sendKeyDown(sample.vk);
    }
    if (sample.action == InputAction::KeyUp) {
        InputInjector::sendKeyUp(sample.vk);
    }
}

void InputReceiver::handleMouse(const InputSample &sample) {
    switch (sample.action) {
        case InputAction::MouseMove:
            InputInjector::sendMouseMove(
                    sample.x,
                    sample.y
            );
            break;

        case InputAction::MouseDown:
            InputInjector::sendMouseDown(
                    sample.mouseButton
            );
            break;

        case InputAction::MouseUp:
            InputInjector::sendMouseUp(
                    sample.mouseButton
            );
            break;

        case InputAction::MouseWheel:
            InputInjector::sendMouseWheel(
                    sample.wheelDelta
            );
            break;

        default:
            break;
    }
}

void InputReceiver::handleSample(const InputSample &sample) {
    switch (sample.device) {
        case InputDevice::Keyboard:
            handleKeyBoard(sample);
            break;

        case InputDevice::Mouse:
            handleMouse(sample);
            break;

        default:
            break;
    }
}

void InputReceiver::sendAck(const quint32 ackSeq) {
    InputSample ackSample;
    ackSample.kind = InputSampleKind::Ack;
    ackSample.ackSeq = ackSeq;
    emit inputAckSampleBytesReady(InputSampleCodec::encode(ackSample));
}

void InputReceiver::tryExecutePending() {
    bool exist = false;
    while (!pendingSamples_.isEmpty()
        && pendingSamples_.firstKey() == expectedSeq_) {
        exist = true;
        qDebug() << QString("[输入][host] 清除input缓存 seq = %1")
            .arg(expectedSeq_);
        handleKeyBoard(pendingSamples_.first());
        pendingSamples_.erase(pendingSamples_.begin());
        expectedSeq_ += 1;
    }
    if (exist) {
        sendAck(expectedSeq_ - 1);
    }
}