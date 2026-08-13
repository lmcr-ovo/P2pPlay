//
// Created by ASUS on 2026/8/13.
//

#include <QDateTime>
#include "InputSenderWorker.h"
#include "InputSampleCodec.h"

InputSenderWorker::InputSenderWorker(QObject* parent)
    : QObject(parent),
    timer_(this) {
    timer_.setInterval(200);
    connect(&timer_, &QTimer::timeout,
            this, &InputSenderWorker::checkRepost);
    timer_.start();
}

void InputSenderWorker::onInputRawSampleReady(const InputSample &rawSample) {
    if (!isReliableInputAction(rawSample.action)) return;

    InputSample sample = rawSample;
    sample.seq = nextSeq_++;
    sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();
    waitToBeAck_.push_back(sample);

    emit inputSampleBytesReady(InputSampleCodec::encode(sample));
}

void InputSenderWorker::onInputAckSampleBytesReady(const QByteArray& bytes) {
    InputSample sample;
    if (!InputSampleCodec::decode(bytes, sample)) {
        return;
    }
    if (!isReliableInputAction(sample.action)
        || sample.kind != InputSampleKind::Ack) {
        return;
    }

    quint32 ackSeq = sample.ackSeq;
    while (!waitToBeAck_.isEmpty()
        && waitToBeAck_.front().seq <= ackSeq) {
        waitToBeAck_.dequeue();
    }
}

void InputSenderWorker::checkRepost() {
    quint16 checked = 0;
    quint64 currTimeMs = QDateTime::currentMSecsSinceEpoch();
    for (const auto& sample : waitToBeAck_) {
        if (checked >= checkPerTick_) {
            return;
        }

        checked += 1;
        if (sample.timeStampMs < currTimeMs
            && currTimeMs - sample.timeStampMs > expireMs_) {
            emit inputSampleBytesReady(InputSampleCodec::encode(sample));
        }
    }
}