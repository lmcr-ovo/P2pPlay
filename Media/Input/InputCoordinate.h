//
// Created by ASUS on 2026/8/25.
//

#ifndef P2PPLAY_INPUTCOORDINATE_H
#define P2PPLAY_INPUTCOORDINATE_H

#include <QtGlobal>
#include <QtMath>

namespace InputCoordinate {

    constexpr qint32 Max = 65535;

    inline qint32 clamp(qint32 value) {
        return qBound<qint32>(0, value, Max);
    }

    inline qint32 toNormalized(qreal relative, qreal size) {
        if (size <= 1.0) {
            return 0;
        }

        relative = qBound<qreal>(
                0.0,
                relative,
                size - 1.0
        );

        const qreal value =
                relative / (size - 1.0) * Max;

        return qBound<qint32>(
                0,
                qRound(value),
                Max
        );
    }

    inline qreal fromNormalized(qint32 normalized,
                                qreal size) {
        if (size <= 1.0) {
            return 0.0;
        }

        normalized = clamp(normalized);

        return static_cast<qreal>(normalized)
               / Max
               * (size - 1.0);
    }

}

#endif // P2PPLAY_INPUTCOORDINATE_H