#include "ShotModel.h"
#include "Colors.h"
#include "ImageLoader.h"
#include "Metadata.h"
#include "Xmp.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrentMap>

#include <algorithm>

namespace {
const QString kSelected = QStringLiteral("selected");
const QString kRejected = QStringLiteral("rejected");

// "DSCF0001.xmp" and darktable's "DSCF0001.RAF.xmp" both belong to stem "DSCF0001".
QString sidecarStem(const QFileInfo &fi)
{
    QString base = fi.completeBaseName();
    for (const char *ext : {".raf", ".jpg", ".jpeg"}) {
        if (base.endsWith(QLatin1String(ext), Qt::CaseInsensitive)) return base.chopped(int(strlen(ext)));
    }
    return base;
}
} // namespace

ShotModel::ShotModel(ImageLoader *loader, QObject *parent)
    : QAbstractListModel(parent), m_loader(loader)
{
    connect(m_loader, &ImageLoader::thumbnailReady, this, [this](const QString &path) {
        const int row = m_rowByPath.value(path, -1);
        if (row >= 0) emit dataChanged(index(row), index(row), {Qt::DecorationRole});
    });
}

void ShotModel::load(const QString &folder)
{
    // Abandon a scan still running for the previous folder.
    if (m_metaWatcher) {
        m_metaWatcher->disconnect(this);
        m_metaWatcher->cancel();
        m_metaWatcher->deleteLater();
        m_metaWatcher = nullptr;
    }

    beginResetModel();
    m_loader->reset();
    m_folder = folder;
    m_shots.clear();
    m_bursts.clear();
    m_rowByPath.clear();
    m_undo.clear();

    QMap<QString, Shot> byStem; // sorted by stem
    QHash<QString, QStringList> sidecars;
    const auto entries = QDir(folder).entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo &fi : entries) {
        const QString ext = fi.suffix().toLower();
        if (ext == "xmp") {
            sidecars[sidecarStem(fi)] << fi.absoluteFilePath();
            continue;
        }
        const bool isJpg = ext == "jpg" || ext == "jpeg";
        if (!isJpg && ext != "raf") continue;
        Shot &s = byStem[fi.completeBaseName()];
        s.stem = fi.completeBaseName();
        (isJpg ? s.jpgPath : s.rafPath) = fi.absoluteFilePath();
    }
    m_shots = QVector<Shot>(byStem.cbegin(), byStem.cend());
    for (int i = 0; i < m_shots.size(); ++i) {
        m_shots[i].sidecars = sidecars.value(m_shots[i].stem);
        m_rowByPath.insert(m_shots[i].displayPath(), i);
    }

    loadSession();
    endResetModel();
    emit changed();

    // Reading EXIF touches every file: on a memory card or a cold disk that takes seconds, so it
    // runs in the background. Only the file paths go to the worker threads.
    QVector<QPair<QString, QString>> paths;
    paths.reserve(m_shots.size());
    for (const Shot &s : std::as_const(m_shots)) paths.append({s.jpgPath, s.rafPath});
    m_metaDone = 0;
    m_metaWatcher = new QFutureWatcher<Metadata>(this);
    connect(m_metaWatcher, &QFutureWatcher<Metadata>::resultsReadyAt, this, &ShotModel::metadataReady);
    connect(m_metaWatcher, &QFutureWatcher<Metadata>::finished, this, [this] {
        groupBursts();
        if (!m_shots.isEmpty()) emit dataChanged(index(0), index(m_shots.size() - 1), {BurstSizeRole, BurstIndexRole});
        m_metaWatcher->deleteLater();
        m_metaWatcher = nullptr;
        emit changed();
        emit metadataFinished();
    });
    m_metaWatcher->setFuture(QtConcurrent::mapped(paths, [](const QPair<QString, QString> &p) {
        return readMetadata(p.first, p.second);
    }));
}

bool ShotModel::metadataLoading() const
{
    return m_metaWatcher != nullptr;
}

