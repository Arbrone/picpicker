#pragma once

#include "Shot.h"

#include <QColor>

// Same colours as the original Python version.
inline QColor markColor(Mark mark)
{
    switch (mark) {
    case Mark::Selected: return QColor(0x5D, 0xF6, 0xA4);
    case Mark::Rejected: return QColor(0xF6, 0x6C, 0x5D);
    default: return {};
    }
}
