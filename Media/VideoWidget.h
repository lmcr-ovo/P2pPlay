//
// Created by ASUS on 2026/8/4.
//

#ifndef P2PPLAY_VIDEOWIDGET_H
#define P2PPLAY_VIDEOWIDGET_H

#include <QWidget>
#include <QTimer>
#include "AppConfig.h"
#include "TraceManager.h"
#include "Input/InputSample.h"

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent);
    void applyConfig(const AppConfig& config);

signals:
    void inputControlActiveChanged(bool active);
    // Guest 鼠标事件发送给 Host
    void mouseInputSampleReady(const InputSample& sample);

public slots:
    void onVideoImageReady(const QImage& img, quint32 sampleId);
    // 接收 Host 发送的归一化鼠标位置
    void onHostMousePositionReceived(const InputSample& sample);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void refreshFrame();
    // 获取视频实际显示区域
    QRect videoDrawRect() const;
    // 视频窗口坐标转换为归一化坐标
    QPoint mapToNormalized(const QPoint& widgetPos) const;
    // 归一化坐标转换为视频窗口坐标
    QPointF mapFromNormalized(qint32 normalizedX,qint32 normalizedY) const;

private:
    QTimer refreshTimer_;
    QImage image_;
    quint16 intervalMs_ = 10;
    quint32 currentSampleId_ = 0;
    quint32 paintedSampleId_ = 0;
    bool frameDirty_ = false;
    bool inputControlActive_ = false;

    // Host 鼠标在 Guest 视频窗口中的显示位置
    QPointF hostMousePos_;

    bool hostMouseVisible_ = false;
    bool localMouseHidden_ = false;
};


#endif //P2PPLAY_VIDEOWIDGET_H
