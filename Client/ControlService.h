//
// Created by ASUS on 2026/8/25.
//

#ifndef P2PPLAY_CONTROLSERVICE_H
#define P2PPLAY_CONTROLSERVICE_H

#include <QObject>
#include <QThread>

#include "Role.h"
#include "ControlServiceWorker.h"

class ControlChannelService : public QObject {
Q_OBJECT

public:
    explicit ControlChannelService(QObject* parent = nullptr);
    ~ControlChannelService() override;

    ControlServiceWorker* worker() const;

    void setRole(Role role);
    Role role() const;

    void start();
    void stop();
    bool isRunning() const;

signals:
    void logReceived(const QString& message);
    void errorOccurred(const QString& reason);

private:
    QThread* thread_ = nullptr;
    ControlServiceWorker* worker_ = nullptr;
};

#endif
