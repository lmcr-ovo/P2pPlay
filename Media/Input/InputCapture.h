//
// Created by ASUS on 2026/8/14.
//

#ifndef P2PPLAY_INPUTCAPTURE_H
#define P2PPLAY_INPUTCAPTURE_H

#include <QObject>
#include <QThread>
#include "InputCaptureWorker.h"

class InputCapture : public QObject {
Q_OBJECT

public:
    explicit InputCapture(QObject* parent = nullptr);
    ~InputCapture() override;

    InputCaptureWorker* worker() const;

public slots:
    void start();
    void stop();

private:
    QThread* thread_ = nullptr;
    InputCaptureWorker* worker_ = nullptr;
};

#endif // P2PPLAY_INPUTCAPTURE_H