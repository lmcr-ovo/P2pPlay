//
// Created by ASUS on 2026/8/6.
//

#ifndef P2PPLAY_LOGGER_H
#define P2PPLAY_LOGGER_H

#include <QObject>
#include <QFile>
#include <QString>
#include <QMutex>
#include <memory>

class Logger : public QObject {
public:
    explicit Logger(QObject* parent, const QString& fileName);
    ~Logger() override;

    void writeLine(const QString& data);

private:
    std::unique_ptr<QFile> file_;
    QMutex mutex_;
};


#endif //P2PPLAY_LOGGER_H
