//
// Created by ASUS on 2026/8/24.
//

#ifndef P2PPLAY_INPUTSERVICEWORKER_H
#define P2PPLAY_INPUTSERVICEWORKER_H

#include <QObject>
#include <QList>
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

private:
    void connectRoleSignals();
    void clearRoleConnections();

public slots:
    void start();
    void setControlActive(bool active);
    void onInputSampleBytesReceived(const QByteArray& bytes);

private:
    Role role_ = Role::Unknown;
    QList<QMetaObject::Connection> roleConnections_;

    InputCapture capture_;
    InputSender sender_;
    InputReceiver receiver_;
};


#endif //P2PPLAY_INPUTSERVICEWORKER_H
