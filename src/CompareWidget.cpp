#include "CompareWidget.h"
#include "ViewerWidget.h"

#include <QGridLayout>

CompareWidget::CompareWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);

    for (int i = 0; i < MaxPanes; ++i) {
        auto *pane = new ViewerWidget(this);
        pane->setFocusPolicy(Qt::NoFocus); // keys go to the CompareWidget
        m_panes << pane;

        connect(pane, &ViewerWidget::clicked, this, [this, i] {
            setFocus();
            emit paneClicked(i);
        });
        // Mirror zoom and panning onto the other panes.
        connect(pane, &ViewerWidget::zoomChanged, this, [this, i](bool zoomed, QPointF center) {
            for (int j = 0; j < m_count; ++j)
                if (j != i) m_panes[j]->setZoomed(zoomed, center);
            emit zoomChanged();
        });
        connect(pane, &ViewerWidget::panned, this, [this, i](QPointF delta) {
            for (int j = 0; j < m_count; ++j)
                if (j != i) m_panes[j]->panBy(delta);
        });
    }
}

void CompareWidget::setPaneCount(int count)
{
    count = std::clamp(count, 1, MaxPanes);
    if (count == m_count) return;
    m_count = count;
    for (ViewerWidget *pane : m_panes) m_layout->removeWidget(pane);
    const int columns = count == 4 ? 2 : count;
    for (int i = 0; i < MaxPanes; ++i) {
        m_panes[i]->setVisible(i < count);
        if (i < count) m_layout->addWidget(m_panes[i], i / columns, i % columns);
    }
}

void CompareWidget::setActive(int pane)
{
    m_active = pane;
    for (int i = 0; i < MaxPanes; ++i) m_panes[i]->setHighlighted(i == pane);
}

bool CompareWidget::anyZoomed() const
{
    for (int i = 0; i < m_count; ++i)
        if (m_panes[i]->zoomed()) return true;
    return false;
}

void CompareWidget::unzoom()
{
    for (ViewerWidget *pane : m_panes) pane->setZoomed(false, {});
}

void CompareWidget::keyPressEvent(QKeyEvent *event)
{
    const KeyAction a = keyAction(event);
    if (a.action == Action::None) {
        QWidget::keyPressEvent(event);
        return;
    }
    if (event->isAutoRepeat() && isEditAction(a.action)) return;
    switch (a.action) {
    case Action::Rotate:
        m_panes[m_active]->rotate();
        break;
    case Action::Zoom:
        // Each pane zooms on its own AF point: the subject may have moved between frames.
        if (anyZoomed()) unzoom();
        else for (int i = 0; i < m_count; ++i) m_panes[i]->toggleZoom();
        emit zoomChanged();
        break;
    case Action::Back:
        if (anyZoomed()) {
            unzoom();
            emit zoomChanged();
        } else {
            emit actionTriggered(a);
        }
        break;
    default:
        emit actionTriggered(a);
    }
}
