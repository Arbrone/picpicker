#pragma once

#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>

enum class Mark { None, Selected, Rejected };
enum class Label { None, Red, Yellow, Green, Blue, Purple };

// Read from the EXIF / Fujifilm MakerNote of the shot (see Metadata.cpp).
struct Metadata {
    qint64 captureMs = -1;     // capture time incl. sub-seconds, -1 if unknown
    QString exposure;          // "1/250s  f/2.8  ISO 400  23mm"
    QSize imageSize;           // full size of the displayed image, after EXIF orientation
    QPointF focus{-1, -1};     // AF point, normalized to the displayed image (after orientation)
    bool hasFocus() const { return focus.x() >= 0; }
};

// One camera shot: the JPG and/or RAF sharing the same file stem.
struct Shot {
    QString stem;
    QString jpgPath;
    QString rafPath;
    QStringList sidecars;  // .xmp files belonging to the shot; moved together with it
    Mark mark = Mark::None;
    int rating = 0;        // 0..5
    Label label = Label::None;
    int burst = -1;        // index into ShotModel bursts, -1 when the shot isn't part of one
    Metadata meta;

    // File used for display: the JPG when present, otherwise the RAF's embedded preview.
    QString displayPath() const { return jpgPath.isEmpty() ? rafPath : jpgPath; }

    QStringList files() const
    {
        QStringList out;
        if (!jpgPath.isEmpty()) out << jpgPath;
        if (!rafPath.isEmpty()) out << rafPath;
        return out + sidecars;
    }
};
