//
// Created by ASUS on 2026/8/20.
//

#ifndef P2PPLAY_RATECONTROLLER_H
#define P2PPLAY_RATECONTROLLER_H

#include <QObject>
#include <QTimer>
#include "AppConfig.h"

class RateController : public QObject {
    Q_OBJECT
public:
    explicit RateController(QObject* parent);
    void applyConfig(const AppConfig& config);

signals:
    void targetBitrateChanged(double rate);

public slots:
    void onSendBlock();
    void onReceiverReportBytesReceived(const QByteArray& bytes);

private slots:
    void onProbeTick(); // 检查是否长时间空闲

private:
    QTimer timer_;
    double upRate_ = 1.1;
    double downRate_ = 0.7;

    double lossLow_ = 0.01;
    double lossHigh_ = 0.03;

    quint64 coolPeriodMs_ = 2000;   // 冷却期 T_cool
    quint64 idlePeriodMs_ = 3000;   // 空闲期 T_idle

    bool cooling_ = false;           // 冷却中
    quint64 coolBeginMs_ = 0;        // 冷却开始时刻
    quint64 lastBlockedMs_ = 0;      // 上次 10035 时刻（空闲检测）
    double lastLoss_ = 0.0;          // 最近一次丢帧率
    bool hasLoss_ = false;           // 是否收到过报告
};


#endif //P2PPLAY_RATECONTROLLER_H
