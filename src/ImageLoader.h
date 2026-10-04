#pragma once

#include "Shot.h"

#include <QCache>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QThreadPool>

// Asynchronous image decoding with in-memory caches.
// Never demosaics RAW data: RAF files are shown through their embedded JPEG preview.
class ImageLoader : public QObject {
    Q_OBJECT
public:
    static constexpr int ThumbSize = 256;

    explicit ImageLoader(QObject *parent = nullptr);
    ~ImageLoader() override;

    // Drops all caches and ignores results of jobs still running (call on folder change).
    void reset();

    // Return the cached image, or a null QImage after scheduling a decode.
    QImage thumbnail(const Shot &shot);
    QImage preview(const Shot &shot, bool fullRes);

    // Schedule a decode without returning anything (used for prefetching).
    void request(const Shot &shot, bool fullRes, int priority);

signals:
    void thumbnailReady(const QString &path);
    void previewReady(const QString &stem, bool fullRes);

private:
    QString previewKey(const QString &stem, bool fullRes) const;

    QThreadPool m_thumbPool;
    QThreadPool m_previewPool;
    QCache<QString, QImage> m_thumbs;    // key: display path, cost in KiB
    QCache<QString, QImage> m_previews;  // key: previewKey(), cost in KiB
    QSet<QString> m_pendingThumbs;
    QSet<QString> m_pendingPreviews;
    int m_previewSide;
    int m_generation = 0;
    QString m_cacheDir;
};
