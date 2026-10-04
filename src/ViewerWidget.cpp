#include "ViewerWidget.h"
#include "Colors.h"

#include <QKeyEvent>
#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kLevels[] = {0.125, 0.25, 0.33, 0.5, 0.67, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0};
}

ViewerWidget::ViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void ViewerWidget::setImage(const QImage &image, bool newShot, QSize nativeSize)
{
    if (newShot) m_rotation = 0;
    m_nativeSize = nativeSize;
    m_image = m_rotation ? image.transformed(QTransform().rotate(m_rotation)) : image;
    m_fitted = {};
    clampCenter();
    updateCursor();
    update();
}

void ViewerWidget::setOverlay(const QString &text, Mark mark, Label label)
{
    m_overlay = text;
    m_mark = mark;
    m_label = label;
    update();
}

void ViewerWidget::setFocusPoint(QPointF normalized)
{
    m_focus = normalized;
    update();
}

void ViewerWidget::setShowFocus(bool show)
{
    m_showFocus = show;
    update();
}

void ViewerWidget::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void ViewerWidget::rotate()
{
    m_rotation = (m_rotation + 90) % 360;
    m_image = m_image.transformed(QTransform().rotate(90));
    m_fitted = {};
    clampCenter();
    update();
}

QPointF ViewerWidget::rotatedFocus() const
{
    QPointF p = m_focus;
    for (int r = 0; r < m_rotation; r += 90) p = {1 - p.y(), p.x()};
    return p;
}

QSizeF ViewerWidget::nativeSize() const
{
    QSizeF native = m_nativeSize.isValid() ? QSizeF(m_nativeSize) : QSizeF(m_image.size());
    if (m_nativeSize.isValid() && m_rotation % 180) native.transpose();
    return native;
}

qreal ViewerWidget::fitScale() const
{
    const QSizeF native = nativeSize();
    if (native.isEmpty()) return 1;
    const qreal dpr = devicePixelRatioF();
    return std::min(width() * dpr / native.width(), height() * dpr / native.height());
}

QRectF ViewerWidget::imageRect() const
{
    if (m_image.isNull()) return {};
    const QSizeF s = nativeSize() * effectiveScale() / devicePixelRatioF();
    if (!zoomed()) return {QPointF((width() - s.width()) / 2, (height() - s.height()) / 2), s};
    const QPointF topLeft = QPointF(width() / 2.0, height() / 2.0)
                            - QPointF(m_center.x() * s.width(), m_center.y() * s.height());
    return {topLeft, s};
}

void ViewerWidget::clampCenter()
{
    // When the image is larger than the view, don't let it pan past its edges.
    if (!zoomed()) return;
    const QSizeF s = nativeSize() * m_scale / devicePixelRatioF();
    auto clampAxis = [](qreal c, qreal view, qreal image) {
        if (image <= view) return 0.5;
        const qreal half = view / 2 / image;
        return std::clamp(c, half, 1 - half);
    };
    m_center = {clampAxis(m_center.x(), width(), s.width()), clampAxis(m_center.y(), height(), s.height())};
}

void ViewerWidget::setZoom(qreal scale, QPointF center)
{
    if (m_image.isNull()) return;
    // Zooming out to (or below) the fit size snaps back to "fit".
    m_scale = scale <= fitScale() * 1.001 ? 0 : std::min(scale, MaxScale);
    m_center = center;
    clampCenter();
    updateCursor();
    update();
}

void ViewerWidget::zoomAround(qreal scale, QPointF anchor)
{
    const QRectF r = imageRect();
    if (r.isEmpty()) return;
    const QPointF p((anchor.x() - r.left()) / r.width(), (anchor.y() - r.top()) / r.height());
    const QSizeF s = nativeSize() * scale / devicePixelRatioF();
    const QPointF offset = anchor - QPointF(width() / 2.0, height() / 2.0);
    setZoom(scale, {p.x() - offset.x() / s.width(), p.y() - offset.y() / s.height()});
}

void ViewerWidget::toggleZoom()
{
    setZoom(zoomed() ? 0 : 1.0, m_focus.x() >= 0 ? rotatedFocus() : QPointF(0.5, 0.5));
}

void ViewerWidget::zoomStep(int direction)
{
    const qreal current = effectiveScale();
    qreal target = current;
    if (direction > 0) {
        for (qreal level : kLevels)
            if (level > current * 1.01) { target = level; break; }
    } else {
        target = 0;
        for (qreal level : kLevels)
            if (level < current * 0.99) target = level;
    }
    zoomAround(target, QPointF(width() / 2.0, height() / 2.0));
}

void ViewerWidget::panBy(QPointF pixels)
{
    if (!zoomed()) return;
    const QSizeF s = nativeSize() * m_scale / devicePixelRatioF();
    m_center -= QPointF(pixels.x() / s.width(), pixels.y() / s.height());
    clampCenter();
    update();
}

