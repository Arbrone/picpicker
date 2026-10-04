#include "ViewerWidget.h"
#include "Colors.h"

#include <QKeyEvent>
#include <QPainter>
#include <QTransform>

#include <algorithm>

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
    update();
}

QPointF ViewerWidget::rotatedFocus() const
{
    QPointF p = m_focus;
    for (int r = 0; r < m_rotation; r += 90) p = {1 - p.y(), p.x()};
    return p;
}

QSizeF ViewerWidget::zoomedSize() const
{
    // 100%: one pixel of the full-size image per device pixel.
    QSizeF native = m_nativeSize.isValid() ? QSizeF(m_nativeSize) : QSizeF(m_image.size());
    if (m_rotation % 180) native.transpose();
    return native / devicePixelRatioF();
}

QRectF ViewerWidget::imageRect() const
{
    if (m_image.isNull()) return {};
    if (m_zoomed) {
        const QSizeF s = zoomedSize();
        const QPointF topLeft = QPointF(width() / 2.0, height() / 2.0)
                                - QPointF(m_center.x() * s.width(), m_center.y() * s.height());
        return {topLeft, s};
    }
    const QSizeF fit = QSizeF(m_image.size()).scaled(QSizeF(size()), Qt::KeepAspectRatio);
    return {QPointF((width() - fit.width()) / 2, (height() - fit.height()) / 2), fit};
}

void ViewerWidget::setZoomed(bool zoomed, QPointF center)
{
    if (m_image.isNull()) return;
    m_zoomed = zoomed;
    if (zoomed) m_center = {std::clamp(center.x(), 0.0, 1.0), std::clamp(center.y(), 0.0, 1.0)};
    update();
}

void ViewerWidget::toggleZoom()
{
    setZoomed(!m_zoomed, m_focus.x() >= 0 ? rotatedFocus() : QPointF(0.5, 0.5));
}

void ViewerWidget::panBy(QPointF pixels)
{
    if (!m_zoomed) return;
    const QSizeF s = zoomedSize();
    m_center = {std::clamp(m_center.x() - pixels.x() / s.width(), 0.0, 1.0),
                std::clamp(m_center.y() - pixels.y() / s.height(), 0.0, 1.0)};
    update();
}

void ViewerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 18));
    const QRectF target = imageRect();

    if (!m_image.isNull()) {
        if (m_zoomed) {
            p.setRenderHint(QPainter::SmoothPixmapTransform);
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
            const qreal side = std::max(28.0, 0.06 * std::min(target.width(), target.height()));
            const QRectF box(c - QPointF(side / 2, side / 2), QSizeF(side, side));
            p.setRenderHint(QPainter::Antialiasing);
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
    if (!m_overlay.isEmpty()) {
        const int dot = m_label == Label::None ? 0 : p.fontMetrics().height();
        const QRect textRect = p.fontMetrics().boundingRect(m_overlay);
        const QRect box(12, height() - textRect.height() - 26, textRect.width() + dot + 16 + (dot ? 6 : 0),
                        textRect.height() + 8);
        p.fillRect(box, QColor(0, 0, 0, 170));
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
        emit zoomChanged(m_zoomed, m_center);
        break;
    case Action::Back:
        if (m_zoomed) {
            setZoomed(false, {});
            emit zoomChanged(false, m_center);
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
    QWidget::resizeEvent(event);
}

void ViewerWidget::mousePressEvent(QMouseEvent *event)
{
    m_lastDrag = event->position();
    emit clicked();
}

void ViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_zoomed || !(event->buttons() & Qt::LeftButton)) return;
    const QPointF delta = event->position() - m_lastDrag;
    m_lastDrag = event->position();
    panBy(delta);
    emit panned(delta);
}

void ViewerWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QRectF r = imageRect();
    if (r.isEmpty()) return;
    // Zoom on the clicked point.
    const QPointF center((event->position().x() - r.left()) / r.width(), (event->position().y() - r.top()) / r.height());
    setZoomed(!m_zoomed, center);
    emit zoomChanged(m_zoomed, m_center);
}
