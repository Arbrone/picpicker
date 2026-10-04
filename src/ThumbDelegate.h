#pragma once

#include <QStyledItemDelegate>

#include <algorithm>

class ThumbDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static constexpr int MinWidth = 120;
    static constexpr int MaxWidth = 480;

    // Stacked bursts show "×N"; unstacked frames show their position in the burst.
    void setStacked(bool stacked) { m_stacked = stacked; }
    void setCellWidth(int width) { m_width = std::clamp(width, MinWidth, MaxWidth); }
    int cellWidth() const { return m_width; }
    QSize cellSize() const { return {m_width, m_width * 6 / 7}; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    bool m_stacked = true;
    int m_width = 220;
};
