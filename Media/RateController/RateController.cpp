//
// Created by ASUS on 2026/8/20.
//

#include <QDateTime>
#include "RateController.h"
#include "ReceiverReportSample.h"
RateController::RateController(QObject* parent)
    : QObject(parent),
    timer_(this) {
    connect(&timer_, &QTimer::timeout,
            this, &RateController::onProbeTick);
}

void RateController::applyConfig(const AppConfig &config) {
    upRate_ = config.video.upRate;
    downRate_ = config.video.downRate;

    lossLow_ = config.video.lossLow;
    lossHigh_ = config.video.lossHigh;

    coolPeriodMs_ = config.video.coolingPeriodMs;
    idlePeriodMs_ = config.video.checkIdlePeriod;
    if (!timer_.isActive()) {
        timer_.start(idlePeriodMs_);
    }
}

void RateController::onSendBlock() {
    quint64 now = QDateTime::currentMSecsSinceEpoch();

    /*
    if (cooling_ && now - coolBeginMs_ <= coolPeriodMs_) {
        qDebug() << "在冷却器不予处理";
        return;
    }
    */
    cooling_ = true;
    coolBeginMs_ = now;
    lastBlockedMs_ = now;

    qDebug() << QString("[自适应][host] 发送阻塞(10035) → ×%1").arg(downRate_);
    emit targetBitrateChanged(downRate_);
}

void RateController::onReceiverReportBytesReceived(const QByteArray &bytes) {
    ReceiverReportSample sample;
    if (!ReceiverReportSampleCodec::decode(bytes, &sample)) {
        return;
    }

    lastLoss_ = sample.lossRate();
    hasLoss_ = true;

    qDebug() << QString("[自适应][host] 收到报告 丢帧率=%1%")
            .arg(lastLoss_ * 100, 0, 'f', 2);

    if (lastLoss_ >= lossHigh_) {
        quint64 now = QDateTime::currentMSecsSinceEpoch();
        cooling_ = true;
        coolBeginMs_ = now;

        qDebug() << QString("[自适应][host] 丢帧率高 %1% ≥ %2% → ×%3")
                .arg(lastLoss_ * 100, 0, 'f', 2).arg(lossHigh_ * 100, 0, 'f', 1).arg(downRate_);
        emit targetBitrateChanged(downRate_);
    }
}

void RateController::onProbeTick() {
    quint64 now = QDateTime::currentMSecsSinceEpoch();

    if (cooling_ && now - coolBeginMs_ <= coolPeriodMs_) {
        return;
    }

    cooling_ = false;

    if (now - lastBlockedMs_ >= idlePeriodMs_
        && hasLoss_
        && lastLoss_ < lossLow_) {

        qDebug() << QString("[自适应][host] 空闲%1ms 丢帧率%2% < %3% → ×%4")
                .arg(idlePeriodMs_).arg(lastLoss_ * 100, 0, 'f', 2)
                .arg(lossLow_ * 100, 0, 'f', 1).arg(upRate_);
        emit targetBitrateChanged(upRate_);
        cooling_ = true;
        coolBeginMs_ = now;
    }
}