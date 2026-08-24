//
// Created by ASUS on 2026/8/24.
//

#ifndef P2PPLAY_INPUTSERVICE_H
#define P2PPLAY_INPUTSERVICE_H

#include <QObject>
#include <QThread>
#include "AppConfig.h"
#include "InputServiceWorker.h"

class InputService : public QObject {
    Q_OBJECT
public:
    explicit InputService(QObject* parent);
    ~InputService() override;
    void applyConfig(const AppConfig& config);
    void setRole(const Role& role);
    InputServiceWorker* worker() const;

public slots:
    void start();
    void setControlActive(bool active);

private:
    QThread* thread_ = nullptr;
    InputServiceWorker* worker_ = nullptr;

    Role role_ = Role::Unknown;
};


#endif //P2PPLAY_INPUTSERVICE_H
