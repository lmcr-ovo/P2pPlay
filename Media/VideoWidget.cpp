//
// Created by ASUS on 2026/8/4.
//

#include "VideoWidget.h"
#include <QPainter>
#include <QDateTime>
#include <QDebug>
#include "VideoSample.h"

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent) {
    setAutoFillBackground(true);
    setPalette(Qt::black);
}

void VideoWidget::onVideoImage(const QImage& img) {
    if (img.isNull()) {
        return;
    }

    image_ = img;
    update();
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
}