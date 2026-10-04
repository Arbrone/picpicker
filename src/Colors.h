#pragma once

#include "Shot.h"

#include <QColor>
#include <QString>

// Same colours as the original Python version.
inline QColor markColor(Mark mark)
{
    switch (mark) {
    case Mark::Selected: return QColor(0x5D, 0xF6, 0xA4);
    case Mark::Rejected: return QColor(0xF6, 0x6C, 0x5D);
    default: return {};
    }
}

inline QColor labelColor(Label label)
{
    switch (label) {
    case Label::Red: return QColor(0xE5, 0x48, 0x4D);
    case Label::Yellow: return QColor(0xF2, 0xC9, 0x4C);
    case Label::Green: return QColor(0x4C, 0xB8, 0x5C);
    case Label::Blue: return QColor(0x4A, 0x8F, 0xE7);
    case Label::Purple: return QColor(0xA0, 0x6C, 0xE0);
    default: return {};
    }
}

// Names used by Lightroom's default label set; also stored in the session file.
inline QString labelName(Label label)
{
    switch (label) {
    case Label::Red: return QStringLiteral("Red");
    case Label::Yellow: return QStringLiteral("Yellow");
    case Label::Green: return QStringLiteral("Green");
    case Label::Blue: return QStringLiteral("Blue");
    case Label::Purple: return QStringLiteral("Purple");
    default: return {};
    }
}

inline Label labelFromName(const QString &name)
{
    for (Label l : {Label::Red, Label::Yellow, Label::Green, Label::Blue, Label::Purple})
        if (labelName(l) == name) return l;
    return Label::None;
}

inline QString stars(int rating)
{
    return QString(rating, QChar(0x2605));
}
