#pragma once

#include "Actions.h"
#include "Shot.h"

#include <QMainWindow>
#include <QVector>

class CompareWidget;
class Filmstrip;
class ImageLoader;
class QCheckBox;
class QComboBox;
class QLabel;
class QListView;
class QPushButton;
class QStackedWidget;
class QProgressBar;
class QTimer;
class QVBoxLayout;
class ShotFilter;
class ShotModel;
class ThumbDelegate;
class ViewerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openFolder(const QString &folder);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    struct CompareState {
        QVector<int> rows;      // everything Left/Right can reach
        QVector<int> shown;     // rows displayed in each pane last time
        int active = 0;         // index into rows
        int first = 0;          // rows[first] is in pane 0
        int panes = 2;
        bool keepWholeSet = false; // K rejects all rows, not only the visible panes
        QWidget *returnTo = nullptr;
    };

    QWidget *buildSidePanel();
    void rebuildStartPage();
    void setPage(QWidget *page);
    void chooseFolder();
    void openViewer(const QModelIndex &proxyIndex);
    void showGrid(int focusRow);
    void viewerAction(KeyAction a);
    void compareAction(KeyAction a);
    bool gridAction(KeyAction a);
    void applyEdit(const QVector<int> &rows, KeyAction a);
    void goTo(int position);
    int groupStart(int position) const;
    int groupEnd(int position) const;
    void refreshViewer(bool newShot);
    void openCompare(const QVector<int> &rows, int active, int panes, bool keepWholeSet);
    void moveCompare(int active);
    void closeCompare();
    void refreshCompare();
    void undo();
    void applySelection();
    void updateCounts();
    void setShowFocus(bool show);
    void setThumbWidth(int width);
    void retainVisibleThumbnails();
    void toggleHelp();
    void setLoading(bool loading);
    void toast(const QString &text, const QColor &color = Qt::white);

    QImage bestImage(const Shot &shot, bool zoomed);
    QString overlayText(int row, int pos, int total) const;
    QVector<int> viewerRows() const;    // visible shots, stacked bursts expanded to their frames
    QVector<int> gridSelection() const; // stacked bursts expand to all their frames
    int currentRow() const;

    ImageLoader *m_loader;
    ShotModel *m_model;
    ShotFilter *m_filter;
    ThumbDelegate *m_delegate;
    QStackedWidget *m_stack;
    QWidget *m_startPage;
    QVBoxLayout *m_recentLayout;
    QListView *m_grid;
    ViewerWidget *m_viewer;
    CompareWidget *m_compare;
    Filmstrip *m_film;
    QWidget *m_side;
    QComboBox *m_filterCombo;
    QComboBox *m_ratingCombo;
    QCheckBox *m_stackBox;
    QCheckBox *m_focusBox;
    QCheckBox *m_advanceBox;
    QLabel *m_counts;
    QLabel *m_hint;
    QLabel *m_toast;
    QTimer *m_toastTimer;
    QLabel *m_help;
    QWidget *m_loading;          // blocks the window while the folder's metadata is read
    QLabel *m_loadingLabel;
    QProgressBar *m_loadingBar;
    QTimer *m_loadingDelay;
    QPushButton *m_applyButton;

    // Viewer navigation over a snapshot of source rows, so marking a shot under an active
    // filter doesn't reshuffle what Left/Right do.
    QVector<int> m_rows;
    int m_pos = -1;
    CompareState m_cmp;
    bool m_showFocus = true;
};
