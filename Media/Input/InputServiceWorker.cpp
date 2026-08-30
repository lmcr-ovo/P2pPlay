//
// Created by ASUS on 2026/8/24.
//

#include <windows.h>
#include "InputServiceWorker.h"
#include "InputCoordinate.h"
#include "InputSampleCodec.h"

InputServiceWorker::InputServiceWorker(QObject* parent)
    : QObject(parent),
    capture_(this),
    sender_(this),
    receiver_(this),
    mouseSyncTimer_(this) {
}

InputServiceWorker::~InputServiceWorker() {
    clearRoleConnections();
}

void InputServiceWorker::setRole(const Role& role) {
    role_ = role;
    clearRoleConnections();
    connectRoleSignals();
}

void InputServiceWorker::connectRoleSignals() {
    if (role_ == Role::Host) {
        // 接收方回复Ack
        roleConnections_.append(connect(
                &receiver_, &InputReceiver::inputAckSampleBytesReady,
                this, &InputServiceWorker::inputAckSampleBytesReady));

        // Host 每 33ms 发送一次鼠标位置
        mouseSyncTimer_.setInterval(33);

        roleConnections_.append(
                connect(
                        &mouseSyncTimer_,
                        &QTimer::timeout,
                        this,
                        &InputServiceWorker::
                        sendHostMousePosition
                )
        );
    } else {
        // 接收capture发出的inputRawSample
        roleConnections_.append(connect(
                &capture_, &InputCapture::inputRawSampleReady,
                &sender_, &InputSender::onInputRawSampleReady
                ));
        // 接收sender发出的inputSampleBytesReady
        roleConnections_.append(connect(
                &sender_, &InputSender::inputSampleBytesReady,
                this, &InputServiceWorker::inputSampleBytesReady
                ));
    }
}

void InputServiceWorker::clearRoleConnections() {
    for (const auto& conn : roleConnections_) {
        disconnect(conn);
    }
    roleConnections_.clear();
    mouseSyncTimer_.stop();
}

void InputServiceWorker::sendHostMousePosition() {
    POINT point;

    if (!GetCursorPos(&point)) {
        return;
    }

    const int virtualLeft =
            GetSystemMetrics(SM_XVIRTUALSCREEN);

    const int virtualTop =
            GetSystemMetrics(SM_YVIRTUALSCREEN);

    const int virtualWidth =
            GetSystemMetrics(SM_CXVIRTUALSCREEN);

    const int virtualHeight =
            GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (virtualWidth <= 1 ||
        virtualHeight <= 1) {
        return;
    }

    const qreal relativeX =
            static_cast<qreal>(
                    point.x - virtualLeft
            );

    const qreal relativeY =
            static_cast<qreal>(
                    point.y - virtualTop
            );

    InputSample sample;
    sample.kind = InputSampleKind::Request;
    sample.device = InputDevice::Mouse;
    sample.action = InputAction::MouseMove;

    sample.x = InputCoordinate::toNormalized(
            relativeX,
            virtualWidth
    );

    sample.y = InputCoordinate::toNormalized(
            relativeY,
            virtualHeight
    );

    sample.timeStampMs =
            QDateTime::currentMSecsSinceEpoch();

    // 鼠标移动为不可靠高频消息
    emit inputSampleBytesReady(
            InputSampleCodec::encode(sample)
    );
}

void InputServiceWorker::start() {
    if (role_ == Role::Guest) {
        capture_.start();
        return;
    }

    if (role_ == Role::Host) {
        if (!mouseSyncTimer_.isActive()) {
            mouseSyncTimer_.start();
        }
    }
}

void InputServiceWorker::setControlActive(bool active) {
    capture_.setControlActive(active);
}

void InputServiceWorker::onInputSampleBytesReceived(const QByteArray &bytes) {
    if (role_ == Role::Host) {
        // Host 接收 Guest 键盘和鼠标事件
        receiver_.onInputSampleBytesReady(bytes);
        return;
    }

    if (role_ != Role::Guest) {
        return;
    }

    InputSample sample;

    if (!InputSampleCodec::decode(bytes, sample)) {
        return;
    }

    if (sample.kind == InputSampleKind::Ack) {
        sender_.onInputAckSampleBytesReady(bytes);
        return;
    }

    if (sample.kind != InputSampleKind::Request) {
        return;
    }

    if (sample.device == InputDevice::Mouse &&
        sample.action == InputAction::MouseMove) {

        emit hostMousePositionReceived(sample);
    }
}

void InputServiceWorker::onMouseInputSampleReady(
        const InputSample& sample) {

    if (role_ != Role::Guest) {
        return;
    }

    if (sample.action == InputAction::MouseMove) {
        // 高频鼠标移动直接发送
        emit inputSampleBytesReady(
                InputSampleCodec::encode(sample)
        );
    } else {
        // 鼠标按键和滚轮使用可靠传输
        sender_.onInputRawSampleReady(sample);
    }
}