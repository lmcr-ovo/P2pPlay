//
// Created by ASUS on 2026/8/4.
//

#ifndef P2PPLAY_VIDEOWIDGET_H
#define P2PPLAY_VIDEOWIDGET_H

#include <QWidget>

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent);

public slots:
    void onVideoImageReady(const QImage& img);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage image_;
};


#endif //P2PPLAY_VIDEOWIDGET_H
