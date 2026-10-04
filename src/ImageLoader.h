#pragma once

#include "Shot.h"

#include <QCache>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QThreadPool>

#include <functional>

// Asynchronous image decoding with in-memory caches.
// Never demosaics RAW data: RAF files are shown through their embedded JPEG preview.
class ImageLoader : public QObject {
    Q_OBJECT
public:
    static constexpr int ThumbSize = 256;
    static constexpr int UrgentPriority = 1 << 20;

    explicit ImageLoader(QObject *parent = nullptr);
    ~ImageLoader() override;

    // Drops all caches and ignores results of jobs still running (call on folder change).
    void reset();

    // Returns the best thumbnail available right now, possibly null. The first call returns the
    // small thumbnail embedded in the EXIF (decoded on the spot, well under a millisecond) and
    // queues the sharp one; thumbnailReady() fires when it arrives. Higher priority loads first.
    QImage thumbnail(const Shot &shot, int priority = UrgentPriority);

    // Returns the cached preview, or a null QImage after queueing a decode.
    QImage preview(const Shot &shot, bool fullRes);
    // Queue a decode without returning anything (used for prefetching).
    void request(const Shot &shot, bool fullRes, int priority);

    // Cancel queued (not yet started) jobs for anything not in the set: what scrolled out of
    // view, or shots you skipped past. Running jobs finish, since a decode can't be interrupted.
    void retainThumbnails(const QSet<QString> &displayPaths);
    void retainPreviews(const QSet<QString> &stems);

signals:
    void thumbnailReady(const QString &path);
    void previewReady(const QString &stem, bool fullRes);

private:
    class Job;
    void schedule(QThreadPool &pool, QHash<QString, Job *> &jobs, const QString &key, int priority,
                  std::function<void(Job *)> work);
    void finish(QHash<QString, Job *> &jobs, Job *job);
    void cancel(QThreadPool &pool, QHash<QString, Job *> &jobs, const std::function<bool(const QString &)> &drop);
    QString previewKey(const QString &stem, bool fullRes) const;
    QString diskCacheFile(const QString &path) const;

    QThreadPool m_thumbPool;
    QThreadPool m_previewPool;
    QCache<QString, QImage> m_thumbs;    // key: display path, cost in KiB
    QSet<QString> m_lowRes;              // thumbnails that are still the EXIF placeholder
    QSet<QString> m_failed;              // could not be decoded: don't retry on every repaint
    QCache<QString, QImage> m_previews;  // key: previewKey(), cost in KiB
    QHash<QString, Job *> m_thumbJobs;   // queued or running, by display path
    QHash<QString, Job *> m_previewJobs; // queued or running, by previewKey()
    int m_previewSide;
    int m_generation = 0;
    QString m_cacheDir;
};