void ViewerWidget::updateCursor()
{
    setCursor(!zoomed() ? Qt::ArrowCursor : m_dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
}

void ViewerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 18));
    const QRectF target = imageRect();

    if (!m_image.isNull()) {
        if (zoomed()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, m_scale < 2);
            p.drawImage(target, m_image);
        } else {
            // Cache the fitted image so repaints are a plain blit.
            const qreal dpr = devicePixelRatioF();
            const QSize px = (target.size() * dpr).toSize();
            if (m_fitted.size() != px) {
                m_fitted = m_image.scaled(px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                m_fitted.setDevicePixelRatio(dpr);
            }
            p.drawImage(target.topLeft(), m_fitted);
        }

        if (m_showFocus && m_focus.x() >= 0) {
            const QPointF f = rotatedFocus();
            const QPointF c(target.left() + f.x() * target.width(), target.top() + f.y() * target.height());
            // About the size of a Fuji AF area, but always visible.
            const qreal side = std::clamp(0.06 * std::min(target.width(), target.height()), 28.0, 240.0);
            const QRectF box(c - QPointF(side / 2, side / 2), QSizeF(side, side));
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0, 0, 0, 160), 4));
            p.drawRect(box);
            p.setPen(QPen(QColor(0xFF, 0xD2, 0x3F), 2));
            p.drawRect(box);
        }
    }

    if (m_mark != Mark::None) {
        p.fillRect(QRect(0, 0, width(), 6), markColor(m_mark));
        p.fillRect(QRect(0, height() - 6, width(), 6), markColor(m_mark));
    }
    if (m_highlighted) {
        p.setPen(QPen(QColor(0xFF, 0xD2, 0x3F), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
    }

    auto pill = [&](const QRect &box) { p.fillRect(box, QColor(0, 0, 0, 170)); };
    if (!m_image.isNull()) {
        const QString zoom = zoomed() ? QStringLiteral("%1%").arg(std::lround(m_scale * 100))
                                      : QStringLiteral("Fit %1%").arg(std::lround(fitScale() * 100));
        QRect box = p.fontMetrics().boundingRect(zoom).adjusted(-8, -4, 8, 4);
        box.moveTopRight(QPoint(width() - 12, 14));
        pill(box);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, zoom);
    }
    if (!m_overlay.isEmpty()) {
        const int dot = m_label == Label::None ? 0 : p.fontMetrics().height();
        const QRect textRect = p.fontMetrics().boundingRect(m_overlay);
        const QRect box(12, height() - textRect.height() - 26, textRect.width() + dot + 16 + (dot ? 6 : 0),
                        textRect.height() + 8);
        pill(box);
        if (dot) {
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(labelColor(m_label));
            p.drawEllipse(QRectF(box.left() + 8, box.center().y() - dot / 2.0 + 1, dot - 2, dot - 2));
        }
        p.setPen(Qt::white);
        p.drawText(box.adjusted(8 + dot + (dot ? 6 : 0), 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, m_overlay);
    }
}

void ViewerWidget::keyPressEvent(QKeyEvent *event)
{
    const KeyAction a = keyAction(event);
    if (a.action == Action::None) {
        QWidget::keyPressEvent(event);
        return;
    }
    if (event->isAutoRepeat() && isEditAction(a.action)) return;
    switch (a.action) {
    case Action::Rotate:
        rotate();
        break;
    case Action::Zoom:
        toggleZoom();
        emit zoomChanged(m_scale, m_center);
        break;
    case Action::ZoomIn:
    case Action::ZoomOut:
        zoomStep(a.action == Action::ZoomIn ? 1 : -1);
        emit zoomChanged(m_scale, m_center);
        break;
    case Action::Back:
        if (zoomed()) {
            setZoom(0, m_center);
            emit zoomChanged(0, m_center);
        } else {
            emit actionTriggered(a);
        }
        break;
    default:
        emit actionTriggered(a);
    }
}

void ViewerWidget::resizeEvent(QResizeEvent *event)
{
    m_fitted = {};
    clampCenter();
    QWidget::resizeEvent(event);
}

void ViewerWidget::mousePressEvent(QMouseEvent *event)
{
    m_lastDrag = event->position();
    m_dragging = event->button() == Qt::LeftButton;
    updateCursor();
    emit clicked();
}

void ViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!zoomed() || !(event->buttons() & Qt::LeftButton)) return;
    const QPointF delta = event->position() - m_lastDrag;
    m_lastDrag = event->position();
    panBy(delta);
    emit panned(delta);
}

void ViewerWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
    updateCursor();
}

void ViewerWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Fit <-> 100% on the clicked point.
    zoomAround(zoomed() ? 0 : 1.0, event->position());
    emit zoomChanged(m_scale, m_center);
}

void ViewerWidget::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull()) return;
    // Smooth zoom around the cursor; works for mouse wheels and touchpads.
    const qreal factor = std::pow(1.0015, event->angleDelta().y());
    zoomAround(effectiveScale() * factor, event->position());
    emit zoomChanged(m_scale, m_center);
    event->accept();
}
