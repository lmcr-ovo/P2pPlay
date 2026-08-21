//
// Created by ASUS on 2026/8/20.
//

#ifndef P2PPLAY_RECEIVERREPORTSAMPLE_H
#define P2PPLAY_RECEIVERREPORTSAMPLE_H

#include <QtCore>

struct ReceiverReportSample {
    quint32 expectFrames = 0;     // 本周期期望帧数
    quint32 receivedFrames = 0;   // 实际收到帧数

    // 丢帧率
    double lossRate() const {
        if (expectFrames == 0) {
            return 0.0;
        }
        return 1.0 - static_cast<double>(receivedFrames) / expectFrames;
    }
};

class ReceiverReportSampleCodec {
public:
    static QByteArray encode(const ReceiverReportSample& report);
    static bool decode(const QByteArray& bytes, ReceiverReportSample* report);
};

#endif //P2PPLAY_RECEIVERREPORTSAMPLE_H
