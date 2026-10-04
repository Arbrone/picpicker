#pragma once

#include <QStyledItemDelegate>

class ThumbDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static constexpr int CellWidth = 220;
    static constexpr int CellHeight = 190;

    // Stacked bursts show "×N"; unstacked frames show their position in the burst.
    void setStacked(bool stacked) { m_stacked = stacked; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    bool m_stacked = true;
};
