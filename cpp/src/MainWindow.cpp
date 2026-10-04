#include "MainWindow.h"
#include "ApplyDialog.h"
#include "ImageLoader.h"
#include "ShotModel.h"
#include "ThumbDelegate.h"
#include "ViewerWidget.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cstdlib>

namespace {
const char *kHelp =
    "<b>Viewer</b><br>"
    "← / → &nbsp;previous / next<br>"
    "↑ or S &nbsp;select<br>"
    "↓ or X &nbsp;reject<br>"
    "Space &nbsp;clear mark<br>"
    "Z / double-click &nbsp;100% zoom<br>"
    "R &nbsp;rotate<br>"
    "Esc &nbsp;back to grid<br><br>"
    "<b>Grid</b><br>"
    "Enter / double-click &nbsp;open<br>"
    "S / X / Space &nbsp;mark selection<br><br>"
    "Ctrl+Z &nbsp;undo last mark";
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
    m_grid->setModel(m_filter);
    m_grid->setItemDelegate(new ThumbDelegate(m_grid));
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setGridSize({ThumbDelegate::CellWidth, ThumbDelegate::CellHeight});
    m_grid->setSpacing(0);
    connect(m_grid, &QListView::activated, this, &MainWindow::openViewer);

    auto gridKey = [this](QKeySequence key, Mark mark) {
        auto *sc = new QShortcut(key, m_grid, nullptr, nullptr, Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, [this, mark] { markGridSelection(mark); });
    };
    gridKey(Qt::Key_S, Mark::Selected);
    gridKey(Qt::Key_X, Mark::Rejected);
    gridKey(Qt::Key_Space, Mark::None);

    // Viewer
    m_viewer = new ViewerWidget;
    connect(m_viewer, &ViewerWidget::nextRequested, this, [this] { goTo(m_pos + 1); });
    connect(m_viewer, &ViewerWidget::prevRequested, this, [this] { goTo(m_pos - 1); });
    connect(m_viewer, &ViewerWidget::firstRequested, this, [this] { goTo(0); });
    connect(m_viewer, &ViewerWidget::lastRequested, this, [this] { goTo(m_nav.size() - 1); });
    connect(m_viewer, &ViewerWidget::markRequested, this, &MainWindow::markCurrent);
    connect(m_viewer, &ViewerWidget::backRequested, this, &MainWindow::showGrid);
    connect(m_viewer, &ViewerWidget::zoomToggled, this, [this] { showCurrent(false); });

    connect(m_loader, &ImageLoader::previewReady, this, [this](const QString &stem) {
        const int row = currentRow();
        if (m_stack->currentWidget() == m_viewer && row >= 0 && m_model->shot(row).stem == stem)
            showCurrent(false);
    });

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_startPage);
    m_stack->addWidget(m_grid);
    m_stack->addWidget(m_viewer);

    // Side panel
    auto *folderButton = new QPushButton(tr("Open Folder…"));
    connect(folderButton, &QPushButton::clicked, this, &MainWindow::chooseFolder);

    m_filterCombo = new QComboBox;
    m_filterCombo->addItems({tr("All"), tr("Unmarked"), tr("Selected"), tr("Rejected")});
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this,
            [this](int i) { m_filter->setFilter(ShotFilter::Filter(i)); });

    m_counts = new QLabel;
    m_counts->setTextFormat(Qt::RichText);
    auto *help = new QLabel(tr(kHelp));
    help->setTextFormat(Qt::RichText);
    help->setStyleSheet("color: #999;");

    m_applyButton = new QPushButton(tr("Apply…"));
    m_applyButton->setMinimumHeight(40);
    connect(m_applyButton, &QPushButton::clicked, this, &MainWindow::applySelection);

    auto *side = new QWidget;
    side->setFixedWidth(220);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->addWidget(folderButton);
    sideLayout->addSpacing(12);
    sideLayout->addWidget(new QLabel(tr("Show:")));
    sideLayout->addWidget(m_filterCombo);
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

    connect(m_model, &ShotModel::marksChanged, this, &MainWindow::updateCounts);
    updateCounts();
}

void MainWindow::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"), m_model->folder());
    if (!dir.isEmpty()) openFolder(dir);
}

