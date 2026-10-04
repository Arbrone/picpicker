#pragma once

#include "Actions.h"

#include <QVector>
#include <QWidget>

class QGridLayout;
class ViewerWidget;

// 2–4 viewer panes side by side, with zoom and panning kept in sync.
class CompareWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int MaxPanes = 4;

    explicit CompareWidget(QWidget *parent = nullptr);

    void setPaneCount(int count);
    int paneCount() const { return m_count; }
    ViewerWidget *pane(int i) const { return m_panes[i]; }
    void setActive(int pane);
    void unzoom();

signals:
    void actionTriggered(KeyAction action);
    void paneClicked(int pane);
    void zoomChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool anyZoomed() const;

    QVector<ViewerWidget *> m_panes;
    QGridLayout *m_layout;
    int m_count = 0;
    int m_active = 0;
};
