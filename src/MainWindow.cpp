#include "MainWindow.h"
#include "ApplyDialog.h"
#include "Colors.h"
#include "CompareWidget.h"
#include "ImageLoader.h"
#include "ShotModel.h"
#include "ThumbDelegate.h"
#include "ViewerWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include <cstdlib>

namespace {
const char *kHelp =
    "<b>Viewer</b><br>"
    "← / → &nbsp;previous / next<br>"
    "↑ or S &nbsp;select &nbsp; ↓ or X &nbsp;reject<br>"
    "Space &nbsp;clear mark<br>"
    "1–5 &nbsp;stars, 0 &nbsp;none<br>"
    "6–9 &nbsp;red / yellow / green / blue<br>"
    "Enter or B &nbsp;open burst<br>"
    "K &nbsp;keep this frame, reject the rest<br>"
    "C &nbsp;compare<br>"
    "Z / double-click &nbsp;100% (on AF point)<br>"
    "F &nbsp;show AF point &nbsp; R &nbsp;rotate<br>"
    "Esc &nbsp;back<br><br>"
    "<b>Grid</b><br>"
    "Enter / double-click &nbsp;open<br>"
    "S X Space 0–9 &nbsp;mark selection<br>"
    "C &nbsp;compare selection or burst<br><br>"
    "Ctrl+Z &nbsp;undo";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("PicPicker");
    resize(1600, 900);

    m_loader = new ImageLoader(this);
    m_model = new ShotModel(m_loader, this);
    m_filter = new ShotFilter(this);
    m_filter->setSourceModel(m_model);

    // Start page
    m_startPage = new QWidget;
    auto *openButton = new QPushButton(tr("Open Folder…"));
    openButton->setMinimumSize(220, 60);
    auto *startLayout = new QVBoxLayout(m_startPage);
    startLayout->addStretch();
    startLayout->addWidget(openButton, 0, Qt::AlignCenter);
    startLayout->addStretch();
    connect(openButton, &QPushButton::clicked, this, &MainWindow::chooseFolder);

    // Grid
    m_grid = new QListView;
    m_delegate = new ThumbDelegate(m_grid);
    m_grid->setModel(m_filter);
    m_grid->setItemDelegate(m_delegate);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setGridSize({ThumbDelegate::CellWidth, ThumbDelegate::CellHeight});
    m_grid->setSpacing(0);
    m_grid->installEventFilter(this);
    connect(m_grid, &QListView::activated, this, &MainWindow::openViewer);

    // Viewer
    m_viewer = new ViewerWidget;
    connect(m_viewer, &ViewerWidget::actionTriggered, this, &MainWindow::viewerAction);
    connect(m_viewer, &ViewerWidget::zoomChanged, this, [this] { refreshViewer(false); });

    // Compare
    m_compare = new CompareWidget;
    connect(m_compare, &CompareWidget::actionTriggered, this, &MainWindow::compareAction);
    connect(m_compare, &CompareWidget::zoomChanged, this, &MainWindow::refreshCompare);
    connect(m_compare, &CompareWidget::paneClicked, this, [this](int pane) { moveCompare(m_cmp.first + pane); });

