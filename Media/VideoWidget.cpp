//
// Created by ASUS on 2026/8/4.
//

#include "VideoWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include "Input/InputSample.h"
#include "Input/InputCoordinate.h"
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
    refreshTimer_.setInterval(static_cast<quint16>(intervalMs_));
}

void VideoWidget::onVideoImageReady(const QImage& img, quint32 sampleId) {
    if (img.isNull()) {
        return;
    }

    image_ = img;
    currentSampleId_ = sampleId;
    frameDirty_ = true;
}

void VideoWidget::onHostMousePositionReceived(
        const InputSample& sample) {

    if (sample.device != InputDevice::Mouse ||
        sample.action != InputAction::MouseMove) {
        return;
    }

    hostMouseVisible_ = true;

    hostMousePos_ = mapFromNormalized(
            sample.x,
            sample.y
    );

    update();
}

void VideoWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    QPainter painter(this);

    if (!image_.isNull()) {
        painter.drawImage(
                videoDrawRect(),
                image_
        );
    }

    if (hostMouseVisible_) {
        painter.setRenderHint(
                QPainter::Antialiasing
        );

        const qreal x = hostMousePos_.x();
        const qreal y = hostMousePos_.y();

        QPolygonF arrow;
        arrow << QPointF(x, y);
        arrow << QPointF(x + 4, y + 12);
        arrow << QPointF(x + 7, y + 8);
        arrow << QPointF(x + 14, y + 15);
        arrow << QPointF(x + 11, y + 18);
        arrow << QPointF(x + 7, y + 12);

        painter.setBrush(
                QColor(255, 255, 255, 220)
        );

        painter.setPen(
                QPen(Qt::black, 1)
        );

        painter.drawPolygon(arrow);
    }

    if (currentSampleId_ != 0 &&
        paintedSampleId_ != currentSampleId_) {

        TraceManager::instance().record(
                currentSampleId_,
                TraceStage::RenderEnd,
                TraceManager::nowUs()
        );

        paintedSampleId_ = currentSampleId_;
    }
}

void VideoWidget::mouseMoveEvent(
        QMouseEvent* event) {

    QWidget::mouseMoveEvent(event);

    if (!inputControlActive_) {
        return;
    }

    const QPoint normalizedPos = mapToNormalized(event->pos());

    InputSample sample;
    sample.kind = InputSampleKind::Request;
    sample.device = InputDevice::Mouse;
    sample.action = InputAction::MouseMove;

    sample.x = normalizedPos.x();
    sample.y = normalizedPos.y();

    sample.timeStampMs = QDateTime::currentMSecsSinceEpoch();

    emit mouseInputSampleReady(sample);
}

void VideoWidget::mousePressEvent(
        QMouseEvent* event) {

    QWidget::mousePressEvent(event);
    setFocus(Qt::MouseFocusReason);

    if (!inputControlActive_) {
        inputControlActive_ = true;
        emit inputControlActiveChanged(true);
    }

    const QPoint normalizedPos =
            mapToNormalized(event->pos());

    InputSample sample;
    sample.kind = InputSampleKind::Request;
    sample.device = InputDevice::Mouse;
    sample.action = InputAction::MouseDown;

    sample.x = normalizedPos.x();
    sample.y = normalizedPos.y();

    sample.mouseButton =
            mapMouseButtonFromQt(
                    event->button()
            );

    sample.timeStampMs =
            QDateTime::currentMSecsSinceEpoch();

    emit mouseInputSampleReady(sample);
}

void VideoWidget::mouseReleaseEvent(
        QMouseEvent* event) {

    QWidget::mouseReleaseEvent(event);

    if (!inputControlActive_) {
        return;
    }

    const QPoint normalizedPos =
            mapToNormalized(event->pos());

    InputSample sample;
    sample.kind = InputSampleKind::Request;
    sample.device = InputDevice::Mouse;
    sample.action = InputAction::MouseUp;

    sample.x = normalizedPos.x();
    sample.y = normalizedPos.y();

    sample.mouseButton =
            mapMouseButtonFromQt(
                    event->button()
            );

    sample.timeStampMs =
            QDateTime::currentMSecsSinceEpoch();

    emit mouseInputSampleReady(sample);
}

void VideoWidget::wheelEvent(
        QWheelEvent* event) {

    QWidget::wheelEvent(event);

    if (!inputControlActive_) {
        return;
    }

    InputSample sample;
    sample.kind = InputSampleKind::Request;
    sample.device = InputDevice::Mouse;
    sample.action = InputAction::MouseWheel;

    sample.wheelDelta =
            event->angleDelta().y();

    sample.timeStampMs =
            QDateTime::currentMSecsSinceEpoch();

    emit mouseInputSampleReady(sample);
}

void VideoWidget::enterEvent(QEvent* event) {
    QWidget::enterEvent(event);

    localMouseHidden_ = true;
    setCursor(Qt::BlankCursor);
}

void VideoWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);

    localMouseHidden_ = false;
    unsetCursor();
}

void VideoWidget::focusOutEvent(QFocusEvent* event) {
    if (inputControlActive_) {
        inputControlActive_ = false;
        emit inputControlActiveChanged(false);
    }

    if (localMouseHidden_) {
        localMouseHidden_ = false;
        unsetCursor();
    }

    QWidget::focusOutEvent(event);
}

void VideoWidget::closeEvent(QCloseEvent* event) {
    if (inputControlActive_) {
        inputControlActive_ = false;
        emit inputControlActiveChanged(false);
    }

    if (localMouseHidden_) {
        localMouseHidden_ = false;
        unsetCursor();
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

QRect VideoWidget::videoDrawRect() const {
    if (image_.isNull()) {
        return rect();
    }

    const QSize fitSize =
            image_.size().scaled(
                    rect().size(),
                    Qt::KeepAspectRatio
            );

    QRect drawRect(
            QPoint(0, 0),
            fitSize
    );

    drawRect.moveCenter(rect().center());

    return drawRect;
}

QPoint VideoWidget::mapToNormalized(
        const QPoint& widgetPos) const {

    const QRect drawRect = videoDrawRect();

    if (drawRect.isEmpty()) {
        return QPoint(0, 0);
    }

    // 将黑边区域的坐标限制到视频区域边界
    const int x = qBound(
            drawRect.left(),
            widgetPos.x(),
            drawRect.right()
    );

    const int y = qBound(
            drawRect.top(),
            widgetPos.y(),
            drawRect.bottom()
    );

    const qreal relativeX =
            static_cast<qreal>(
                    x - drawRect.left()
            );

    const qreal relativeY =
            static_cast<qreal>(
                    y - drawRect.top()
            );

    return QPoint(
            InputCoordinate::toNormalized(
                    relativeX,
                    drawRect.width()
            ),
            InputCoordinate::toNormalized(
                    relativeY,
                    drawRect.height()
            )
    );
}

QPointF VideoWidget::mapFromNormalized(
        qint32 normalizedX,
        qint32 normalizedY) const {

    const QRect drawRect = videoDrawRect();

    if (drawRect.isEmpty()) {
        return QPointF();
    }

    const qreal relativeX =
            InputCoordinate::fromNormalized(
                    normalizedX,
                    drawRect.width()
            );

    const qreal relativeY =
            InputCoordinate::fromNormalized(
                    normalizedY,
                    drawRect.height()
            );

    return QPointF(
            drawRect.left() + relativeX,
            drawRect.top() + relativeY
    );
}