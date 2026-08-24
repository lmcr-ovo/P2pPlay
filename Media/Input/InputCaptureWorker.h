//
// Created by ASUS on 2026/8/14.
//

#ifndef P2PPLAY_INPUTCAPTUREWORKER_H
#define P2PPLAY_INPUTCAPTUREWORKER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include "InputSample.h"

class InputCaptureWorker : public QObject {
    Q_OBJECT

public:
    explicit InputCaptureWorker(QObject* parent = nullptr);
    ~InputCaptureWorker() override;

    bool handleKeyboardEvent(quint32 vk, bool pressed);

public slots:
    bool start();
    void stop();

    // 窗口被选中，钩子生效
    void setControlActive(bool active);
    void setKeyMapping(quint32 fromVk, quint32 toVk);
    void clearKeyMappings();

signals:
    void inputRawSampleReady(const InputSample& sample);
    void errorOccurred(const QString& reason);
    void logReceived(const QString& message);

private:
    quint32 mapKey(quint32 vk, bool pressed);

private:
    void* keyboardHook_ = nullptr;

    bool controlActive_ = false;
    bool blockLocalInput_ = true;

    QHash<quint32, quint32> keyMappings_;
    QHash<quint32, quint32> activeMappings_;
    QSet<quint32> pressedKeys_;
};


#endif //P2PPLAY_INPUTCAPTUREWORKER_H