    connect(m_loader, &ImageLoader::previewReady, this, [this](const QString &stem) {
        if (m_stack->currentWidget() == m_viewer) {
            const int row = currentRow();
            if (row >= 0 && m_model->shot(row).stem == stem) refreshViewer(false);
        } else if (m_stack->currentWidget() == m_compare) {
            for (int row : m_cmp.shown)
                if (m_model->shot(row).stem == stem) return refreshCompare();
        }
    });

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_startPage);
    m_stack->addWidget(m_grid);
    m_stack->addWidget(m_viewer);
    m_stack->addWidget(m_compare);

    // Side panel
    auto *folderButton = new QPushButton(tr("Open Folder…"));
    connect(folderButton, &QPushButton::clicked, this, &MainWindow::chooseFolder);

    m_filterCombo = new QComboBox;
    m_filterCombo->addItems({tr("All"), tr("Unmarked"), tr("Selected"), tr("Rejected")});
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this,
            [this](int i) { m_filter->setFilter(ShotFilter::Filter(i)); });

    m_ratingCombo = new QComboBox;
    m_ratingCombo->addItem(tr("Any rating"));
    for (int i = 1; i <= 5; ++i) m_ratingCombo->addItem(stars(i) + (i < 5 ? tr(" or more") : QString()));
    connect(m_ratingCombo, &QComboBox::currentIndexChanged, m_filter, &ShotFilter::setMinRating);

    m_stackBox = new QCheckBox(tr("Stack bursts"));
    m_stackBox->setChecked(true);
    connect(m_stackBox, &QCheckBox::toggled, this, [this](bool on) {
        m_delegate->setStacked(on);
        m_filter->setStackBursts(on);
    });

    m_focusBox = new QCheckBox(tr("Show AF point (F)"));
    m_focusBox->setChecked(m_showFocus);
    connect(m_focusBox, &QCheckBox::toggled, this, &MainWindow::setShowFocus);

    m_counts = new QLabel;
    m_counts->setTextFormat(Qt::RichText);
    auto *help = new QLabel(tr(kHelp));
    help->setTextFormat(Qt::RichText);
    help->setStyleSheet("color: #999;");

    m_applyButton = new QPushButton(tr("Apply…"));
    m_applyButton->setMinimumHeight(40);
    connect(m_applyButton, &QPushButton::clicked, this, &MainWindow::applySelection);

    auto *side = new QWidget;
    side->setFixedWidth(240);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->addWidget(folderButton);
    sideLayout->addSpacing(12);
    sideLayout->addWidget(new QLabel(tr("Show:")));
    sideLayout->addWidget(m_filterCombo);
    sideLayout->addWidget(m_ratingCombo);
    sideLayout->addWidget(m_stackBox);
    sideLayout->addWidget(m_focusBox);
    sideLayout->addSpacing(12);
    sideLayout->addWidget(m_counts);
    sideLayout->addStretch();
    sideLayout->addWidget(help);
    sideLayout->addStretch();
    sideLayout->addWidget(m_applyButton);

    auto *central = new QWidget;
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(side);
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    auto *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undo);

    connect(m_model, &ShotModel::changed, this, [this] {
        updateCounts();
        // Marks can change which frame covers a stacked burst.
        if (m_filter->stackBursts()) m_filter->refresh();
    });
    connect(m_model, &ShotModel::xmpSkipped, this, [this](const QString &path) {
        statusBar()->showMessage(tr("%1 was not written: it belongs to another application or is read-only")
                                     .arg(QFileInfo(path).fileName()), 6000);
    });
    updateCounts();
}

void MainWindow::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"), m_model->folder());
    if (!dir.isEmpty()) openFolder(dir);
}

void MainWindow::openFolder(const QString &folder)
{
    m_navs.clear();
    m_cmp = {};
    m_model->load(QFileInfo(folder).absoluteFilePath());
    setWindowTitle(QStringLiteral("PicPicker — %1").arg(m_model->folder()));
    m_stack->setCurrentWidget(m_grid);
    m_grid->setFocus();
    if (m_filter->rowCount() > 0) m_grid->setCurrentIndex(m_filter->index(0, 0));
}

QVector<int> MainWindow::visibleRows() const
{
    QVector<int> rows;
    for (int i = 0; i < m_filter->rowCount(); ++i) rows << m_filter->mapToSource(m_filter->index(i, 0)).row();
    return rows;
}

QVector<int> MainWindow::gridSelection() const
{
    QModelIndexList indexes = m_grid->selectionModel()->selectedIndexes();
    std::sort(indexes.begin(), indexes.end(), [](auto &a, auto &b) { return a.row() < b.row(); });
    QVector<int> rows;
    for (const QModelIndex &idx : indexes) {
        const int row = m_filter->mapToSource(idx).row();
        const int burst = m_model->shot(row).burst;
        if (m_filter->stackBursts() && burst >= 0) rows << m_model->burstRows(burst);
        else rows << row;
    }
    return rows;
}

int MainWindow::currentRow() const
{
    if (m_navs.isEmpty()) return -1;
    const Nav &n = m_navs.last();
    return n.pos >= 0 && n.pos < n.rows.size() ? n.rows[n.pos] : -1;
}

// ---- Grid ------------------------------------------------------------------------------------

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_grid && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        // Arrows, Enter and Esc keep their usual grid behaviour.
        switch (key->key()) {
        case Qt::Key_Up: case Qt::Key_Down: case Qt::Key_Left: case Qt::Key_Right:
        case Qt::Key_Home: case Qt::Key_End: case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Escape:
            return false;
        default:
            break;
        }
        const KeyAction a = keyAction(key);
        if (key->isAutoRepeat() && isEditAction(a.action)) return true;
        return gridAction(a);
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::gridAction(KeyAction a)
{
    const QVector<int> rows = gridSelection();
    switch (a.action) {
    case Action::Select: case Action::Reject: case Action::ClearMark: case Action::Rate: case Action::Label:
        applyEdit(rows, a);
        return true;
    case Action::Compare: {
        if (rows.size() < 2) {
            statusBar()->showMessage(tr("Select 2 or more shots, or a burst, to compare"), 4000);
            return true;
        }
        m_cmp.returnTo = m_grid;
        openCompare(rows, 0, std::min<int>(rows.size(), CompareWidget::MaxPanes), true);
        return true;
    }
    default:
        return false;
    }
}

