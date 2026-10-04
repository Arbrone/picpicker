#include "ThumbDelegate.h"
#include "Colors.h"
#include "ShotModel.h"

#include <QImage>
#include <QPainter>

void ThumbDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform);
    const QRect cell = option.rect.adjusted(4, 4, -4, -4);

    const auto mark = Mark(index.data(ShotModel::MarkRole).toInt());
    const bool current = option.state & QStyle::State_Selected;
    p->fillRect(cell, current ? option.palette.highlight().color().darker(160) : QColor(40, 40, 40));
    if (mark != Mark::None) {
        p->setPen(QPen(markColor(mark), 5));
        p->drawRect(cell.adjusted(2, 2, -3, -3));
    } else if (current) {
        p->setPen(QPen(option.palette.highlight().color(), 2));
        p->drawRect(cell.adjusted(1, 1, -2, -2));
    }

    const int textHeight = option.fontMetrics.height() + 6;
    const QRect imageRect = cell.adjusted(8, 8, -8, -textHeight - 4);
    const QImage thumb = index.data(Qt::DecorationRole).value<QImage>();
    if (!thumb.isNull()) {
        const QSize size = thumb.size().scaled(imageRect.size(), Qt::KeepAspectRatio);
        QRect target(QPoint(), size);
        target.moveCenter(imageRect.center());
        p->drawImage(target, thumb);
    }

    const QRect textRect(cell.left() + 8, cell.bottom() - textHeight, cell.width() - 16, textHeight);
    QString badges;
    if (index.data(ShotModel::HasJpgRole).toBool()) badges += QStringLiteral("JPG ");
    if (index.data(ShotModel::HasRafRole).toBool()) badges += QStringLiteral("RAF");
    p->setPen(QColor(150, 150, 150));
    p->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, badges.trimmed());
    p->setPen(QColor(230, 230, 230));
    const int badgeWidth = option.fontMetrics.horizontalAdvance(badges) + 6;
    p->drawText(textRect.adjusted(0, 0, -badgeWidth, 0), Qt::AlignLeft | Qt::AlignVCenter,
                option.fontMetrics.elidedText(index.data().toString(), Qt::ElideMiddle, textRect.width() - badgeWidth));
    p->restore();
}

QSize ThumbDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {CellWidth, CellHeight};
}