void ShotModel::metadataReady(int begin, int end)
{
    for (int i = begin; i < end; ++i) m_shots[i].meta = m_metaWatcher->resultAt(i);
    m_metaDone += end - begin;
    // The EXIF thumbnail is now available as a placeholder for these rows.
    emit dataChanged(index(begin), index(end - 1), {Qt::DecorationRole});
    emit metadataProgress(m_metaDone, m_shots.size());
}

void ShotModel::groupBursts()
{
    // Shots are in file-name order, which is capture order on the camera.
    QVector<int> current;
    auto flush = [&] {
        if (current.size() > 1) {
            for (int row : current) m_shots[row].burst = m_bursts.size();
            m_bursts.append(current);
        }
        current.clear();
    };
    for (int row = 0; row < m_shots.size(); ++row) {
        const qint64 t = m_shots[row].meta.captureMs;
        if (!current.isEmpty()) {
            const qint64 prev = m_shots[current.last()].meta.captureMs;
            if (t < 0 || prev < 0 || t - prev < 0 || t - prev > BurstGapMs) flush();
        }
        current.append(row);
    }
    flush();
}

int ShotModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_shots.size();
}

QVariant ShotModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shots.size()) return {};
    const Shot &s = m_shots[index.row()];
    switch (role) {
    case Qt::DisplayRole: return s.stem;
    // Rows higher up the grid load first.
    case Qt::DecorationRole: return m_loader->thumbnail(s, -index.row());
    case MarkRole: return int(s.mark);
    case RatingRole: return s.rating;
    case LabelRole: return int(s.label);
    case HasJpgRole: return !s.jpgPath.isEmpty();
    case HasRafRole: return !s.rafPath.isEmpty();
    case BurstSizeRole: return s.burst < 0 ? 0 : int(m_bursts[s.burst].size());
    case BurstIndexRole: return s.burst < 0 ? -1 : int(m_bursts[s.burst].indexOf(index.row()));
    default: return {};
    }
}

int ShotModel::countMarked(Mark mark) const
{
    return int(std::count_if(m_shots.cbegin(), m_shots.cend(),
                             [mark](const Shot &s) { return s.mark == mark; }));
}

void ShotModel::edit(const QVector<int> &rows, const std::function<void(int, Shot &)> &fn)
{
    QVector<Saved> before;
    for (int row : rows) {
        if (row < 0 || row >= m_shots.size()) continue;
        Shot &s = m_shots[row];
        const Saved saved{row, s.mark, s.rating, s.label};
        fn(row, s);
        if (s.mark == saved.mark && s.rating == saved.rating && s.label == saved.label) continue;
        before.append(saved);
        writeSidecar(row);
        emit dataChanged(index(row), index(row), {MarkRole, RatingRole, LabelRole});
    }
    if (before.isEmpty()) return;
    m_undo.append(before);
    saveSession();
    emit changed();
}

void ShotModel::setMark(const QVector<int> &rows, Mark mark)
{
    edit(rows, [mark](int, Shot &s) { s.mark = mark; });
}

void ShotModel::setRating(const QVector<int> &rows, int rating)
{
    edit(rows, [rating](int, Shot &s) { s.rating = std::clamp(rating, 0, 5); });
}

void ShotModel::toggleLabel(const QVector<int> &rows, Label label)
{
    const bool allHave = std::all_of(rows.cbegin(), rows.cend(), [&](int r) { return m_shots[r].label == label; });
    edit(rows, [label, allHave](int, Shot &s) { s.label = allHave ? Label::None : label; });
}

void ShotModel::keep(int keeper, const QVector<int> &group)
{
    edit(group, [keeper](int row, Shot &s) { s.mark = row == keeper ? Mark::Selected : Mark::Rejected; });
}

