//
// Created by ASUS on 2026/8/7.
//

#ifndef P2PPLAY_TRACEEVENT_H
#define P2PPLAY_TRACEEVENT_H

#include <QtCore>

enum class TraceStage : quint8 {
    CaptureEnd = 0,
    EncodeEnd,
    PackEnd,
    SendEnd,
    ReassembleEnd,
    DecodeEnd,
    RenderEnd
};

struct TraceEvent {
    quint32 sampleId = 0;
    TraceStage stage = TraceStage::CaptureEnd;
    qint64 timestampUs = 0;
};

inline QString traceStageName(TraceStage stage) {
    switch (stage) {
        case TraceStage::CaptureEnd:
            return "CaptureEnd";
        case TraceStage::EncodeEnd:
            return "EncodeEnd";
        case TraceStage::PackEnd:
            return "PackEnd";
        case TraceStage::SendEnd:
            return "SendEnd";
        case TraceStage::ReassembleEnd:
            return "ReassembleEnd";
        case TraceStage::DecodeEnd:
            return "DecodeEnd";
        case TraceStage::RenderEnd:
            return "RenderEnd";
    }
    return "Unknown";
}

#endif //P2PPLAY_TRACEEVENT_H
