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

void ViewerWidget::setImage(const QImage &image, bool newShot)
{
    if (newShot) {
        m_rotation = 0;
        m_center = {0.5, 0.5};
    }
    m_image = m_rotation ? image.transformed(QTransform().rotate(m_rotation)) : image;
    m_fitted = {};
    update();
}

void ViewerWidget::setOverlay(const QString &text, Mark mark)
{
    m_overlay = text;
    m_mark = mark;
    update();
}

void ViewerWidget::rotate()
{
    m_rotation = (m_rotation + 90) % 360;
    m_image = m_image.transformed(QTransform().rotate(90));
    m_fitted = {};
    update();
}

QSize ViewerWidget::zoomedSize() const
{
    // 100%: one image pixel per device pixel.
    return (QSizeF(m_image.size()) / devicePixelRatioF()).toSize();
}

void ViewerWidget::setZoomed(bool zoomed, QPointF focus)
{
    if (m_zoomed == zoomed || m_image.isNull()) return;
    m_zoomed = zoomed;
    if (zoomed) {
        // Zoom on the clicked point of the fitted image.
        const QSize fit = m_image.size().scaled(size(), Qt::KeepAspectRatio);
        QRect r(QPoint(), fit);
        r.moveCenter(rect().center());
        m_center = {std::clamp((focus.x() - r.left()) / r.width(), 0.0, 1.0),
                    std::clamp((focus.y() - r.top()) / r.height(), 0.0, 1.0)};
    }
    update();
    emit zoomToggled(zoomed);
}

void ViewerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 18));

    if (!m_image.isNull()) {
        if (m_zoomed) {
            const QSize s = zoomedSize();
            const QPointF topLeft = QPointF(width() / 2.0, height() / 2.0)
                                    - QPointF(m_center.x() * s.width(), m_center.y() * s.height());
            p.drawImage(QRectF(topLeft, QSizeF(s)), m_image);
        } else {
            const qreal dpr = devicePixelRatioF();
            const QSize target = m_image.size().scaled((QSizeF(size()) * dpr).toSize(), Qt::KeepAspectRatio);
            if (m_fitted.size() != target) {
                m_fitted = m_image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                m_fitted.setDevicePixelRatio(dpr);
            }
            const QSizeF logical = QSizeF(m_fitted.size()) / dpr;
            p.drawImage(QPointF((width() - logical.width()) / 2, (height() - logical.height()) / 2), m_fitted);
        }
    }

    if (m_mark != Mark::None) {
        p.fillRect(QRect(0, 0, width(), 6), markColor(m_mark));
        p.fillRect(QRect(0, height() - 6, width(), 6), markColor(m_mark));
    }
    if (!m_overlay.isEmpty()) {
        const QRect textRect = p.fontMetrics().boundingRect(m_overlay).adjusted(-8, -4, 8, 4);
        const QRect box(QPoint(12, height() - textRect.height() - 18), textRect.size());
        p.fillRect(box, QColor(0, 0, 0, 170));
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, m_overlay);
    }
}

void ViewerWidget::keyPressEvent(QKeyEvent *event)
{
    const bool repeat = event->isAutoRepeat();
    switch (event->key()) {
    case Qt::Key_Right: emit nextRequested(); break;
    case Qt::Key_Left: emit prevRequested(); break;
    case Qt::Key_Home: emit firstRequested(); break;
    case Qt::Key_End: emit lastRequested(); break;
    case Qt::Key_Up:
    case Qt::Key_S:
        if (!repeat) emit markRequested(Mark::Selected);
        break;
    case Qt::Key_Down:
    case Qt::Key_X:
        if (!repeat) emit markRequested(Mark::Rejected);
        break;
    case Qt::Key_Space:
        if (!repeat) emit markRequested(Mark::None);
        break;
    case Qt::Key_Escape:
        if (m_zoomed) setZoomed(false, {});
        else emit backRequested();
        break;
    case Qt::Key_R: rotate(); break;
    case Qt::Key_Z: setZoomed(!m_zoomed, rect().center()); break;
    default: QWidget::keyPressEvent(event);
    }
}

void ViewerWidget::resizeEvent(QResizeEvent *event)
{
    m_fitted = {};
    QWidget::resizeEvent(event);
}

void ViewerWidget::mousePressEvent(QMouseEvent *event)
{
    m_dragStart = event->position().toPoint();
    m_dragCenter = m_center;
}

void ViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_zoomed || !(event->buttons() & Qt::LeftButton)) return;
    const QSize s = zoomedSize();
    const QPointF delta = event->position() - m_dragStart;
    m_center = {std::clamp(m_dragCenter.x() - delta.x() / s.width(), 0.0, 1.0),
                std::clamp(m_dragCenter.y() - delta.y() / s.height(), 0.0, 1.0)};
    update();
}

void ViewerWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    setZoomed(!m_zoomed, event->position());
}
