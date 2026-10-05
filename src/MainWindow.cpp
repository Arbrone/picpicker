#include "MainWindow.h"
#include "ApplyDialog.h"
#include "Colors.h"
#include "CompareWidget.h"
#include "Filmstrip.h"
#include "ImageLoader.h"
#include "ShotModel.h"
#include "ThumbDelegate.h"
#include "ViewerWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cstdlib>

namespace {

const char *kHelp = R"(
<table align="center" cellspacing="0" cellpadding="3">
<tr><td colspan="2"><h3>Viewer &amp; compare</h3></td><td width="40"></td><td colspan="2"><h3>Grid</h3></td></tr>
<tr><td><b>← →</b></td><td>previous / next shot</td><td></td><td><b>Enter</b>, double-click</td><td>open in viewer</td></tr>
<tr><td><b>Shift+← →</b>, PgUp/PgDn</td><td>skip the rest of a burst</td><td></td><td><b>S X Space</b></td><td>select / reject / clear selection</td></tr>
<tr><td><b>↑</b> or <b>S</b></td><td>select (then next)</td><td></td><td><b>1–5 0</b>, <b>6–9</b></td><td>stars, colour label</td></tr>
<tr><td><b>↓</b> or <b>X</b></td><td>reject (then next)</td><td></td><td><b>C</b></td><td>compare selection or burst</td></tr>
<tr><td><b>Space</b></td><td>clear mark</td><td></td><td><b>Ctrl+wheel</b>, <b>+ −</b></td><td>thumbnail size</td></tr>
<tr><td><b>1–5</b>, <b>0</b></td><td>star rating, no rating</td><td></td><td></td><td></td></tr>
<tr><td><b>6 7 8 9</b></td><td>red / yellow / green / blue label</td><td></td><td colspan="2"><h3>Anywhere</h3></td></tr>
<tr><td><b>K</b></td><td>keep this frame, reject rest of burst</td><td></td><td><b>Ctrl+Z</b></td><td>undo</td></tr>
<tr><td><b>C</b></td><td>compare burst / next shot</td><td></td><td><b>Tab</b></td><td>show / hide side panel</td></tr>
<tr><td><b>Wheel</b>, <b>+ −</b></td><td>zoom</td><td></td><td><b>F11</b></td><td>full screen</td></tr>
<tr><td><b>Z</b>, double-click</td><td>fit ↔ 100% (on AF point)</td><td></td><td><b>Ctrl+O</b></td><td>open folder</td></tr>
<tr><td><b>Drag</b></td><td>pan when zoomed</td><td></td><td><b>H</b>, <b>?</b></td><td>this help</td></tr>
<tr><td><b>F</b> / <b>R</b></td><td>show AF point / rotate</td><td></td><td></td><td></td></tr>
<tr><td><b>Esc</b></td><td>leave zoom, compare, viewer</td><td></td><td></td><td></td></tr>
</table>
<p align="center" style="color:#aaa">Press any key to close</p>)";

const char *kHintGrid = "Enter open  ·  S / X select / reject  ·  1–5 stars  ·  C compare  ·  Ctrl+wheel size  ·  H help";
const char *kHintViewer = "← → next  ·  Shift+→ skip burst  ·  ↑ ↓ select / reject  ·  K keep  ·  wheel zoom  ·  Esc grid  ·  H help";
const char *kHintCompare = "← → choose  ·  ↑ ↓ select / reject  ·  K keep  ·  wheel / Z zoom all  ·  Esc back  ·  H help";

constexpr int kMaxRecent = 6;

// Full-window key cheat sheet; any key or click closes it.
class HelpOverlay : public QLabel {
public:
    using QLabel::QLabel;
    QPointer<QWidget> returnFocus;

protected:
    void keyPressEvent(QKeyEvent *) override { close(); }
    void mousePressEvent(QMouseEvent *) override { close(); }

private:
    void close()
    {
        hide();
        if (returnFocus) returnFocus->setFocus();
    }
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("PicPicker");
    setAcceptDrops(true);

    m_loader = new ImageLoader(this);
    m_model = new ShotModel(m_loader, this);
    m_filter = new ShotFilter(this);
    m_filter->setSourceModel(m_model);