bool ShotModel::undo(QVector<int> *rows)
{
    if (m_undo.isEmpty()) return false;
    const QVector<Saved> step = m_undo.takeLast();
    if (rows) rows->clear();
    for (const Saved &saved : step) {
        Shot &s = m_shots[saved.row];
        s.mark = saved.mark;
        s.rating = saved.rating;
        s.label = saved.label;
        writeSidecar(saved.row);
        emit dataChanged(index(saved.row), index(saved.row), {MarkRole, RatingRole, LabelRole});
        if (rows) rows->append(saved.row);
    }
    saveSession();
    emit changed();
    return true;
}

void ShotModel::writeSidecar(int row)
{
    Shot &s = m_shots[row];
    const QString path = Xmp::path(s);
    switch (Xmp::write(s)) {
    case Xmp::Result::Written:
        if (!s.sidecars.contains(path)) s.sidecars.append(path);
        break;
    case Xmp::Result::Removed:
        s.sidecars.removeAll(path);
        break;
    case Xmp::Result::Foreign:
    case Xmp::Result::Error:
        emit xmpSkipped(path);
        break;
    case Xmp::Result::Unchanged:
        break;
    }
}

QString ShotModel::sessionFile() const
{
    return QDir(m_folder).filePath(QStringLiteral(".picpicker.json"));
}

void ShotModel::saveSession() const
{
    QJsonObject shots;
    for (const Shot &s : m_shots) {
        QJsonObject o;
        if (s.mark != Mark::None) o.insert("mark", s.mark == Mark::Selected ? kSelected : kRejected);
        if (s.rating > 0) o.insert("rating", s.rating);
        if (s.label != Label::None) o.insert("label", labelName(s.label));
        if (!o.isEmpty()) shots.insert(s.stem, o);
    }
    if (shots.isEmpty()) {
        QFile::remove(sessionFile());
        return;
    }
    QSaveFile file(sessionFile());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(QJsonObject{{"version", 2}, {"shots", shots}}).toJson(QJsonDocument::Compact));
    file.commit();
}

void ShotModel::loadSession()
{
    QFile file(sessionFile());
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject shots = root.value("shots").toObject();
    const QJsonObject legacy = root.value("marks").toObject(); // version 1: {"marks": {stem: "selected"}}
    for (Shot &s : m_shots) {
        const QJsonObject o = shots.value(s.stem).toObject();
        const QString m = o.contains("mark") ? o.value("mark").toString() : legacy.value(s.stem).toString();
        s.mark = m == kSelected ? Mark::Selected : m == kRejected ? Mark::Rejected : Mark::None;
        s.rating = std::clamp(o.value("rating").toInt(), 0, 5);
        s.label = labelFromName(o.value("label").toString());
    }
}

template<typename F> void ShotFilter::change(F f)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    f();
    endFilterChange();
#else
    f();
    invalidateFilter();
#endif
}

void ShotFilter::setFilter(Filter filter) { change([&] { m_filter = filter; }); }
void ShotFilter::setMinRating(int rating) { change([&] { m_minRating = rating; }); }
void ShotFilter::setStackBursts(bool stack) { change([&] { m_stack = stack; }); }
void ShotFilter::refresh() { change([] {}); }

bool ShotFilter::accepts(const Shot &s) const
{
    if (s.rating < m_minRating) return false;
    switch (m_filter) {
    case Unmarked: return s.mark == Mark::None;
    case Selected: return s.mark == Mark::Selected;
    case Rejected: return s.mark == Mark::Rejected;
    default: return true;
    }
}

bool ShotFilter::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    const auto *model = static_cast<const ShotModel *>(sourceModel());
    const Shot &s = model->shot(sourceRow);
    if (!m_stack || s.burst < 0) return accepts(s);

    // Stack cover: the first selected frame, else the first unrejected one, among frames that pass.
    int cover = -1;
    int rank = 3;
    for (int row : model->burstRows(s.burst)) {
        const Shot &f = model->shot(row);
        if (!accepts(f)) continue;
        const int r = f.mark == Mark::Selected ? 0 : f.mark == Mark::None ? 1 : 2;
        if (r < rank) {
            rank = r;
            cover = row;
        }
    }
    return cover == sourceRow;
}
