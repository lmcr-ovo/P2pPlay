//
// Created by ASUS on 2026/8/4.
//
#include <windows.h>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include "ScreenVideoSource.h"

ScreenVideoSource::ScreenVideoSource(QObject* parent)
    : QObject(parent) {
    connect(&timer_, &QTimer::timeout, this, [this] {
        screenShot();
    });
}

void ScreenVideoSource::applyConfig(const AppConfig &config) {
    intervalMs_ = config.video.frameIntervalMs();
    width_ = config.video.width;
    height_ = config.video.height;
}

void ScreenVideoSource::start() {
    if (!timer_.isActive()) {
        timer_.start(intervalMs_);
    }
}

void ScreenVideoSource::stop() {
    if (timer_.isActive()) {
        timer_.stop();
    }
}

void ScreenVideoSource::screenShot() {
    //QImage img = qtCaptureScreen();
    QImage img = win32CaptureScreen();

    QImage scaled = img.scaled(
            width_,
            height_,
            Qt::KeepAspectRatio,
            Qt::FastTransformation);

    TraceManager::instance().record(nextSampleSeq_, TraceStage::CaptureEnd, TraceManager::nowUs());
    emit videoImageReady(scaled, nextSampleSeq_++);
}

QImage ScreenVideoSource::qtCaptureScreen() {
    QScreen* screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0);
    QImage img = pixmap.toImage();
    return img;
}

QImage ScreenVideoSource::win32CaptureScreen() {
    HWND hDesktopWnd = GetDesktopWindow();
    HDC hDesktopDC = GetDC(hDesktopWnd);

    int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HDC hMemDC = CreateCompatibleDC(hDesktopDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hDesktopDC, screenWidth, screenHeight);
    HGDIOBJ hOldObj = SelectObject(hMemDC, hBitmap);

    BitBlt(hMemDC,
           0,
           0,
           screenWidth,
           screenHeight,
           hDesktopDC,
           0,
           0,
           SRCCOPY);

    BITMAPINFO bmpInfo = {};
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = screenWidth;
    bmpInfo.bmiHeader.biHeight = -screenHeight;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 32;
    bmpInfo.bmiHeader.biCompression = BI_RGB;

    QImage img(screenWidth, screenHeight, QImage::Format_ARGB32);

    GetDIBits(hMemDC,
              hBitmap,
              0,
              screenHeight,
              img.bits(),
              &bmpInfo,
              DIB_RGB_COLORS);

    SelectObject(hMemDC, hOldObj);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(hDesktopWnd, hDesktopDC);

    return img;
}