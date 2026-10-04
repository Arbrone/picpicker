#include "ThumbDelegate.h"
#include "Colors.h"
#include "ShotModel.h"

#include <QImage>
#include <QPainter>

void ThumbDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform);
    p->setRenderHint(QPainter::Antialiasing);
    const QRect cell = option.rect.adjusted(4, 4, -4, -4);
    const int burstSize = index.data(ShotModel::BurstSizeRole).toInt();

    // A stacked burst gets a second card peeking out behind it.
    if (m_stacked && burstSize > 1) p->fillRect(cell.adjusted(4, -2, -4, -cell.height() + 2), QColor(70, 70, 70));

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

    const QFontMetrics &fm = option.fontMetrics;
    const int textHeight = fm.height() + 6;
    const QRect imageRect = cell.adjusted(8, 8, -8, -textHeight - 4);
    const QImage thumb = index.data(Qt::DecorationRole).value<QImage>();
    QRect target = imageRect;
    if (!thumb.isNull()) {
        target = QRect(QPoint(), thumb.size().scaled(imageRect.size(), Qt::KeepAspectRatio));
        target.moveCenter(imageRect.center());
        p->drawImage(target, thumb);
    }

    // Badges on the image: burst (top-right), colour label (top-left), stars (bottom-left).
    auto badge = [&](const QString &text, Qt::Alignment corner, QColor color) {
        QRect r = fm.boundingRect(text).adjusted(-5, -1, 5, 1);
        r.moveTopLeft(target.topLeft() + QPoint(4, 4));
        if (corner & Qt::AlignRight) r.moveRight(target.right() - 4);
        if (corner & Qt::AlignBottom) r.moveBottom(target.bottom() - 4);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 170));
        p->drawRoundedRect(r, 3, 3);
        p->setPen(color);
        p->drawText(r, Qt::AlignCenter, text);
    };
    if (burstSize > 1) {
        const int i = index.data(ShotModel::BurstIndexRole).toInt();
        badge(m_stacked ? QStringLiteral("×%1").arg(burstSize) : QStringLiteral("%1/%2").arg(i + 1).arg(burstSize),
              Qt::AlignTop | Qt::AlignRight, Qt::white);
    }
    if (const int rating = index.data(ShotModel::RatingRole).toInt())
        badge(stars(rating), Qt::AlignBottom | Qt::AlignLeft, QColor(0xFF, 0xD2, 0x3F));
    if (const auto label = Label(index.data(ShotModel::LabelRole).toInt()); label != Label::None) {
        const int d = fm.height() - 2;
        p->setPen(QPen(QColor(0, 0, 0, 170), 2));
        p->setBrush(labelColor(label));
        p->drawEllipse(QRect(target.left() + 5, target.top() + 5, d, d));
    }

    const QRect textRect(cell.left() + 8, cell.bottom() - textHeight, cell.width() - 16, textHeight);
    QString files;
    if (index.data(ShotModel::HasJpgRole).toBool()) files += QStringLiteral("JPG ");
    if (index.data(ShotModel::HasRafRole).toBool()) files += QStringLiteral("RAF");
    p->setPen(QColor(150, 150, 150));
    p->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, files.trimmed());
    p->setPen(QColor(230, 230, 230));
    const int filesWidth = fm.horizontalAdvance(files) + 6;
    p->drawText(textRect.adjusted(0, 0, -filesWidth, 0), Qt::AlignLeft | Qt::AlignVCenter,
                fm.elidedText(index.data().toString(), Qt::ElideMiddle, textRect.width() - filesWidth));
    p->restore();
}

QSize ThumbDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return cellSize();
}