    // Start page
    m_startPage = new QWidget;
    auto *startLayout = new QVBoxLayout(m_startPage);
    auto *openButton = new QPushButton(tr("Open Folder…"));
    openButton->setMinimumSize(240, 56);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    auto *dropHint = new QLabel(tr("or drop a folder here"));
    dropHint->setStyleSheet("color: #888;");
    m_recentLayout = new QVBoxLayout;
    startLayout->addStretch();
    startLayout->addWidget(openButton, 0, Qt::AlignCenter);
    startLayout->addWidget(dropHint, 0, Qt::AlignCenter);
    startLayout->addSpacing(24);
    startLayout->addLayout(m_recentLayout);
    startLayout->addStretch(2);

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
    m_grid->setSpacing(0);
    m_grid->installEventFilter(this);
    m_grid->viewport()->installEventFilter(this);
    connect(m_grid, &QListView::activated, this, &MainWindow::openViewer);
    // While scrolling, regularly drop queued thumbnails that are no longer near the screen.
    auto *retainTimer = new QTimer(this);
    retainTimer->setSingleShot(true);
    retainTimer->setInterval(60);
    connect(retainTimer, &QTimer::timeout, this, &MainWindow::retainVisibleThumbnails);
    connect(m_grid->verticalScrollBar(), &QScrollBar::valueChanged, retainTimer, [retainTimer] {
        if (!retainTimer->isActive()) retainTimer->start(); // throttle, don't debounce
    });

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

    m_film = new Filmstrip(m_model, m_loader);
    m_film->hide();
    connect(m_film, &Filmstrip::activated, this, [this](int pos) {
        if (m_stack->currentWidget() == m_compare) moveCompare(pos);
        else goTo(pos);
    });

    auto *mainArea = new QWidget;
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_stack, 1);
    mainLayout->addWidget(m_film);

    // Feedback for every edit: with auto-advance the marked shot is gone before you see it.
    m_toast = new QLabel(mainArea);
    m_toast->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_toast->hide();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(1100);
    connect(m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);

    m_side = buildSidePanel();
    auto *central = new QWidget;
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_side);
    layout->addWidget(mainArea, 1);
    setCentralWidget(central);

    m_help = new HelpOverlay(central);
    m_help->setText(tr(kHelp));
    m_help->setTextFormat(Qt::RichText);
    m_help->setAlignment(Qt::AlignCenter);
    m_help->setAutoFillBackground(true);
    m_help->setStyleSheet("background: rgba(15, 15, 15, 235); color: #eee; font-size: 11pt;");
    m_help->setFocusPolicy(Qt::StrongFocus);
    m_help->hide();

    m_hint = new QLabel;
    m_hint->setStyleSheet("color: #888;");
    statusBar()->addPermanentWidget(m_hint);

    // Window-wide shortcuts
    auto shortcut = [this](const QKeySequence &key, auto fn) {
        connect(new QShortcut(key, this), &QShortcut::activated, this, fn);
    };
    shortcut(QKeySequence::Undo, [this] { undo(); });
    shortcut(QKeySequence::Open, [this] { chooseFolder(); });
    shortcut(Qt::Key_Tab, [this] { m_side->setVisible(!m_side->isVisible()); });
    shortcut(Qt::Key_F11, [this] { isFullScreen() ? showNormal() : showFullScreen(); });

    connect(m_model, &ShotModel::changed, this, [this] {
        updateCounts();
        // Marks can change which frame covers a stacked burst.
        if (m_filter->stackBursts()) m_filter->refresh();
    });
    // Loading overlay: bursts, AF points and exposure are only complete once every file's
    // metadata is read, so the window waits for it. The event loop keeps running meanwhile.
    m_loading = new QWidget(this);
    m_loading->setAttribute(Qt::WA_StyledBackground);
    m_loading->setStyleSheet("background: rgba(15, 15, 15, 215); color: #eee;");
    m_loadingLabel = new QLabel;
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setStyleSheet("background: transparent; font-size: 13pt;");
    m_loadingBar = new QProgressBar;
    m_loadingBar->setFixedWidth(420);
    m_loadingBar->setTextVisible(false);
    auto *loadingLayout = new QVBoxLayout(m_loading);
    loadingLayout->addStretch();
    loadingLayout->addWidget(m_loadingLabel, 0, Qt::AlignCenter);
    loadingLayout->addWidget(m_loadingBar, 0, Qt::AlignCenter);
    loadingLayout->addStretch();
    m_loading->hide();
    centralWidget()->installEventFilter(this); // keep the overlay sized to the window
    // Small folders finish in a few milliseconds: don't flash the overlay for those.
    m_loadingDelay = new QTimer(this);
    m_loadingDelay->setSingleShot(true);
    m_loadingDelay->setInterval(150);
    connect(m_loadingDelay, &QTimer::timeout, this, [this] {
        m_loading->setGeometry(centralWidget()->geometry());
        m_loading->show();
        m_loading->raise();
    });

    connect(m_model, &ShotModel::metadataProgress, this, [this](int done, int total) {
        m_loadingLabel->setText(tr("Reading photo info… %1 / %2").arg(done).arg(total));
        m_loadingBar->setRange(0, total);
        m_loadingBar->setValue(done);
    });
    connect(m_model, &ShotModel::metadataFinished, this, [this] {
        setLoading(false);
        if (m_model->burstCount() > 0) toast(tr("%1 shots, %2 bursts").arg(m_model->count()).arg(m_model->burstCount()));
    });
    connect(m_model, &ShotModel::xmpSkipped, this, [this](const QString &path) {
        statusBar()->showMessage(tr("%1 was not written: it belongs to another application or is read-only")
                                     .arg(QFileInfo(path).fileName()), 6000);
    });

    // Restore settings
    QSettings settings;
    if (!restoreGeometry(settings.value("geometry").toByteArray())) resize(1600, 900);
    m_side->setVisible(settings.value("panel", true).toBool());
    m_stackBox->setChecked(settings.value("stack", true).toBool());
    m_advanceBox->setChecked(settings.value("advance", true).toBool());
    setShowFocus(settings.value("showFocus", true).toBool());
    setThumbWidth(settings.value("thumbWidth", 220).toInt());

    rebuildStartPage();
    updateCounts();
    setPage(m_startPage);
}