void MainWindow::applyEdit(const QVector<int> &rows, KeyAction a)
{
    switch (a.action) {
    case Action::Select: m_model->setMark(rows, Mark::Selected); break;
    case Action::Reject: m_model->setMark(rows, Mark::Rejected); break;
    case Action::ClearMark: m_model->setMark(rows, Mark::None); break;
    case Action::Rate: m_model->setRating(rows, a.arg); break;
    case Action::Label: m_model->toggleLabel(rows, Label(a.arg)); break;
    default: break;
    }
}

void MainWindow::showGrid(int focusRow)
{
    m_navs.clear();
    m_stack->setCurrentWidget(m_grid);
    if (focusRow >= 0) {
        // With stacking on, a burst frame is found through its cover.
        QModelIndex idx = m_filter->mapFromSource(m_model->index(focusRow));
        const int burst = m_model->shot(focusRow).burst;
        if (!idx.isValid() && burst >= 0) {
            for (int row : m_model->burstRows(burst))
                if ((idx = m_filter->mapFromSource(m_model->index(row))).isValid()) break;
        }
        if (idx.isValid()) {
            m_grid->setCurrentIndex(idx);
            m_grid->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        }
    }
    m_grid->setFocus();
}

// ---- Viewer ----------------------------------------------------------------------------------

void MainWindow::openViewer(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;
    m_navs = {Nav{visibleRows(), proxyIndex.row()}};
    // Opening a stacked burst goes straight inside it.
    const int row = currentRow();
    const int burst = m_model->shot(row).burst;
    if (m_filter->stackBursts() && burst >= 0) {
        const QVector<int> &frames = m_model->burstRows(burst);
        m_navs.append(Nav{frames, int(frames.indexOf(row))});
    }
    m_stack->setCurrentWidget(m_viewer);
    m_viewer->setFocus();
    refreshViewer(true);
}

void MainWindow::goTo(int position)
{
    if (position < 0 || position >= nav().rows.size() || position == nav().pos) return;
    nav().pos = position;
    refreshViewer(true);
}

void MainWindow::advancePastBurst()
{
    if (m_navs.size() > 1) m_navs.removeLast();
    // In an unstacked list the rest of the burst follows the current frame: skip it.
    const int burst = m_model->shot(currentRow()).burst;
    int p = nav().pos + 1;
    while (burst >= 0 && p < nav().rows.size() && m_model->shot(nav().rows[p]).burst == burst) ++p;
    nav().pos = std::min<int>(p, nav().rows.size() - 1);
    refreshViewer(true);
}

void MainWindow::viewerAction(KeyAction a)
{
    const int row = currentRow();
    if (row < 0) return;
    const int burst = m_model->shot(row).burst;
    switch (a.action) {
    case Action::Next: goTo(nav().pos + 1); break;
    case Action::Prev: goTo(nav().pos - 1); break;
    case Action::First: goTo(0); break;
    case Action::Last: goTo(nav().rows.size() - 1); break;
    case Action::Select:
    case Action::Reject:
        applyEdit({row}, a);
        if (nav().pos + 1 < nav().rows.size()) goTo(nav().pos + 1);
        else refreshViewer(false);
        break;
    case Action::ClearMark:
    case Action::Rate:
    case Action::Label:
        applyEdit({row}, a);
        refreshViewer(false);
        break;
    case Action::Keep:
        if (burst < 0) return viewerAction({Action::Select});
        m_model->keep(row, m_model->burstRows(burst));
        advancePastBurst();
        break;
    case Action::Dive:
        if (burst >= 0 && nav().rows != m_model->burstRows(burst)) {
            const QVector<int> &frames = m_model->burstRows(burst);
            m_navs.append(Nav{frames, int(frames.indexOf(row))});
            refreshViewer(true);
        }
        break;
    case Action::Back:
        if (m_navs.size() > 1) {
            m_navs.removeLast();
            // The outer list may show another frame of the burst as its cover.
            if (!nav().rows.contains(row) && burst >= 0) {
                for (int r : m_model->burstRows(burst))
                    if (int p = nav().rows.indexOf(r); p >= 0) { nav().pos = p; break; }
            } else if (int p = nav().rows.indexOf(row); p >= 0) {
                nav().pos = p;
            }
            refreshViewer(true);
        } else {
            showGrid(row);
        }
        break;
    case Action::Compare:
        m_cmp.returnTo = m_viewer;
        if (burst >= 0) {
            const QVector<int> &frames = m_model->burstRows(burst);
            openCompare(frames, int(frames.indexOf(row)), std::min<int>(frames.size(), CompareWidget::MaxPanes), true);
        } else {
            openCompare(nav().rows, nav().pos, 2, false);
        }
        break;
    case Action::ToggleFocus:
        setShowFocus(!m_showFocus);
        break;
    default:
        break;
    }
}

