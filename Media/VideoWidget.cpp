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
    setMouseTracking(true);

    setAutoFillBackground(true);
    setPalette(Qt::black);

    connect(&refreshTimer_, &QTimer::timeout,
            this, &VideoWidget::refreshFrame);
    refreshTimer_.start();
}

void VideoWidget::applyConfig(const AppConfig &config) {
    intervalMs_ = config.video.frameIntervalMs();
    refreshTimer_.setInterval(intervalMs_);
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

void VideoWidget::mousePressEvent(QMouseEvent* event) {
    QWidget::mousePressEvent(event);

    setFocus(Qt::MouseFocusReason);

    if (!inputControlActive_) {
        inputControlActive_ = true;
        emit inputControlActiveChanged(true);
    }
}

void VideoWidget::focusOutEvent(QFocusEvent* event) {
    if (inputControlActive_) {
        inputControlActive_ = false;
        emit inputControlActiveChanged(false);
    }

    QWidget::focusOutEvent(event);
}

void VideoWidget::closeEvent(QCloseEvent* event) {
    if (inputControlActive_) {
        inputControlActive_ = false;
        emit inputControlActiveChanged(false);
    }

    QWidget::closeEvent(event);
}


void VideoWidget::refreshFrame() {
    if (!frameDirty_) {
        return;
    }

    frameDirty_ = false;
    update();
}
