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
    explicit ViewerWidget(QWidget *parent = nullptr);

    // newShot resets the manual rotation. Zoom state and pan position are kept, so stepping
    // through a burst at 100% stays on the same spot. nativeSize is the full size of the shot's
    // image; when known, 100% zoom uses it even while a smaller preview is displayed.
    void setImage(const QImage &image, bool newShot, QSize nativeSize = {});
    void setOverlay(const QString &text, Mark mark, Label label);
    void setFocusPoint(QPointF normalized); // negative = unknown
    void setShowFocus(bool show);
    void setHighlighted(bool highlighted);

    bool zoomed() const { return m_zoomed; }
    QPointF center() const { return m_center; }
    void setZoomed(bool zoomed, QPointF center);
    void toggleZoom();     // zooms on the AF point when known, else the centre
    void rotate();
    void panBy(QPointF pixels);

signals:
    void actionTriggered(KeyAction action);
    void zoomChanged(bool zoomed, QPointF center); // only for user-initiated zoom changes
    void panned(QPointF pixels);
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QSizeF zoomedSize() const;
    QRectF imageRect() const;
    QPointF rotatedFocus() const;

    QImage m_image;      // rotated source
    QImage m_fitted;     // m_image scaled to the widget, rebuilt lazily
    QSize m_nativeSize;  // before manual rotation
    int m_rotation = 0;
    bool m_zoomed = false;
    QPointF m_center{0.5, 0.5}; // normalized image point at the widget centre when zoomed
    QPointF m_lastDrag;
    QPointF m_focus{-1, -1};
    bool m_showFocus = true;
    bool m_highlighted = false;
    QString m_overlay;
    Mark m_mark = Mark::None;
    Label m_label = Label::None;
};
