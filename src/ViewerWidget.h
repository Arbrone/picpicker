#pragma once

#include "Shot.h"

#include <QImage>
#include <QWidget>

// Full-window image display. Keyboard actions are emitted as signals; MainWindow owns navigation.
class ViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget *parent = nullptr);

    // newShot resets rotation and zoom pan; otherwise the image is an upgrade of the same shot.
    void setImage(const QImage &image, bool newShot);
    void setOverlay(const QString &text, Mark mark);
    bool zoomed() const { return m_zoomed; }

signals:
    void nextRequested();
    void prevRequested();
    void firstRequested();
    void lastRequested();
    void markRequested(Mark mark);
    void backRequested();
    void zoomToggled(bool zoomed);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void rotate();
    void setZoomed(bool zoomed, QPointF focus);
    QSize zoomedSize() const;

    QImage m_image;      // rotated source
    QImage m_fitted;     // m_image scaled to the widget, rebuilt lazily
    int m_rotation = 0;
    bool m_zoomed = false;
    QPointF m_center{0.5, 0.5}; // normalized image point at the widget centre when zoomed
    QPoint m_dragStart;
    QPointF m_dragCenter;
    QString m_overlay;
    Mark m_mark = Mark::None;
};
