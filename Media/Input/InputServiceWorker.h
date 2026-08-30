//
// Created by ASUS on 2026/8/24.
//

#ifndef P2PPLAY_INPUTSERVICEWORKER_H
#define P2PPLAY_INPUTSERVICEWORKER_H

#include <QObject>
#include <QList>
#include <QTimer>
#include "Role.h"
#include "InputCapture.h"
#include "InputSender.h"
#include "InputReceiver.h"

class InputServiceWorker : public QObject {
    Q_OBJECT
public:
    explicit InputServiceWorker(QObject* parent = nullptr);
    ~InputServiceWorker() override;
    void setRole(const Role& role);

signals:
    void inputSampleBytesReady(const QByteArray& bytes);
    void inputAckSampleBytesReady(const QByteArray& bytes);

    // Guest 收到 Host 鼠标位置
    void hostMousePositionReceived(const InputSample& sample);

public slots:
    void start();
    void setControlActive(bool active);
    void onInputSampleBytesReceived(const QByteArray& bytes);
    // Guest 视频窗口产生的鼠标事件
    void onMouseInputSampleReady(const InputSample& sample);

private:
    void connectRoleSignals();
    void clearRoleConnections();

    // Host 定时发送鼠标位置
    void sendHostMousePosition();

private:
    Role role_ = Role::Unknown;
    QList<QMetaObject::Connection> roleConnections_;

    InputCapture capture_;
    InputSender sender_;
    InputReceiver receiver_;

    QTimer mouseSyncTimer_;
};


#endif //P2PPLAY_INPUTSERVICEWORKER_H
