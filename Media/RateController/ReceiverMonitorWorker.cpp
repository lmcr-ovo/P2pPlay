//
// Created by ASUS on 2026/8/20.
//

#include "ReceiverMonitorWorker.h"
#include "ReceiverReportSample.h"
#include "Video/VideoSampleCodec.h"

ReceiverMonitorWorker::ReceiverMonitorWorker(QObject* parent)
    : QObject(parent),
    timer_(this) {
    connect(&timer_, &QTimer::timeout,
            this, &ReceiverMonitorWorker::onSettleTick);
    // 不在这里启动，等 applyConfig 设置好间隔后由它启动
}

ReceiverMonitorWorker::~ReceiverMonitorWorker() {
    timer_.stop();
}

void ReceiverMonitorWorker::applyConfig(const AppConfig &config) {
    monitorPeriod_ = config.video.monitorPeriod;

    timer_.setInterval(monitorPeriod_);
    if (!timer_.isActive()) {
        timer_.start();
    }
}

void ReceiverMonitorWorker::onVideoSampleBytes(const QByteArray& bytes) {
    quint32 seq = 0;
    if (!VideoSampleCodec::peekVideoSeq(bytes, seq)) {
        return;
    }
    if (!hasFirstSeq_) {
        seqMin_ = seq;
        hasFirstSeq_ = true;
    }
    seqMax_ = seq;
    receivedCount_++;
}

void ReceiverMonitorWorker::onSettleTick() {   // QTimer::timeout，间隔 τ
    if (!hasFirstSeq_) {
        return;   // 本周期没收到帧，不发报告
    }

    const quint32 expected = seqMax_ - seqMin_ + 1;   // 期望帧数（自动处理回绕）
    const quint32 lost = expected - receivedCount_;   // 丢失帧数

    ReceiverReportSample report;
    report.expectFrames = expected;
    report.receivedFrames = receivedCount_;

    qDebug() << QString("[自适应][guest] 发报告 expected=%1 received=%2 丢失=%3 丢帧率=%4%")
                .arg(expected).arg(receivedCount_).arg(lost)
                .arg(lost * 100.0 / expected, 0, 'f', 2);

    emit reportFrameBytesReady(ReceiverReportSampleCodec::encode(report));  // 发回 host

    // 清零重计
    hasFirstSeq_ = false;
    receivedCount_ = 0;
}