QWidget *MainWindow::buildSidePanel()
{
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
    connect(m_stackBox, &QCheckBox::toggled, this, [this](bool on) {
        m_delegate->setStacked(on);
        m_filter->setStackBursts(on);
    });
    m_focusBox = new QCheckBox(tr("Show AF point (F)"));
    connect(m_focusBox, &QCheckBox::toggled, this, &MainWindow::setShowFocus);
    m_advanceBox = new QCheckBox(tr("Next shot after marking"));

    m_counts = new QLabel;
    m_counts->setTextFormat(Qt::RichText);
    auto *helpButton = new QPushButton(tr("Keyboard shortcuts (H)"));
    helpButton->setFlat(true);
    connect(helpButton, &QPushButton::clicked, this, &MainWindow::toggleHelp);

    m_applyButton = new QPushButton(tr("Apply…"));
    m_applyButton->setMinimumHeight(40);
    m_applyButton->setToolTip(tr("Move selected and rejected shots into their folders"));
    connect(m_applyButton, &QPushButton::clicked, this, &MainWindow::applySelection);

    auto *side = new QWidget;
    side->setFixedWidth(230);
    auto *l = new QVBoxLayout(side);
    l->addWidget(folderButton);
    l->addSpacing(12);
    l->addWidget(new QLabel(tr("Show:")));
    l->addWidget(m_filterCombo);
    l->addWidget(m_ratingCombo);
    l->addWidget(m_stackBox);
    l->addSpacing(12);
    l->addWidget(m_focusBox);
    l->addWidget(m_advanceBox);
    l->addSpacing(12);
    l->addWidget(m_counts);
    l->addStretch();
    l->addWidget(helpButton);
    l->addWidget(m_applyButton);
    return side;
}