QImage MainWindow::bestImage(const Shot &shot, bool zoomed)
{
    // Full-res when zoomed, then the screen-sized preview, then the thumbnail.
    QImage img;
    if (zoomed) img = m_loader->preview(shot, true);
    if (img.isNull()) img = m_loader->preview(shot, false);
    if (img.isNull()) img = m_loader->thumbnail(shot);
    return img;
}

QString MainWindow::overlayText(int row, int pos, int total) const
{
    const Shot &shot = m_model->shot(row);
    QStringList parts{shot.stem, QStringLiteral("%1/%2").arg(pos + 1).arg(total)};
    if (shot.burst >= 0) {
        const QVector<int> &frames = m_model->burstRows(shot.burst);
        parts << tr("burst %1/%2").arg(frames.indexOf(row) + 1).arg(frames.size());
    }
    parts << (shot.jpgPath.isEmpty() ? "RAF" : shot.rafPath.isEmpty() ? "JPG" : "JPG+RAF");
    if (shot.rating) parts << stars(shot.rating);
    if (!shot.meta.exposure.isEmpty()) parts << shot.meta.exposure;
    return parts.join(QStringLiteral("   "));
}

void MainWindow::refreshViewer(bool newShot)
{
    const int row = currentRow();
    if (row < 0) return;
    const Shot &shot = m_model->shot(row);

    const QImage img = bestImage(shot, m_viewer->zoomed());
    if (!img.isNull() || newShot) m_viewer->setImage(img, newShot, shot.meta.imageSize);
    m_viewer->setFocusPoint(shot.meta.focus);
    m_viewer->setOverlay(overlayText(row, nav().pos, nav().rows.size()), shot.mark, shot.label);

    // Prefetch the shots you're most likely to see next.
    for (int offset : {1, 2, 3, -1}) {
        const int p = nav().pos + offset;
        if (p >= 0 && p < nav().rows.size()) m_loader->request(m_model->shot(nav().rows[p]), false, 5 - std::abs(offset));
    }
}

// ---- Compare ---------------------------------------------------------------------------------

void MainWindow::openCompare(const QVector<int> &rows, int active, int panes, bool keepWholeSet)
{
    m_cmp.rows = rows;
    m_cmp.panes = std::clamp(panes, 1, int(rows.size()));
    m_cmp.keepWholeSet = keepWholeSet;
    m_cmp.shown.clear();
    m_cmp.first = std::clamp(active, 0, int(rows.size()) - m_cmp.panes);
    m_compare->setPaneCount(m_cmp.panes);
    m_compare->unzoom();
    m_stack->setCurrentWidget(m_compare);
    m_compare->setFocus();
    moveCompare(active);
}

void MainWindow::moveCompare(int active)
{
    m_cmp.active = std::clamp(active, 0, int(m_cmp.rows.size()) - 1);
    // Slide the window of panes so the active shot stays visible.
    if (m_cmp.active < m_cmp.first) m_cmp.first = m_cmp.active;
    if (m_cmp.active >= m_cmp.first + m_cmp.panes) m_cmp.first = m_cmp.active - m_cmp.panes + 1;
    m_compare->setActive(m_cmp.active - m_cmp.first);
    refreshCompare();
}

