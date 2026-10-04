#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class ShotModel;

// Moves every file of each marked shot into the selected / rejected folders.
class ApplyDialog : public QDialog {
    Q_OBJECT
public:
    ApplyDialog(ShotModel *model, QWidget *parent = nullptr);

private:
    void apply();
    void browse(QLineEdit *edit);
    QStringList moveAll(const QStringList &files, const QString &dir, bool toTrash) const;

    ShotModel *m_model;
    QLineEdit *m_selectedEdit;
    QLineEdit *m_rejectedEdit;
    QCheckBox *m_trashBox;
    QPlainTextEdit *m_errors;
    QPushButton *m_applyButton;
};