void MainWindow::rebuildStartPage()
{
    while (QLayoutItem *item = m_recentLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QStringList recent = QSettings().value("recent").toStringList();
    if (recent.isEmpty()) return;
    auto *title = new QLabel(tr("Recent folders"));
    title->setStyleSheet("color: #888;");
    m_recentLayout->addWidget(title, 0, Qt::AlignCenter);
    for (const QString &dir : recent) {
        auto *button = new QPushButton(dir);
        button->setFlat(true);
        button->setEnabled(QFileInfo(dir).isDir());
        connect(button, &QPushButton::clicked, this, [this, dir] { openFolder(dir); });
        m_recentLayout->addWidget(button, 0, Qt::AlignCenter);
    }
}

void MainWindow::setPage(QWidget *page)
{
    m_stack->setCurrentWidget(page);
    m_film->setVisible(page == m_viewer || page == m_compare);
    m_hint->setText(page == m_grid ? tr(kHintGrid) : page == m_viewer ? tr(kHintViewer)
                    : page == m_compare ? tr(kHintCompare) : QString());
    page->setFocus();
}

void MainWindow::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"), m_model->folder());
    if (!dir.isEmpty()) openFolder(dir);
}

void MainWindow::openFolder(const QString &folder)
{
    const QString path = QFileInfo(folder).absoluteFilePath();
    m_rows.clear();
    m_pos = -1;
    m_cmp = {};
    m_model->load(path);
    setLoading(m_model->metadataLoading());
    setWindowTitle(QStringLiteral("PicPicker — %1").arg(path));

    QSettings settings;
    QStringList recent = settings.value("recent").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    settings.setValue("recent", recent.mid(0, kMaxRecent));
    rebuildStartPage();

    setPage(m_grid);
    if (m_filter->rowCount() > 0) m_grid->setCurrentIndex(m_filter->index(0, 0));
    else toast(tr("No JPG or RAF files in this folder"));
}

QVector<int> MainWindow::viewerRows() const
{
    QVector<int> rows;
    for (int i = 0; i < m_filter->rowCount(); ++i) {
        const int row = m_filter->mapToSource(m_filter->index(i, 0)).row();
        const int burst = m_model->shot(row).burst;
        if (!m_filter->stackBursts() || burst < 0) {
            rows << row;
            continue;
        }
        for (int frame : m_model->burstRows(burst))
            if (m_filter->accepts(m_model->shot(frame))) rows << frame;
    }
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
    return m_pos >= 0 && m_pos < m_rows.size() ? m_rows[m_pos] : -1;
}

void MainWindow::toast(const QString &text, const QColor &color)
{
    m_toast->setText(text);
    m_toast->setStyleSheet(QStringLiteral("background: rgba(0,0,0,200); color: %1; padding: 8px 18px;"
                                          "border-radius: 6px; font-size: 13pt; font-weight: bold;")
                               .arg(color.name()));
    m_toast->adjustSize();
    m_toast->move((m_toast->parentWidget()->width() - m_toast->width()) / 2, 16);
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start();
}

void MainWindow::setLoading(bool loading)
{
    // Disabling the central widget blocks mouse and keyboard input; the overlay isn't part of it.
    centralWidget()->setEnabled(!loading);
    if (loading) {
        m_loadingLabel->setText(tr("Reading photo info…"));
        m_loadingBar->setRange(0, 0);
        m_loadingDelay->start();
        return;
    }
    m_loadingDelay->stop();
    m_loading->hide();
    m_stack->currentWidget()->setFocus();
}

void MainWindow::toggleHelp()
{
    if (m_help->isVisible()) {
        m_help->hide();
        return;
    }
    static_cast<HelpOverlay *>(m_help)->returnFocus = m_stack->currentWidget();
    m_help->setGeometry(centralWidget()->rect());
    m_help->show();
    m_help->raise();
    m_help->setFocus();
}

