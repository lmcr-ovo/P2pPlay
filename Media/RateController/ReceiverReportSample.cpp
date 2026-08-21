//
// Created by ASUS on 2026/8/20.
//

#include "ReceiverReportSample.h"
#include <QDataStream>

QByteArray ReceiverReportSampleCodec::encode(const ReceiverReportSample& report) {
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out.setVersion(QDataStream::Qt_5_14);

    out << report.expectFrames
        << report.receivedFrames;

    return bytes;
}

bool ReceiverReportSampleCodec::decode(const QByteArray& bytes, ReceiverReportSample* report) {
    if (report == nullptr) {
        return false;
    }

    QDataStream in(bytes);
    in.setByteOrder(QDataStream::BigEndian);
    in.setVersion(QDataStream::Qt_5_14);

    ReceiverReportSample result;
    in >> result.expectFrames
       >> result.receivedFrames;

    if (in.status() != QDataStream::Ok) {
        return false;
    }

    *report = result;
    return true;
}