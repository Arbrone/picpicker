#include "Metadata.h"

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QTimeZone>

#include <cmath>
#include <optional>

namespace {

constexpr int kHeadSize = 256 * 1024; // EXIF APP1 is at most 64 KiB and comes first in camera JPEGs

// Bounds-checked view over a TIFF structure.
class Tiff {
public:
    struct Entry {
        quint16 type = 0;
        quint32 count = 0;
        int value = 0; // absolute offset of the value bytes
    };
    using Ifd = QHash<quint16, Entry>;

    Tiff(const QByteArray &data, int start, bool littleEndian)
        : m_data(data), m_start(start), m_le(littleEndian) {}

    quint16 u16(int at) const
    {
        if (at < 0 || at + 2 > m_data.size()) return 0;
        const auto *p = reinterpret_cast<const uchar *>(m_data.constData() + at);
        return m_le ? quint16(p[0] | p[1] << 8) : quint16(p[0] << 8 | p[1]);
    }
    quint32 u32(int at) const
    {
        if (at < 0 || at + 4 > m_data.size()) return 0;
        const auto *p = reinterpret_cast<const uchar *>(m_data.constData() + at);
        return m_le ? quint32(p[0]) | quint32(p[1]) << 8 | quint32(p[2]) << 16 | quint32(p[3]) << 24
                    : quint32(p[0]) << 24 | quint32(p[1]) << 16 | quint32(p[2]) << 8 | quint32(p[3]);
    }

    // `base` is what offsets are relative to: the TIFF header, or the MakerNote start.
    Ifd ifd(int base, quint32 offset) const
    {
        Ifd out;
        const int at = base + int(offset);
        const int n = u16(at);
        if (offset == 0 || n > 1000) return out;
        for (int i = 0; i < n; ++i) {
            const int e = at + 2 + 12 * i;
            if (e + 12 > m_data.size()) break;
            Entry entry{u16(e + 2), u32(e + 4), 0};
            static constexpr int sizes[] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};
            const int size = entry.type < 13 ? sizes[entry.type] : 1;
            entry.value = qint64(size) * entry.count <= 4 ? e + 8 : base + int(u32(e + 8));
            out.insert(u16(e), entry);
        }
        return out;
    }

    std::optional<quint32> uint(const Ifd &ifd, quint16 tag, int index = 0) const
    {
        auto it = ifd.find(tag);
        if (it == ifd.end() || quint32(index) >= it->count) return {};
        if (it->type == 3) return u16(it->value + 2 * index);
        if (it->type == 4) return u32(it->value + 4 * index);
        return {};
    }
    std::optional<double> rational(const Ifd &ifd, quint16 tag) const
    {
        auto it = ifd.find(tag);
        if (it == ifd.end() || (it->type != 5 && it->type != 10)) return {};
        const quint32 den = u32(it->value + 4);
        if (den == 0) return {};
        return it->type == 5 ? double(u32(it->value)) / den : double(qint32(u32(it->value))) / qint32(den);
    }
    QByteArray string(const Ifd &ifd, quint16 tag) const
    {
        auto it = ifd.find(tag);
        if (it == ifd.end() || it->type != 2) return {};
        QByteArray s = m_data.mid(it->value, int(it->count));
        const int nul = s.indexOf('\0');
        return nul >= 0 ? s.left(nul) : s;
    }

    int start() const { return m_start; }
    const QByteArray &data() const { return m_data; }

private:
    const QByteArray &m_data;
    int m_start;
    bool m_le;
};

QByteArray readJpegHead(const QString &path, bool raf)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    if (raf) {
        // RAF header: magic, then the big-endian offset of the embedded JPEG at byte 84.
        const QByteArray header = file.read(92);
        if (header.size() < 92 || !header.startsWith("FUJIFILMCCD-RAW")) return {};
        const auto *p = reinterpret_cast<const uchar *>(header.constData() + 84);
        const quint32 offset = quint32(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3];
        if (!file.seek(offset)) return {};
    }
    return file.read(kHeadSize);
}

// Returns the offset of the TIFF header inside the JPEG's Exif APP1 segment, or -1.
int findExif(const QByteArray &jpeg)
{
    const auto *d = reinterpret_cast<const uchar *>(jpeg.constData());
    if (jpeg.size() < 4 || d[0] != 0xFF || d[1] != 0xD8) return -1;
    int p = 2;
    while (p + 4 <= jpeg.size() && d[p] == 0xFF) {
        const int marker = d[p + 1];
        const int length = d[p + 2] << 8 | d[p + 3];
        if (marker == 0xDA) break; // start of scan: no more metadata
        if (marker == 0xE1 && jpeg.mid(p + 4, 6) == QByteArray("Exif\0\0", 6)) return p + 10;
        p += 2 + length;
    }
    return -1;
}

