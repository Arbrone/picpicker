#include "ShotModel.h"
#include "ImageLoader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>

#include <algorithm>

namespace {
const QString kSelected = QStringLiteral("selected");
const QString kRejected = QStringLiteral("rejected");
}

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
    beginResetModel();
    m_loader->reset();
    m_folder = folder;
    m_shots.clear();
    m_rowByPath.clear();
    m_undo.clear();

    QMap<QString, Shot> byStem; // sorted by stem
    const auto entries = QDir(folder).entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo &fi : entries) {
        const QString ext = fi.suffix().toLower();
        const bool isJpg = ext == "jpg" || ext == "jpeg";
        if (!isJpg && ext != "raf") continue;
        Shot &s = byStem[fi.completeBaseName()];
        s.stem = fi.completeBaseName();
        (isJpg ? s.jpgPath : s.rafPath) = fi.absoluteFilePath();
    }
    m_shots = QVector<Shot>(byStem.cbegin(), byStem.cend());
    for (int i = 0; i < m_shots.size(); ++i) m_rowByPath.insert(m_shots[i].displayPath(), i);

    loadSession();
    endResetModel();
    emit marksChanged();
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
    case Qt::DecorationRole: return m_loader->thumbnail(s);
    case MarkRole: return int(s.mark);
    case HasJpgRole: return !s.jpgPath.isEmpty();
    case HasRafRole: return !s.rafPath.isEmpty();
    default: return {};
    }
}

int ShotModel::countMarked(Mark mark) const
{
    return int(std::count_if(m_shots.cbegin(), m_shots.cend(),
                             [mark](const Shot &s) { return s.mark == mark; }));
}

void ShotModel::setMark(int row, Mark mark)
{
    if (row < 0 || row >= m_shots.size() || m_shots[row].mark == mark) return;
    m_undo.append({row, m_shots[row].mark});
    m_shots[row].mark = mark;
    emit dataChanged(index(row), index(row), {MarkRole});
    saveSession();
    emit marksChanged();
}

bool ShotModel::undo(int *row)
{
    if (m_undo.isEmpty()) return false;
    const auto [r, previous] = m_undo.takeLast();
    m_shots[r].mark = previous;
    emit dataChanged(index(r), index(r), {MarkRole});
    saveSession();
    emit marksChanged();
    if (row) *row = r;
    return true;
}

QString ShotModel::sessionFile() const
{
    return QDir(m_folder).filePath(QStringLiteral(".picpicker.json"));
}

void ShotModel::saveSession() const
{
    QJsonObject marks;
    for (const Shot &s : m_shots) {
        if (s.mark != Mark::None) marks.insert(s.stem, s.mark == Mark::Selected ? kSelected : kRejected);
    }
    if (marks.isEmpty()) {
        QFile::remove(sessionFile());
        return;
    }
    QSaveFile file(sessionFile());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(QJsonObject{{"marks", marks}}).toJson(QJsonDocument::Compact));
    file.commit();
}

void ShotModel::loadSession()
{
    QFile file(sessionFile());
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject marks = QJsonDocument::fromJson(file.readAll()).object().value("marks").toObject();
    for (Shot &s : m_shots) {
        const QString m = marks.value(s.stem).toString();
        s.mark = m == kSelected ? Mark::Selected : m == kRejected ? Mark::Rejected : Mark::None;
    }
}

void ShotFilter::setFilter(Filter filter)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_filter = filter;
    endFilterChange();
#else
    m_filter = filter;
    invalidateFilter();
#endif
}

bool ShotFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filter == All) return true;
    const auto mark = Mark(sourceModel()->index(sourceRow, 0, sourceParent).data(ShotModel::MarkRole).toInt());
    switch (m_filter) {
    case Unmarked: return mark == Mark::None;
    case Selected: return mark == Mark::Selected;
    case Rejected: return mark == Mark::Rejected;
    default: return true;
    }
}
