//
// Created by ASUS on 2026/8/4.
//

#include "VideoWidget.h"
#include <QPainter>
#include <QDebug>
#include <QKeyEvent>
#include "Video/VideoSample.h"
#include "Input/InputSample.h"
#include "Input/InputSampleCodec.h"

#include <windows.h>

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);

    setAutoFillBackground(true);
    setPalette(Qt::black);

    connect(&refreshTimer_, &QTimer::timeout, this, &VideoWidget::refreshFrame);
    refreshTimer_.start();
}

void VideoWidget::applyConfig(const AppConfig &config) {
    intervalMs_ = config.video.frameIntervalMs();
    refreshTimer_.setInterval(intervalMs_);

    keyMapper_.insert('A', VK_LWIN);
}

void VideoWidget::onVideoImageReady(const QImage& img, quint32 sampleId) {
    if (img.isNull()) {
        return;
    }

    image_ = img;
    currentSampleId_ = sampleId;
    frameDirty_ = true;
}

void VideoWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter(this);

    if (image_.isNull())
        return;

    const QRect winRect = this->rect();
    // 等比例缩放画面、居中显示
    QSize fitSize = image_.size().scaled(winRect.size(), Qt::KeepAspectRatio);
    QRect drawRect(QPoint(0,0), fitSize);
    drawRect.moveCenter(winRect.center());

    painter.drawImage(drawRect, image_);

    if (currentSampleId_ != 0 && paintedSampleId_ != currentSampleId_) {
        TraceManager::instance().record(currentSampleId_,
                                        TraceStage::RenderEnd,
                                        TraceManager::nowUs());
        paintedSampleId_ = currentSampleId_;
    }
}

bool VideoWidget::eventFilter(QObject* watched, QEvent* event) {

    switch (event->type()) {
        case QEvent::KeyPress: {
            auto *ke = static_cast<QKeyEvent*>(event);
            InputSample sample;
            sample.kind = InputSampleKind::Request;
            sample.device = InputDevice::Keyboard;
            sample.action = InputAction::KeyDown;

            if (keyMapper_.contains(ke->nativeVirtualKey())) {
                sample.vk = keyMapper_[ke->nativeVirtualKey()];
                qDebug() << "map from" << ke->nativeVirtualKey() << sample.vk;
            } else {
                qDebug() << "don't map" << ke->nativeVirtualKey();
                sample.vk = ke->nativeVirtualKey();
            }


            sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();
            emit inputRawSampleReady(sample);
            break;
        }
        case QEvent::KeyRelease: {
            auto *ke = static_cast<QKeyEvent*>(event);
            InputSample sample;
            sample.kind = InputSampleKind::Request;
            sample.device = InputDevice::Keyboard;
            sample.action = InputAction::KeyUp;

            if (keyMapper_.contains(ke->nativeVirtualKey())) {
                sample.vk = keyMapper_[ke->nativeVirtualKey()];
            } else {
                sample.vk = ke->nativeVirtualKey();
            }

            sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();
            emit inputRawSampleReady(sample);
            break;
        }

        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            InputSample sample;
            sample.kind = InputSampleKind::Request;
            sample.device = InputDevice::Mouse;
            sample.action = InputAction::MouseDown;
            sample.x = me->pos().x();
            sample.y = me->pos().y();
            sample.mouseButton = mapMouseButtonFromQt(me->button());
            sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();
            emit inputRawSampleReady(sample);
            break;
        }

        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            InputSample sample;
            sample.kind = InputSampleKind::Request;
            sample.device = InputDevice::Mouse;
            sample.action = InputAction::MouseUp;
            sample.x = me->pos().x();
            sample.y = me->pos().y();
            sample.mouseButton = mapMouseButtonFromQt(me->button());
            sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();
            emit inputRawSampleReady(sample);
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            InputSample sample;
            sample.kind = InputSampleKind::Request;
            sample.device = InputDevice::Mouse;
            sample.action = InputAction::MouseMove;
            sample.x = me->pos().x();
            sample.y = me->pos().y();
            sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();
            emit inputRawSampleReady(sample);
            break;
        }

        case QEvent::Wheel: {
            auto* we = static_cast<QWheelEvent*>(event);
            InputSample sample;
            sample.kind = InputSampleKind::Request;
            sample.device = InputDevice::Mouse;
            sample.action = InputAction::MouseWheel;
            sample.wheelDelta = we->angleDelta().y();
            sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();
            emit inputRawSampleReady(sample);
            break;
        }
        default:
            break;
    }
    return QWidget::eventFilter(watched, event);
}

void VideoWidget::refreshFrame() {
    if (!frameDirty_) {
        return;
    }

    frameDirty_ = false;
    update();
}
