//
// Created by ASUS on 2026/8/13.
//

#ifndef P2PPLAY_INPUTRECEIVER_H
#define P2PPLAY_INPUTRECEIVER_H

#include <QObject>
#include <QThread>
#include "InputReceiverWorker.h"

class InputReceiver : public QObject {
    Q_OBJECT
public:
    explicit InputReceiver(QObject* parent);
    ~InputReceiver() override;
    InputReceiverWorker* worker() const;

private:
    QThread* thread_ = nullptr;
    InputReceiverWorker* worker_ = nullptr;
};


#endif //P2PPLAY_INPUTRECEIVER_H