void MainWindow::openFolder(const QString &folder)
{
    m_nav.clear();
    m_pos = -1;
    m_model->load(QFileInfo(folder).absoluteFilePath());
    setWindowTitle(QStringLiteral("PicPicker — %1").arg(m_model->folder()));
    m_stack->setCurrentWidget(m_grid);
    m_grid->setFocus();
    if (m_filter->rowCount() > 0) m_grid->setCurrentIndex(m_filter->index(0, 0));
}

int MainWindow::currentRow() const
{
    return m_pos >= 0 && m_pos < m_nav.size() ? m_nav[m_pos] : -1;
}

void MainWindow::openViewer(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;
    m_nav.clear();
    for (int i = 0; i < m_filter->rowCount(); ++i)
        m_nav << m_filter->mapToSource(m_filter->index(i, 0)).row();
    m_pos = proxyIndex.row();
    m_stack->setCurrentWidget(m_viewer);
    m_viewer->setFocus();
    showCurrent(true);
}

void MainWindow::showGrid()
{
    m_stack->setCurrentWidget(m_grid);
    const int row = currentRow();
    if (row >= 0) {
        const QModelIndex idx = m_filter->mapFromSource(m_model->index(row));
        if (idx.isValid()) {
            m_grid->setCurrentIndex(idx);
            m_grid->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        }
    }
    m_grid->setFocus();
}

void MainWindow::goTo(int position)
{
    if (position < 0 || position >= m_nav.size() || position == m_pos) return;
    m_pos = position;
    showCurrent(true);
}

void MainWindow::showCurrent(bool newShot)
{
    const int row = currentRow();
    if (row < 0) return;
    const Shot &shot = m_model->shot(row);

    // Best available image: full-res when zoomed, then screen-sized preview, then the thumbnail.
    QImage img;
    if (m_viewer->zoomed()) img = m_loader->preview(shot, true);
    if (img.isNull()) img = m_loader->preview(shot, false);
    if (img.isNull()) img = m_loader->thumbnail(shot);
    if (!img.isNull() || newShot) m_viewer->setImage(img, newShot);

    QString files = shot.jpgPath.isEmpty() ? "RAF" : shot.rafPath.isEmpty() ? "JPG" : "JPG+RAF";
    QString text = QStringLiteral("%1   %2/%3   %4").arg(shot.stem).arg(m_pos + 1).arg(m_nav.size()).arg(files);
    const QString info = m_loader->info(shot.stem);
    if (!info.isEmpty()) text += "   " + info;
    m_viewer->setOverlay(text, shot.mark);

    // Prefetch the shots you're most likely to see next.
    for (int offset : {1, 2, 3, -1}) {
        const int p = m_pos + offset;
        if (p >= 0 && p < m_nav.size()) m_loader->request(m_model->shot(m_nav[p]), false, 5 - std::abs(offset));
    }
}

void MainWindow::markCurrent(Mark mark)
{
    const int row = currentRow();
    if (row < 0) return;
    m_model->setMark(row, mark);
    if (mark != Mark::None && m_pos + 1 < m_nav.size()) goTo(m_pos + 1);
    else showCurrent(false);
}

void MainWindow::markGridSelection(Mark mark)
{
    // Map to source rows first: marking can remove rows from a filtered proxy.
    QVector<int> rows;
    for (const QModelIndex &idx : m_grid->selectionModel()->selectedIndexes())
        rows << m_filter->mapToSource(idx).row();
    for (int row : rows) m_model->setMark(row, mark);
}

void MainWindow::undo()
{
    int row = -1;
    if (!m_model->undo(&row)) return;
    if (m_stack->currentWidget() == m_viewer) {
        const int pos = m_nav.indexOf(row);
        if (pos >= 0) {
            m_pos = pos;
            showCurrent(true);
        }
    }
}

void MainWindow::updateCounts()
{
    const int selected = m_model->countMarked(Mark::Selected);
    const int rejected = m_model->countMarked(Mark::Rejected);
    m_counts->setText(tr("%1 shots<br><span style='color:#5DF6A4'>%2 selected</span><br>"
                         "<span style='color:#F66C5D'>%3 rejected</span>")
                          .arg(m_model->count()).arg(selected).arg(rejected));
    m_applyButton->setEnabled(selected + rejected > 0);
}

void MainWindow::applySelection()
{
    ApplyDialog dialog(m_model, this);
    dialog.exec();
    // Reload whatever happened: moved files are gone, the session file drops stale entries.
    openFolder(m_model->folder());
}
