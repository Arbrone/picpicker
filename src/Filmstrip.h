#pragma once

#include <QVector>
#include <QWidget>

class ImageLoader;
class ShotModel;

// Horizontal strip of thumbnails around the current shot. Frames of a burst are bracketed
// together so you can see where a burst starts and ends.
class Filmstrip : public QWidget {
    Q_OBJECT
public:
    Filmstrip(ShotModel *model, ImageLoader *loader, QWidget *parent = nullptr);

    // rows: source rows in navigation order. current: index into rows.
    // [windowFirst, windowFirst + windowSize) is outlined (the shots visible in compare).
    void setRows(const QVector<int> &rows, int current, int windowFirst = -1, int windowSize = 0);

signals:
    void activated(int position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int cellWidth() const;
    int positionAt(int x) const;
    qreal offset() const; // x of rows[0]

    ShotModel *m_model;
    ImageLoader *m_loader;
    QVector<int> m_rows;
    int m_current = -1;
    int m_windowFirst = -1;
    int m_windowSize = 0;
};
