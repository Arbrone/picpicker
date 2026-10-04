#pragma once

#include <QString>
#include <QStringList>

enum class Mark { None, Selected, Rejected };

// One camera shot: the JPG and/or RAF sharing the same file stem.
struct Shot {
    QString stem;
    QString jpgPath;
    QString rafPath;
    Mark mark = Mark::None;

    // File used for display: the JPG when present, otherwise the RAF's embedded preview.
    QString displayPath() const { return jpgPath.isEmpty() ? rafPath : jpgPath; }

    QStringList files() const
    {
        QStringList out;
        if (!jpgPath.isEmpty()) out << jpgPath;
        if (!rafPath.isEmpty()) out << rafPath;
        return out;
    }
};
