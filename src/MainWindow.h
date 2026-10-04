#pragma once

#include "Shot.h"

#include <QMainWindow>
#include <QVector>

class ImageLoader;
class QComboBox;
class QLabel;
class QListView;
class QPushButton;
class QStackedWidget;
class ShotFilter;
class ShotModel;
class ViewerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openFolder(const QString &folder);

private:
    void chooseFolder();
    void openViewer(const QModelIndex &proxyIndex);
    void showGrid();
    void showCurrent(bool newShot);
    void goTo(int position);
    void markCurrent(Mark mark);
    void markGridSelection(Mark mark);
    void undo();
    void applySelection();
    void updateCounts();
    int currentRow() const;

    ImageLoader *m_loader;
    ShotModel *m_model;
    ShotFilter *m_filter;
    QStackedWidget *m_stack;
    QWidget *m_startPage;
    QListView *m_grid;
    ViewerWidget *m_viewer;
    QComboBox *m_filterCombo;
    QLabel *m_counts;
    QPushButton *m_applyButton;

    // Viewer navigation: snapshot of source rows taken when the viewer opens, so that
    // marking a shot under an active filter doesn't reshuffle what Left/Right do.
    QVector<int> m_nav;
    int m_pos = -1;
};