// ---- Grid ------------------------------------------------------------------------------------

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == centralWidget() && event->type() == QEvent::Resize)
        m_loading->setGeometry(centralWidget()->geometry());
    if (watched == m_grid->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            setThumbWidth(m_delegate->cellWidth() + (wheel->angleDelta().y() > 0 ? 20 : -20));
            return true;
        }
    }
    if (watched == m_grid && event->type() == QEvent::KeyPress) {
        if (!m_grid->isEnabled()) return true; // the window is blocked while loading
        auto *key = static_cast<QKeyEvent *>(event);
        // Arrows, Enter and Esc keep their usual grid behaviour.
        switch (key->key()) {
        case Qt::Key_Up: case Qt::Key_Down: case Qt::Key_Left: case Qt::Key_Right:
        case Qt::Key_Home: case Qt::Key_End: case Qt::Key_PageUp: case Qt::Key_PageDown:
        case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Escape:
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

void MainWindow::retainVisibleThumbnails()
{
    if (m_filter->rowCount() == 0) return;
    const QRect r = m_grid->viewport()->rect();
    auto rowAt = [&](QPoint p, int fallback) {
        const QModelIndex idx = m_grid->indexAt(p);
        return idx.isValid() ? idx.row() : fallback;
    };
    const int count = m_filter->rowCount();
    const int firstVisible = rowAt(r.topLeft() + QPoint(1, 1), 0);
    const int lastVisible = rowAt(r.bottomRight() - QPoint(1, 1), count - 1);
    // Keep half a screen of margin above and one below (you usually keep scrolling down).
    const int first = rowAt(r.topLeft() - QPoint(0, r.height() / 2) + QPoint(1, 1), 0);
    const int last = rowAt(r.bottomRight() + QPoint(0, r.height()) - QPoint(1, 1), count - 1);
    auto shotAt = [&](int i) -> const Shot & { return m_model->shot(m_filter->mapToSource(m_filter->index(i, 0)).row()); };
    QSet<QString> keep;
    for (int i = first; i <= last; ++i) keep.insert(shotAt(i).displayPath());
    m_loader->retainThumbnails(keep);
    // What's on screen goes before the margin, top to bottom.
    for (int i = firstVisible; i <= lastVisible; ++i)
        m_loader->thumbnail(shotAt(i), ImageLoader::UrgentPriority / 2 - (i - firstVisible));
}

void MainWindow::setThumbWidth(int width)
{
    m_delegate->setCellWidth(width);
    m_grid->setGridSize(m_delegate->cellSize());
    if (m_grid->currentIndex().isValid()) m_grid->scrollTo(m_grid->currentIndex());
}

bool MainWindow::gridAction(KeyAction a)
{
    const QVector<int> rows = gridSelection();
    switch (a.action) {
    case Action::Select: case Action::Reject: case Action::ClearMark: case Action::Rate: case Action::Label:
        applyEdit(rows, a);
        return true;
    case Action::ZoomIn:
    case Action::ZoomOut:
        setThumbWidth(m_delegate->cellWidth() + (a.action == Action::ZoomIn ? 40 : -40));
        return true;
    case Action::Help:
        toggleHelp();
        return true;
    case Action::Compare:
        if (rows.size() < 2) {
            toast(tr("Select 2 or more shots, or a burst, to compare"));
            return true;
        }
        m_cmp.returnTo = m_grid;
        openCompare(rows, 0, std::min<int>(rows.size(), CompareWidget::MaxPanes), true);
        return true;
    default:
        return false;
    }
}

void MainWindow::applyEdit(const QVector<int> &rows, KeyAction a)
{
    if (rows.isEmpty()) return;
    const QString count = rows.size() > 1 ? tr("  (%1 shots)").arg(rows.size()) : QString();
    switch (a.action) {
    case Action::Select:
        m_model->setMark(rows, Mark::Selected);
        toast(tr("✓ Selected") + count, markColor(Mark::Selected));
        break;
    case Action::Reject:
        m_model->setMark(rows, Mark::Rejected);
        toast(tr("✕ Rejected") + count, markColor(Mark::Rejected));
        break;
    case Action::ClearMark:
        m_model->setMark(rows, Mark::None);
        toast(tr("Mark cleared") + count);
        break;
    case Action::Rate:
        m_model->setRating(rows, a.arg);
        toast((a.arg ? stars(a.arg) : tr("No rating")) + count, QColor(0xFF, 0xD2, 0x3F));
        break;
    case Action::Label: {
        m_model->toggleLabel(rows, Label(a.arg));
        const Label now = m_model->shot(rows.first()).label;
        toast(now == Label::None ? tr("Label removed") + count : tr("● %1").arg(labelName(now)) + count,
              now == Label::None ? QColor(Qt::white) : labelColor(now));
        break;
    }
    default:
        break;
    }
}

void MainWindow::showGrid(int focusRow)
{
    setPage(m_grid);
    if (focusRow < 0) return;
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

// ---- Viewer ----------------------------------------------------------------------------------

void MainWindow::openViewer(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;
    m_rows = viewerRows();
    m_pos = std::max(0, int(m_rows.indexOf(m_filter->mapToSource(proxyIndex).row())));
    setPage(m_viewer);
    refreshViewer(true);
}

void MainWindow::goTo(int position)
{
    if (position < 0 || position >= m_rows.size() || position == m_pos) return;
    m_pos = position;
    refreshViewer(true);
}

int MainWindow::groupStart(int position) const
{
    const int burst = m_model->shot(m_rows[position]).burst;
    while (burst >= 0 && position > 0 && m_model->shot(m_rows[position - 1]).burst == burst) --position;
    return position;
}

int MainWindow::groupEnd(int position) const
{
    const int burst = m_model->shot(m_rows[position]).burst;
    while (burst >= 0 && position + 1 < m_rows.size() && m_model->shot(m_rows[position + 1]).burst == burst) ++position;
    return position;
}

void MainWindow::viewerAction(KeyAction a)
{
    const int row = currentRow();
    if (row < 0) return;
    const int burst = m_model->shot(row).burst;
    const bool atEnd = m_pos + 1 >= m_rows.size();
    switch (a.action) {
    case Action::Next:
        if (atEnd) toast(tr("Last shot"));
        goTo(m_pos + 1);
        break;
    case Action::Prev:
        if (m_pos == 0) toast(tr("First shot"));
        goTo(m_pos - 1);
        break;
    case Action::NextGroup: goTo(std::min<int>(groupEnd(m_pos) + 1, m_rows.size() - 1)); break;
    case Action::PrevGroup: {
        const int start = groupStart(m_pos);
        goTo(start > 0 ? groupStart(start - 1) : 0);
        break;
    }
    case Action::First: goTo(0); break;
    case Action::Last: goTo(m_rows.size() - 1); break;
    case Action::Select:
    case Action::Reject:
        applyEdit({row}, a);
        if (m_advanceBox->isChecked() && !atEnd) goTo(m_pos + 1);
        else refreshViewer(false);
        break;
    case Action::ClearMark:
    case Action::Rate:
    case Action::Label:
        applyEdit({row}, a);
        refreshViewer(false);
        break;
    case Action::Keep: {
        if (burst < 0) return viewerAction({Action::Select});
        const QVector<int> &frames = m_model->burstRows(burst);
        m_model->keep(row, frames);
        toast(tr("✓ Kept %1, rejected %2 other frames").arg(m_model->shot(row).stem).arg(frames.size() - 1),
              markColor(Mark::Selected));
        const int next = groupEnd(m_pos) + 1;
        if (next < m_rows.size()) goTo(next);
        else refreshViewer(false);
        break;
    }
    case Action::Back:
        showGrid(row);
        break;
    case Action::Compare:
        m_cmp.returnTo = m_viewer;
        if (burst >= 0) {
            const QVector<int> &frames = m_model->burstRows(burst);
            openCompare(frames, int(frames.indexOf(row)), std::min<int>(frames.size(), CompareWidget::MaxPanes), true);
        } else if (m_rows.size() > 1) {
            openCompare(m_rows, m_pos, 2, false);
        }
        break;
    case Action::ToggleFocus:
        setShowFocus(!m_showFocus);
        toast(m_showFocus ? tr("AF point shown") : tr("AF point hidden"));
        break;
    case Action::Help:
        toggleHelp();
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
    m_viewer->setOverlay(overlayText(row, m_pos, m_rows.size()), shot.mark, shot.label);
    m_film->setRows(m_rows, m_pos);

    // Prefetch the shots you're most likely to see next, and drop queued decodes of shots you
    // already skipped past (holding → would otherwise leave a backlog in front of this one).
    QSet<QString> wanted{shot.stem};
    for (int offset : {1, 2, 3, -1}) {
        const int p = m_pos + offset;
        if (p >= 0 && p < m_rows.size()) wanted.insert(m_model->shot(m_rows[p]).stem);
    }
    m_loader->retainPreviews(wanted);
    for (int offset : {1, 2, 3, -1}) {
        const int p = m_pos + offset;
        if (p >= 0 && p < m_rows.size()) m_loader->request(m_model->shot(m_rows[p]), false, 5 - std::abs(offset));
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
    setPage(m_compare);
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
        pane->setOverlay(overlayText(row, pos, m_cmp.rows.size()), shot.mark, shot.label);
        m_cmp.shown[i] = row;
    }
    m_film->setRows(m_cmp.rows, m_cmp.active, m_cmp.first, m_cmp.panes);
    // Prefetch the next window; drop queued decodes for windows already passed.
    const int prefetchEnd = std::min<int>(m_cmp.rows.size(), m_cmp.first + 2 * m_cmp.panes);
    QSet<QString> wanted;
    for (int p = m_cmp.first; p < prefetchEnd; ++p) wanted.insert(m_model->shot(m_cmp.rows[p]).stem);
    m_loader->retainPreviews(wanted);
    for (int p = m_cmp.first + m_cmp.panes; p < prefetchEnd; ++p)
        m_loader->request(m_model->shot(m_cmp.rows[p]), false, 3);
}

void MainWindow::closeCompare()
{
    const int row = m_cmp.rows.value(m_cmp.active, -1);
    if (m_cmp.returnTo == m_viewer && !m_rows.isEmpty()) {
        if (int p = m_rows.indexOf(row); p >= 0) m_pos = p;
        setPage(m_viewer);
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
    case Action::Next:
    case Action::NextGroup: moveCompare(m_cmp.active + 1); break;
    case Action::Prev:
    case Action::PrevGroup: moveCompare(m_cmp.active - 1); break;
    case Action::First: moveCompare(0); break;
    case Action::Last: moveCompare(m_cmp.rows.size() - 1); break;
    case Action::Select:
    case Action::Reject:
        applyEdit({row}, a);
        if (m_advanceBox->isChecked()) moveCompare(m_cmp.active + 1);
        else refreshCompare();
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
        toast(tr("✓ Kept %1, rejected %2 others").arg(m_model->shot(row).stem).arg(group.size() - 1),
              markColor(Mark::Selected));
        closeCompare();
        // Continue after the group that was just decided.
        if (m_stack->currentWidget() == m_viewer) {
            int last = m_pos;
            for (int r : group) last = std::max(last, int(m_rows.indexOf(r)));
            goTo(std::min<int>(last + 1, m_rows.size() - 1));
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
    case Action::Help:
        toggleHelp();
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
    if (!m_model->undo(&rows) || rows.isEmpty()) {
        toast(tr("Nothing to undo"));
        return;
    }
    toast(tr("↶ Undone"));
    if (m_stack->currentWidget() == m_viewer) {
        // Jump back to the shot that changed.
        if (int p = m_rows.indexOf(rows.first()); p >= 0) m_pos = p;
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
                         "<span style='color:#F66C5D'>%4 rejected</span><br>%5 to go")
                          .arg(m_model->count()).arg(m_model->burstCount()).arg(selected).arg(rejected)
                          .arg(m_model->count() - selected - rejected));
    m_applyButton->setEnabled(selected + rejected > 0);
}

void MainWindow::applySelection()
{
    ApplyDialog dialog(m_model, this);
    dialog.exec();
    // Reload whatever happened: moved files are gone, the session file drops stale entries.
    openFolder(m_model->folder());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("panel", m_side->isVisible());
    settings.setValue("stack", m_stackBox->isChecked());
    settings.setValue("advance", m_advanceBox->isChecked());
    settings.setValue("showFocus", m_showFocus);
    settings.setValue("thumbWidth", m_delegate->cellWidth());
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile()) return;
    const QFileInfo fi(urls.first().toLocalFile());
    openFolder(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath());
}
