#include "ImageLoader.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QScreen>
#include <QStandardPaths>
#include <QThread>
#include <QTransform>

#include <libraw/libraw.h>

#include <algorithm>

namespace {

QImage applyFlip(const QImage &img, int flip)
{
    // LibRaw flip codes: 3 = 180°, 5 = 90° CCW, 6 = 90° CW.
    int angle = flip == 3 ? 180 : flip == 5 ? 270 : flip == 6 ? 90 : 0;
    if (angle == 0) return img;
    return img.transformed(QTransform().rotate(angle));
}

// Decode with the JPEG decoder's DCT downscaling when the image is bigger than maxSide.
// maxSide <= 0 means full resolution.
QImage readScaled(QImageReader &reader, int maxSide, int fallbackFlip)
{
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (maxSide > 0 && size.isValid() && std::max(size.width(), size.height()) > maxSide)
        reader.setScaledSize(size.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (!img.isNull() && reader.transformation() == QImageIOHandler::TransformationNone)
        img = applyFlip(img, fallbackFlip);
    return img;
}

QImage decodeRaf(const QString &path, int maxSide)
{
    LibRaw raw;
    if (raw.open_file(QFile::encodeName(path).constData()) != LIBRAW_SUCCESS) return {};
    if (raw.unpack_thumb() != LIBRAW_SUCCESS) return {};

    const libraw_thumbnail_t &t = raw.imgdata.thumbnail;
    const int flip = raw.imgdata.sizes.flip;
    if (t.tformat == LIBRAW_THUMBNAIL_JPEG) {
        QByteArray data = QByteArray::fromRawData(t.thumb, int(t.tlength));
        QBuffer buffer(&data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "jpeg");
        return readScaled(reader, maxSide, flip);
    }
    if (t.tformat == LIBRAW_THUMBNAIL_BITMAP && t.tcolors == 3) {
        QImage img(reinterpret_cast<const uchar *>(t.thumb), t.twidth, t.theight,
                   t.twidth * 3, QImage::Format_RGB888);
        img = img.copy();
        if (maxSide > 0 && std::max(img.width(), img.height()) > maxSide)
            img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return applyFlip(img, flip);
    }
    return {};
}

QImage decodeShot(const Shot &shot, int maxSide)
{
    if (shot.jpgPath.isEmpty()) return decodeRaf(shot.rafPath, maxSide);
    QImageReader reader(shot.jpgPath);
    return readScaled(reader, maxSide, 0);
}

qint64 costKiB(const QImage &img) { return std::max<qint64>(1, img.sizeInBytes() / 1024); }

} // namespace

ImageLoader::ImageLoader(QObject *parent)
    : QObject(parent)
{
    m_thumbPool.setMaxThreadCount(std::max(2, QThread::idealThreadCount() - 1));
    m_previewPool.setMaxThreadCount(2);
    m_thumbs.setMaxCost(400 * 1024);    // ~400 MiB
    m_previews.setMaxCost(700 * 1024);  // ~700 MiB: a dozen screen-sized previews + a few full-res

    // Size previews for the largest screen, so they stay sharp wherever the window is.
    m_previewSide = 1920;
    for (const QScreen *screen : QGuiApplication::screens()) {
        const QSize px = screen->size() * screen->devicePixelRatio();
        m_previewSide = std::max({m_previewSide, px.width(), px.height()});
    }

    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbs";
    QDir().mkpath(m_cacheDir);
}

ImageLoader::~ImageLoader()
{
    m_thumbPool.clear();
    m_previewPool.clear();
    m_thumbPool.waitForDone();
    m_previewPool.waitForDone();
}

void ImageLoader::reset()
{
    ++m_generation;
    m_thumbPool.clear();
    m_previewPool.clear();
    m_thumbs.clear();
    m_previews.clear();
    m_pendingThumbs.clear();
    m_pendingPreviews.clear();
}

QImage ImageLoader::thumbnail(const Shot &shot)
{
    const QString path = shot.displayPath();
    if (QImage *img = m_thumbs.object(path)) return *img;
    if (m_pendingThumbs.contains(path)) return {};
    m_pendingThumbs.insert(path);

    const QFileInfo fi(path);
    const QByteArray key = QCryptographicHash::hash(
        (path + '|' + QString::number(fi.lastModified().toMSecsSinceEpoch()) + '|'
         + QString::number(fi.size())).toUtf8(),
        QCryptographicHash::Md5).toHex();
    const QString cacheFile = m_cacheDir + '/' + key + ".jpg";
    const int generation = m_generation;

    m_thumbPool.start([this, shot, path, cacheFile, generation] {
        QImage img(cacheFile);
        if (img.isNull()) {
            img = decodeShot(shot, ThumbSize);
            if (!img.isNull()) img.save(cacheFile, "jpg", 85);
        }
        img = img.convertToFormat(QImage::Format_RGB32);
        QMetaObject::invokeMethod(this, [this, path, img, generation] {
            if (generation != m_generation) return;
            m_pendingThumbs.remove(path);
            if (img.isNull()) return;
            m_thumbs.insert(path, new QImage(img), costKiB(img));
            emit thumbnailReady(path);
        }, Qt::QueuedConnection);
    });
    return {};
}

QString ImageLoader::previewKey(const QString &stem, bool fullRes) const
{
    return fullRes ? stem + QStringLiteral("#full") : stem;
}

QImage ImageLoader::preview(const Shot &shot, bool fullRes)
{
    if (QImage *img = m_previews.object(previewKey(shot.stem, fullRes))) return *img;
    request(shot, fullRes, 10);
    return {};
}

void ImageLoader::request(const Shot &shot, bool fullRes, int priority)
{
    const QString key = previewKey(shot.stem, fullRes);
    if (m_previews.contains(key) || m_pendingPreviews.contains(key)) return;
    m_pendingPreviews.insert(key);

    const int maxSide = fullRes ? 0 : m_previewSide;
    const int generation = m_generation;
    m_previewPool.start([this, shot, key, fullRes, maxSide, generation] {
        QImage img = decodeShot(shot, maxSide);
        if (!img.isNull()) img = img.convertToFormat(QImage::Format_RGB32);
        QMetaObject::invokeMethod(this, [this, stem = shot.stem, key, fullRes, img, generation] {
            if (generation != m_generation) return;
            m_pendingPreviews.remove(key);
            if (img.isNull()) return;
            m_previews.insert(key, new QImage(img), costKiB(img));
            emit previewReady(stem, fullRes);
        }, Qt::QueuedConnection);
    }, priority);
}
