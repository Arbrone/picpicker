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

// Number of leading black rows (or columns) in img, scanning from one edge.
int blackLines(const QImage &img, bool rows, bool fromEnd)
{
    const int lines = rows ? img.height() : img.width();
    const int length = rows ? img.width() : img.height();
    for (int i = 0; i < lines / 4; ++i) {
        const int line = fromEnd ? lines - 1 - i : i;
        for (int j = 0; j < length; ++j) {
            if (qGray(rows ? img.pixel(j, line) : img.pixel(line, j)) > 24) return i;
        }
    }
    return lines / 4;
}

// The EXIF thumbnail is stored unrotated, and Fujifilm pads it to 160x120 with black bars.
QImage decodeExifThumb(const Metadata &meta)
{
    if (meta.exifThumb.isEmpty()) return {};
    QImage img = QImage::fromData(meta.exifThumb, "JPEG");
    if (img.isNull()) return {};
    const int angle = meta.orientation == 3 ? 180 : meta.orientation == 6 ? 90 : meta.orientation == 8 ? 270 : 0;
    if (angle) img = img.transformed(QTransform().rotate(angle));
    if (!meta.imageSize.isValid()) return img.convertToFormat(QImage::Format_RGB32);

    // Only look for bars when the aspect ratio says they're there, and only crop if both sides
    // have one of about the same size, so a dark sky or night shot is never cut.
    const qreal aspect = qreal(meta.imageSize.width()) / meta.imageSize.height();
    const qreal current = qreal(img.width()) / img.height();
    if (current < aspect * 0.98 || current > aspect * 1.02) {
        const bool rows = current < aspect;
        const int a = blackLines(img, rows, false);
        const int b = blackLines(img, rows, true);
        if (a > 0 && b > 0 && std::abs(a - b) <= 2) {
            // One extra line per side: JPEG blurs the edge of the bars into the picture.
            const int start = a + 1;
            const int size = (rows ? img.height() : img.width()) - a - b - 2;
            img = rows ? img.copy(0, start, img.width(), size) : img.copy(start, 0, size, img.height());
        }
    }
    return img.convertToFormat(QImage::Format_RGB32);
}

} // namespace

// A pool job we keep ownership of, so a queued job can be taken back out of the pool.
class ImageLoader::Job : public QRunnable {
public:
    QString key;
    int priority = 0;
    std::function<void(Job *)> work;
    void run() override { work(this); }
};

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
    reset();
    m_thumbPool.waitForDone();
    m_previewPool.waitForDone();
}

void ImageLoader::schedule(QThreadPool &pool, QHash<QString, Job *> &jobs, const QString &key, int priority,
                           std::function<void(Job *)> work)
{
    if (Job *job = jobs.value(key)) {
        // Already queued: move it up if it's more urgent now.
        if (priority > job->priority && pool.tryTake(job)) {
            job->priority = priority;
            pool.start(job, priority);
        }
        return;
    }
    auto *job = new Job;
    job->setAutoDelete(false);
    job->key = key;
    job->priority = priority;
    job->work = std::move(work);
    jobs.insert(key, job);
    pool.start(job, priority);
}

void ImageLoader::finish(QHash<QString, Job *> &jobs, Job *job)
{
    // Runs on the GUI thread once the job's result is delivered.
    if (jobs.value(job->key) == job) jobs.remove(job->key);
    delete job;
}

void ImageLoader::cancel(QThreadPool &pool, QHash<QString, Job *> &jobs, const std::function<bool(const QString &)> &drop)
{
    for (auto it = jobs.begin(); it != jobs.end();) {
        // tryTake only succeeds for jobs that haven't started; running ones deliver as usual.
        if (drop(it.key()) && pool.tryTake(it.value())) {
            delete it.value();
            it = jobs.erase(it);
        } else {
            ++it;
        }
    }
}

void ImageLoader::reset()
{
    ++m_generation;
    auto all = [](const QString &) { return true; };
    cancel(m_thumbPool, m_thumbJobs, all);
    cancel(m_previewPool, m_previewJobs, all);
    // Jobs still running are deleted when they report back; forget them here.
    m_thumbJobs.clear();
    m_previewJobs.clear();
    m_thumbs.clear();
    m_lowRes.clear();
    m_failed.clear();
    m_previews.clear();
}

QString ImageLoader::diskCacheFile(const QString &path) const
{
    const QFileInfo fi(path);
    const QByteArray key = QCryptographicHash::hash(
        (path + '|' + QString::number(fi.lastModified().toMSecsSinceEpoch()) + '|'
         + QString::number(fi.size())).toUtf8(),
        QCryptographicHash::Md5).toHex();
    return m_cacheDir + '/' + key + ".jpg";
}

QImage ImageLoader::thumbnail(const Shot &shot, int priority)
{
    const QString path = shot.displayPath();
    QImage current;
    if (QImage *img = m_thumbs.object(path)) {
        if (!m_lowRes.contains(path)) return *img;
        current = *img;
        if (m_failed.contains(path)) return current;
    } else if (m_failed.contains(path)) {
        return {};
    } else if (!(current = decodeExifThumb(shot.meta)).isNull()) {
        m_thumbs.insert(path, new QImage(current), costKiB(current));
        m_lowRes.insert(path);
    }

    const int generation = m_generation;
    schedule(m_thumbPool, m_thumbJobs, path, priority, [this, shot, path, generation](Job *job) {
        const QString cacheFile = diskCacheFile(path);
        QImage img(cacheFile);
        if (img.isNull()) {
            img = decodeShot(shot, ThumbSize);
            if (!img.isNull()) img.save(cacheFile, "jpg", 85);
        }
        img = img.convertToFormat(QImage::Format_RGB32);
        QMetaObject::invokeMethod(this, [this, job, path, img, generation] {
            if (generation != m_generation) {
                delete job;
                return;
            }
            finish(m_thumbJobs, job);
            if (img.isNull()) {
                m_failed.insert(path);
                return;
            }
            m_thumbs.insert(path, new QImage(img), costKiB(img));
            m_lowRes.remove(path);
            emit thumbnailReady(path);
        }, Qt::QueuedConnection);
    });
    return current;
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
    if (m_previews.contains(key)) return;

    const int maxSide = fullRes ? 0 : m_previewSide;
    const int generation = m_generation;
    schedule(m_previewPool, m_previewJobs, key, priority, [this, shot, key, fullRes, maxSide, generation](Job *job) {
        QImage img = decodeShot(shot, maxSide);
        if (!img.isNull()) img = img.convertToFormat(QImage::Format_RGB32);
        QMetaObject::invokeMethod(this, [this, job, stem = shot.stem, key, fullRes, img, generation] {
            if (generation != m_generation) {
                delete job;
                return;
            }
            finish(m_previewJobs, job);
            if (img.isNull()) return;
            m_previews.insert(key, new QImage(img), costKiB(img));
            emit previewReady(stem, fullRes);
        }, Qt::QueuedConnection);
    });
}

void ImageLoader::retainThumbnails(const QSet<QString> &displayPaths)
{
    cancel(m_thumbPool, m_thumbJobs, [&](const QString &path) { return !displayPaths.contains(path); });
}

void ImageLoader::retainPreviews(const QSet<QString> &stems)
{
    cancel(m_previewPool, m_previewJobs, [&](const QString &key) {
        return !stems.contains(key.endsWith(QLatin1String("#full")) ? key.chopped(5) : key);
    });
}
