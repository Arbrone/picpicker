#pragma once

#include "Actions.h"
#include "Shot.h"

#include <QMainWindow>
#include <QVector>

class CompareWidget;
class ImageLoader;
class QCheckBox;
class QComboBox;
class QLabel;
class QListView;
class QPushButton;
class QStackedWidget;
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

private:
    // Viewer navigation over a snapshot of source rows, so marking a shot under an active
    // filter doesn't reshuffle what Left/Right do. Entering a burst pushes a second level.
    struct Nav {
        QVector<int> rows;
        int pos = 0;
    };
    struct CompareState {
        QVector<int> rows;      // everything Left/Right can reach
        QVector<int> shown;     // rows displayed in each pane last time
        int active = 0;         // index into rows
        int first = 0;          // rows[first] is in pane 0
        int panes = 2;
        bool keepWholeSet = false; // K rejects all rows, not only the visible panes
        QWidget *returnTo = nullptr;
    };

    void chooseFolder();
    void openViewer(const QModelIndex &proxyIndex);
    void showGrid(int focusRow);
    void viewerAction(KeyAction a);
    void compareAction(KeyAction a);
    bool gridAction(KeyAction a);
    void applyEdit(const QVector<int> &rows, KeyAction a);
    void goTo(int position);
    void refreshViewer(bool newShot);
    void openCompare(const QVector<int> &rows, int active, int panes, bool keepWholeSet);
    void moveCompare(int active);
    void closeCompare();
    void refreshCompare();
    void advancePastBurst();
    void undo();
    void applySelection();
    void updateCounts();
    void setShowFocus(bool show);

    QImage bestImage(const Shot &shot, bool zoomed);
    QString overlayText(int row, int pos, int total) const;
    QVector<int> visibleRows() const;
    QVector<int> gridSelection() const; // stacked bursts expand to all their frames
    int currentRow() const;
    Nav &nav() { return m_navs.last(); }

    ImageLoader *m_loader;
    ShotModel *m_model;
    ShotFilter *m_filter;
    ThumbDelegate *m_delegate;
    QStackedWidget *m_stack;
    QWidget *m_startPage;
    QListView *m_grid;
    ViewerWidget *m_viewer;
    CompareWidget *m_compare;
    QComboBox *m_filterCombo;
    QComboBox *m_ratingCombo;
    QCheckBox *m_stackBox;
    QCheckBox *m_focusBox;
    QLabel *m_counts;
    QPushButton *m_applyButton;

    QVector<Nav> m_navs;
    CompareState m_cmp;
    bool m_showFocus = true;
};
