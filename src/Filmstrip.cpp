#include "Filmstrip.h"
#include "Colors.h"
#include "ImageLoader.h"
#include "ShotModel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace {
constexpr int kHeight = 96;
constexpr int kGap = 4;
constexpr int kBracket = 10; // space under the cells for the burst bracket
}

Filmstrip::Filmstrip(ShotModel *model, ImageLoader *loader, QWidget *parent)
    : QWidget(parent), m_model(model), m_loader(loader)
{
    setFixedHeight(kHeight);
    connect(m_loader, &ImageLoader::thumbnailReady, this, qOverload<>(&QWidget::update));
    connect(m_model, &ShotModel::changed, this, qOverload<>(&QWidget::update));
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] { m_rows.clear(); update(); });
}

void Filmstrip::setRows(const QVector<int> &rows, int current, int windowFirst, int windowSize)
{
    m_rows = rows;
    m_current = current;
    m_windowFirst = windowFirst;
    m_windowSize = windowSize;
    update();
}

int Filmstrip::cellWidth() const
{
    return int((kHeight - 2 * kGap - kBracket) * 1.5);
}

qreal Filmstrip::offset() const
{
    // Keep the current shot in the middle.
    const int step = cellWidth() + kGap;
    return width() / 2.0 - (m_current + 0.5) * step;
}

int Filmstrip::positionAt(int x) const
{
    const int step = cellWidth() + kGap;
    const int pos = int(std::floor((x - offset()) / step));
    return pos >= 0 && pos < m_rows.size() ? pos : -1;
}

void Filmstrip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(28, 28, 28));
    if (m_rows.isEmpty()) return;
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const int w = cellWidth();
    const int step = w + kGap;
    const int h = kHeight - 2 * kGap - kBracket;
    const qreal x0 = offset();
    const int first = std::max(0, int(-x0 / step) - 1);
    const int last = std::min<int>(m_rows.size() - 1, int((width() - x0) / step) + 1);

    for (int pos = first; pos <= last; ++pos) {
        const Shot &shot = m_model->shot(m_rows[pos]);
        const QRect cell(int(x0 + pos * step), kGap, w, h);
        p.fillRect(cell, QColor(45, 45, 45));
        const QImage thumb = m_loader->thumbnail(shot, ImageLoader::UrgentPriority - std::abs(pos - m_current));
        if (!thumb.isNull()) {
            QRect target(QPoint(), thumb.size().scaled(cell.size(), Qt::KeepAspectRatio));
            target.moveCenter(cell.center());
            p.drawImage(target, thumb);
        }
        if (pos != m_current) p.fillRect(cell, QColor(0, 0, 0, 90)); // dim the others
        if (shot.mark != Mark::None) {
            p.setPen(QPen(markColor(shot.mark), 3));
            p.drawRect(cell.adjusted(1, 1, -2, -2));
        }
        if (shot.label != Label::None) {
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(labelColor(shot.label));
            p.drawEllipse(QRect(cell.left() + 5, cell.top() + 5, 9, 9));
            p.setRenderHint(QPainter::Antialiasing, false);
        }
        if (shot.rating) {
            p.setPen(QColor(0xFF, 0xD2, 0x3F));
            QFont f = p.font();
            f.setPointSizeF(f.pointSizeF() * 0.8);
            p.setFont(f);
            p.drawText(cell.adjusted(4, 0, 0, -2), Qt::AlignLeft | Qt::AlignBottom, stars(shot.rating));
            p.setFont(font());
        }
        if (pos == m_current) {
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(cell.adjusted(-1, -1, 0, 0));
        }

        // Burst bracket: a line under consecutive frames of the same burst.
        if (shot.burst >= 0) {
            const bool prevSame = pos > 0 && m_model->shot(m_rows[pos - 1]).burst == shot.burst;
            const bool nextSame = pos + 1 < m_rows.size() && m_model->shot(m_rows[pos + 1]).burst == shot.burst;
            const int y = cell.bottom() + 6;
            const int left = prevSame ? cell.left() - kGap : cell.left() + 2;
            const int right = nextSame ? cell.right() + 1 : cell.right() - 2;
            p.setPen(QPen(QColor(0xFF, 0xD2, 0x3F), 2));
            p.drawLine(left, y, right, y);
            if (!prevSame) p.drawLine(left, y, left, y - 4);
            if (!nextSame) p.drawLine(right, y, right, y - 4);
        }
    }

    // Outline the shots visible in compare.
    if (m_windowFirst >= 0 && m_windowSize > 1) {
        const QRect r(int(x0 + m_windowFirst * step) - 3, 1, m_windowSize * step - kGap + 6, h + 2 * kGap - 2);
        p.setPen(QPen(QColor(255, 255, 255, 120), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }
}

void Filmstrip::mousePressEvent(QMouseEvent *event)
{
    const int pos = positionAt(int(event->position().x()));
    if (pos >= 0) emit activated(pos);
}

void Filmstrip::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y() + event->angleDelta().x();
    if (delta == 0 || m_rows.isEmpty()) return;
    emit activated(std::clamp<int>(m_current + (delta < 0 ? 1 : -1), 0, m_rows.size() - 1));
}
