//
// Created by ASUS on 2026/8/13.
//

#ifndef P2PPLAY_INPUTSENDER_H
#define P2PPLAY_INPUTSENDER_H

#include <QObject>
#include <QThread>
#include "InputSenderWorker.h"

class InputSender : public QObject {
    Q_OBJECT
public:
    explicit InputSender(QObject* parent);
    ~InputSender() override;
    InputSenderWorker* worker() const;

private:
    QThread* thread_ = nullptr;
    InputSenderWorker* worker_ = nullptr;
};


#endif //P2PPLAY_INPUTSENDER_H
