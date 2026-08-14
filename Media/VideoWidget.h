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

public slots:
    void onVideoImageReady(const QImage& img, quint32 sampleId);

protected:
    void paintEvent(QPaintEvent *event) override;


    void mousePressEvent(QMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void refreshFrame();

private:
    QTimer refreshTimer_;
    QImage image_;
    quint16 intervalMs_ = 10;
    quint32 currentSampleId_ = 0;
    quint32 paintedSampleId_ = 0;
    bool frameDirty_ = false;
    bool inputControlActive_ = false;
};


#endif //P2PPLAY_VIDEOWIDGET_H