void MainWindow::refreshCompare()
{
    m_cmp.shown.resize(m_cmp.panes, -1);
    for (int i = 0; i < m_cmp.panes; ++i) {
        const int pos = m_cmp.first + i;
        const int row = m_cmp.rows[pos];
        const Shot &shot = m_model->shot(row);
        ViewerWidget *pane = m_compare->pane(i);
        const bool newShot = m_cmp.shown[i] != row;
        const QImage img = bestImage(shot, pane->zoomed());
        if (!img.isNull() || newShot) pane->setImage(img, newShot, shot.meta.imageSize);
        pane->setFocusPoint(shot.meta.focus);
        pane->setShowFocus(m_showFocus);
        pane->setOverlay(overlayText(row, pos, m_cmp.rows.size()), shot.mark, shot.label);
        m_cmp.shown[i] = row;
    }
    // Prefetch the next window.
    for (int p = m_cmp.first + m_cmp.panes; p < std::min<int>(m_cmp.rows.size(), m_cmp.first + 2 * m_cmp.panes); ++p)
        m_loader->request(m_model->shot(m_cmp.rows[p]), false, 3);
}

void MainWindow::closeCompare()
{
    const int row = m_cmp.rows.value(m_cmp.active, -1);
    if (m_cmp.returnTo == m_viewer && !m_navs.isEmpty()) {
        if (int p = nav().rows.indexOf(row); p >= 0) nav().pos = p;
        m_stack->setCurrentWidget(m_viewer);
        m_viewer->setFocus();
        refreshViewer(true);
    } else {
        showGrid(row);
    }
}

void MainWindow::compareAction(KeyAction a)
{
    const int row = m_cmp.rows.value(m_cmp.active, -1);
    if (row < 0) return;
    switch (a.action) {
    case Action::Next: moveCompare(m_cmp.active + 1); break;
    case Action::Prev: moveCompare(m_cmp.active - 1); break;
    case Action::First: moveCompare(0); break;
    case Action::Last: moveCompare(m_cmp.rows.size() - 1); break;
    case Action::Select:
    case Action::Reject:
        applyEdit({row}, a);
        moveCompare(m_cmp.active + 1);
        break;
    case Action::ClearMark:
    case Action::Rate:
    case Action::Label:
        applyEdit({row}, a);
        refreshCompare();
        break;
    case Action::Keep: {
        const QVector<int> group = m_cmp.keepWholeSet ? m_cmp.rows : m_cmp.rows.mid(m_cmp.first, m_cmp.panes);
        m_model->keep(row, group);
        if (m_cmp.returnTo == m_viewer && !m_navs.isEmpty()) {
            if (int p = nav().rows.indexOf(row); p >= 0) nav().pos = p;
            m_stack->setCurrentWidget(m_viewer);
            m_viewer->setFocus();
            advancePastBurst();
        } else {
            showGrid(row);
        }
        break;
    }
    case Action::Back:
    case Action::Compare:
        closeCompare();
        break;
    case Action::ToggleFocus:
        setShowFocus(!m_showFocus);
        break;
    default:
        break;
    }
}

// ---- Misc ------------------------------------------------------------------------------------

void MainWindow::setShowFocus(bool show)
{
    m_showFocus = show;
    m_focusBox->setChecked(show);
    m_viewer->setShowFocus(show);
    for (int i = 0; i < CompareWidget::MaxPanes; ++i) m_compare->pane(i)->setShowFocus(show);
}

void MainWindow::undo()
{
    QVector<int> rows;
    if (!m_model->undo(&rows) || rows.isEmpty()) return;
    if (m_stack->currentWidget() == m_viewer) {
        // Jump back to the shot that changed.
        for (int i = m_navs.size() - 1; i >= 0; --i) {
            if (int p = m_navs[i].rows.indexOf(rows.first()); p >= 0) {
                m_navs.resize(i + 1);
                nav().pos = p;
                break;
            }
        }
        refreshViewer(true);
    } else if (m_stack->currentWidget() == m_compare) {
        if (int p = m_cmp.rows.indexOf(rows.first()); p >= 0) moveCompare(p);
        else refreshCompare();
    }
}

void MainWindow::updateCounts()
{
    const int selected = m_model->countMarked(Mark::Selected);
    const int rejected = m_model->countMarked(Mark::Rejected);
    m_counts->setText(tr("%1 shots, %2 bursts<br><span style='color:#5DF6A4'>%3 selected</span><br>"
                         "<span style='color:#F66C5D'>%4 rejected</span>")
                          .arg(m_model->count()).arg(m_model->burstCount()).arg(selected).arg(rejected));
    m_applyButton->setEnabled(selected + rejected > 0);
}

void MainWindow::applySelection()
{
    ApplyDialog dialog(m_model, this);
    dialog.exec();
    // Reload whatever happened: moved files are gone, the session file drops stale entries.
    openFolder(m_model->folder());
}
