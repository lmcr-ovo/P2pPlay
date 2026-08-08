//
// Created by ASUS on 2026/8/6.
//

#include "Logger.h"

#include <QMutexLocker>

Logger::Logger(QObject* parent, const QString& fileName)
    : QObject(parent)
{
    file_ = std::make_unique<QFile>(fileName);
    file_->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

Logger::~Logger() {
    if (file_ != nullptr && file_->isOpen()) {
        file_->flush();
        file_->close();
    }
}

void Logger::writeLine(const QString &data) {
    QMutexLocker locker(&mutex_);
    if (file_ == nullptr || !file_->isOpen()) {
        return;
    }

    QByteArray bytes = data.toUtf8();
    bytes.append('\n');
    file_->write(bytes);
    file_->flush();
}
