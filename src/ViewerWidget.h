#pragma once

#include "Actions.h"
#include "Shot.h"

#include <QColor>
#include <QImage>
#include <QWidget>

// Displays one image. Standalone it handles keys itself; as a compare pane (no focus) the
// CompareWidget drives it.
class ViewerWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr qreal MaxScale = 8.0;

    explicit ViewerWidget(QWidget *parent = nullptr);

    // newShot resets the manual rotation. Zoom level and position are kept, so stepping through
    // a burst zoomed in stays on the same spot. nativeSize is the full size of the shot's image;
    // when known, zoom levels refer to it even while a smaller preview is displayed.
    void setImage(const QImage &image, bool newShot, QSize nativeSize = {});
    void setOverlay(const QString &text, Mark mark, Label label);
    void setFocusPoint(QPointF normalized); // negative = unknown
    void setShowFocus(bool show);
    void setHighlighted(bool highlighted);

    // Scale is in pixels of the full-size image: 1.0 = 100%. 0 means "fit to window".
    qreal scale() const { return m_scale; }
    bool zoomed() const { return m_scale > 0; }
    QPointF center() const { return m_center; }
    void setZoom(qreal scale, QPointF center);
    void toggleZoom();                   // fit <-> 100% on the AF point (or the centre)
    void zoomStep(int direction);        // to the next preset level, around the centre
    void rotate();
    void panBy(QPointF pixels);

signals:
    void actionTriggered(KeyAction action);
    void zoomChanged(qreal scale, QPointF center); // only for user-initiated changes
    void panned(QPointF pixels);
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QSizeF nativeSize() const;    // full-size image, after manual rotation
    qreal fitScale() const;
    qreal effectiveScale() const { return m_scale > 0 ? m_scale : fitScale(); }
    QRectF imageRect() const;
    QPointF rotatedFocus() const;
    void zoomAround(qreal scale, QPointF anchor); // keeps the image point under anchor in place
    void clampCenter();
    void updateCursor();

    QImage m_image;      // rotated source
    QImage m_fitted;     // m_image scaled to the widget, rebuilt lazily
    QSize m_nativeSize;  // before manual rotation
    int m_rotation = 0;
    qreal m_scale = 0;
    QPointF m_center{0.5, 0.5}; // normalized image point at the widget centre when zoomed
    QPointF m_lastDrag;
    bool m_dragging = false;
    QPointF m_focus{-1, -1};
    bool m_showFocus = true;
    bool m_highlighted = false;
    QString m_overlay;
    Mark m_mark = Mark::None;
    Label m_label = Label::None;
};
