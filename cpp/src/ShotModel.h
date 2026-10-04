#pragma once

#include "Shot.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSortFilterProxyModel>
#include <QVector>

class ImageLoader;

class ShotModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { MarkRole = Qt::UserRole + 1, HasJpgRole, HasRafRole };

    explicit ShotModel(ImageLoader *loader, QObject *parent = nullptr);

    // Scans the folder (non-recursive), pairs JPG/RAF by stem and restores saved marks.
    void load(const QString &folder);
    QString folder() const { return m_folder; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    const Shot &shot(int row) const { return m_shots[row]; }
    int count() const { return m_shots.size(); }
    int countMarked(Mark mark) const;

    void setMark(int row, Mark mark);
    bool undo(int *row);

signals:
    void marksChanged();

private:
    void saveSession() const;
    void loadSession();
    QString sessionFile() const;

    ImageLoader *m_loader;
    QString m_folder;
    QVector<Shot> m_shots;
    QHash<QString, int> m_rowByPath;  // display path -> row
    QVector<QPair<int, Mark>> m_undo; // row, previous mark
};

class ShotFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    enum Filter { All, Unmarked, Selected, Rejected };
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setFilter(Filter filter);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    Filter m_filter = All;
};