QString formatExposure(const Tiff &t, const Tiff::Ifd &exif)
{
    QStringList parts;
    if (auto v = t.rational(exif, 0x829a); v && *v > 0) // ExposureTime
        parts << (*v < 1.0 ? QStringLiteral("1/%1s").arg(std::lround(1.0 / *v)) : QStringLiteral("%1s").arg(*v, 0, 'g', 3));
    if (auto v = t.rational(exif, 0x829d); v && *v > 0) // FNumber
        parts << QStringLiteral("f/%1").arg(*v, 0, 'g', 2);
    if (auto v = t.uint(exif, 0x8827); v && *v > 0) // ISO
        parts << QStringLiteral("ISO %1").arg(*v);
    if (auto v = t.rational(exif, 0x920a); v && *v > 0) // FocalLength
        parts << QStringLiteral("%1mm").arg(std::lround(*v));
    return parts.join(QStringLiteral("  "));
}

qint64 captureTime(const Tiff &t, const Tiff::Ifd &exif)
{
    const QByteArray date = t.string(exif, 0x9003); // DateTimeOriginal
    QDateTime dt = QDateTime::fromString(QString::fromLatin1(date), QStringLiteral("yyyy:MM:dd HH:mm:ss"));
    if (!dt.isValid()) return -1;
    dt.setTimeZone(QTimeZone::UTC); // only differences between shots matter
    qint64 ms = dt.toMSecsSinceEpoch();
    const QByteArray sub = t.string(exif, 0x9291).trimmed(); // SubSecTimeOriginal: fraction digits
    if (!sub.isEmpty()) {
        bool ok = false;
        const int value = sub.left(3).toInt(&ok);
        if (ok) ms += value * int(std::pow(10, 3 - std::min<qsizetype>(sub.size(), 3)));
    }
    return ms;
}

QPointF orient(QPointF p, int orientation)
{
    switch (orientation) {
    case 3: return {1 - p.x(), 1 - p.y()};
    case 6: return {1 - p.y(), p.x()}; // rotate 90° CW
    case 8: return {p.y(), 1 - p.x()}; // rotate 90° CCW
    default: return p;
    }
}

Metadata parse(const QByteArray &jpeg, bool withFocus)
{
    Metadata meta;
    const int tiffStart = findExif(jpeg);
    if (tiffStart < 0 || tiffStart + 8 > jpeg.size()) return meta;
    const bool le = jpeg.at(tiffStart) == 'I';
    const Tiff t(jpeg, tiffStart, le);
    const Tiff::Ifd ifd0 = t.ifd(tiffStart, t.u32(tiffStart + 4));
    const int orientation = int(t.uint(ifd0, 0x0112).value_or(1));
    const auto exifOffset = t.uint(ifd0, 0x8769);
    if (!exifOffset) return meta;
    const Tiff::Ifd exif = t.ifd(tiffStart, *exifOffset);

    meta.exposure = formatExposure(t, exif);
    meta.captureMs = captureTime(t, exif);
    const int w = int(t.uint(exif, 0xa002).value_or(0)); // PixelXDimension
    const int h = int(t.uint(exif, 0xa003).value_or(0));
    const bool swap = orientation == 6 || orientation == 8;
    if (w > 0 && h > 0) meta.imageSize = swap ? QSize(h, w) : QSize(w, h);

    // Fujifilm MakerNote: "FUJIFILM", then a little-endian IFD offset relative to the MakerNote.
    // FocusPixel (0x1023) is in the pixel coordinates of the RAF's embedded preview, which is the
    // image PixelXDimension/PixelYDimension describe in that same EXIF block (checked on GFX,
    // X-E5, X-M5, X-T30 III and X-T50 samples). It is not verified for camera JPGs, so we only
    // use it from the RAF.
    auto mn = exif.find(0x927c);
    if (withFocus && w > 0 && h > 0 && mn != exif.end() && jpeg.mid(mn->value, 8) == "FUJIFILM") {
        const Tiff fuji(jpeg, mn->value, true);
        const Tiff::Ifd notes = fuji.ifd(mn->value, fuji.u32(mn->value + 8));
        const auto x = fuji.uint(notes, 0x1023, 0);
        const auto y = fuji.uint(notes, 0x1023, 1);
        if (x && y && (*x || *y) && int(*x) < w && int(*y) < h)
            meta.focus = orient({(*x + 0.5) / w, (*y + 0.5) / h}, orientation);
    }
    return meta;
}

} // namespace

Metadata readMetadata(const QString &jpgPath, const QString &rafPath)
{
    if (rafPath.isEmpty()) return parse(readJpegHead(jpgPath, false), false);

    Metadata meta = parse(readJpegHead(rafPath, true), true);
    if (!jpgPath.isEmpty()) {
        // The JPG is what's displayed: take its full size so 100% zoom matches it.
        const Metadata jpg = parse(readJpegHead(jpgPath, false), false);
        if (jpg.imageSize.isValid()) meta.imageSize = jpg.imageSize;
        if (meta.captureMs < 0) meta.captureMs = jpg.captureMs;
        if (meta.exposure.isEmpty()) meta.exposure = jpg.exposure;
    }
    return meta;
}
