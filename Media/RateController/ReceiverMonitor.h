//
// Created by ASUS on 2026/8/20.
//

#ifndef P2PPLAY_RECEIVERMONITOR_H
#define P2PPLAY_RECEIVERMONITOR_H

#include <QObject>
#include "AppConfig.h"
#include "ReceiverMonitorWorker.h"

class ReceiverMonitor : public QObject {
    Q_OBJECT
public:
    explicit ReceiverMonitor(QObject* parent);
    ~ReceiverMonitor() override;
    void applyConfig(const AppConfig& config);
    ReceiverMonitorWorker* worker() const;

private:
    QThread* thread_ = nullptr;
    ReceiverMonitorWorker* worker_ = nullptr;
};

#endif //P2PPLAY_RECEIVERMONITOR_H
