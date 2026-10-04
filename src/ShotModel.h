#pragma once

#include "Shot.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSortFilterProxyModel>
#include <QVector>

#include <functional>

class ImageLoader;

class ShotModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { MarkRole = Qt::UserRole + 1, RatingRole, LabelRole, HasJpgRole, HasRafRole, BurstSizeRole, BurstIndexRole };

    // Shots taken closer together than this form a burst.
    static constexpr qint64 BurstGapMs = 1000;

    explicit ShotModel(ImageLoader *loader, QObject *parent = nullptr);

    // Scans the folder (non-recursive), pairs JPG/RAF by stem, reads EXIF, groups bursts and
    // restores the saved session.
    void load(const QString &folder);
    QString folder() const { return m_folder; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    const Shot &shot(int row) const { return m_shots[row]; }
    int count() const { return m_shots.size(); }
    int countMarked(Mark mark) const;
    int burstCount() const { return m_bursts.size(); }
    const QVector<int> &burstRows(int burst) const { return m_bursts[burst]; }

    // Every edit below is one undo step, however many shots it touches.
    void setMark(const QVector<int> &rows, Mark mark);
    void setRating(const QVector<int> &rows, int rating);
    void toggleLabel(const QVector<int> &rows, Label label); // clears it when all rows have it
    void keep(int keeper, const QVector<int> &group);         // select keeper, reject the others
    bool undo(QVector<int> *rows);

signals:
    void changed();
    void xmpSkipped(const QString &path);

private:
    struct Saved { int row; Mark mark; int rating; Label label; };
    void edit(const QVector<int> &rows, const std::function<void(int row, Shot &)> &fn);
    void writeSidecar(int row);
    void groupBursts();
    void saveSession() const;
    void loadSession();
    QString sessionFile() const;

    ImageLoader *m_loader;
    QString m_folder;
    QVector<Shot> m_shots;
    QVector<QVector<int>> m_bursts;
    QHash<QString, int> m_rowByPath; // display path -> row
    QVector<QVector<Saved>> m_undo;
};

class ShotFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    enum Filter { All, Unmarked, Selected, Rejected };
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilter(Filter filter);
    void setMinRating(int rating);
    // When stacking, each burst shows as a single cell: its best frame among those that pass.
    void setStackBursts(bool stack);
    bool stackBursts() const { return m_stack; }
    void refresh();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool passes(const Shot &shot) const;
    template<typename F> void change(F f);

    Filter m_filter = All;
    int m_minRating = 0;
    bool m_stack = true;
};
