#include "ApplyDialog.h"
#include "ShotModel.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

ApplyDialog::ApplyDialog(ShotModel *model, QWidget *parent)
    : QDialog(parent), m_model(model)
{
    setWindowTitle(tr("Apply selection"));
    resize(640, 300);

    const QDir folder(model->folder());
    m_selectedEdit = new QLineEdit(folder.filePath("selected"));
    m_rejectedEdit = new QLineEdit(folder.filePath("rejected"));
    m_trashBox = new QCheckBox(tr("Send rejected shots to the Trash instead"));

    auto row = [this](QLineEdit *edit) {
        auto *layout = new QHBoxLayout;
        auto *button = new QPushButton(tr("Change…"));
        connect(button, &QPushButton::clicked, this, [this, edit] { browse(edit); });
        layout->addWidget(edit);
        layout->addWidget(button);
        return layout;
    };

    auto *form = new QFormLayout;
    form->addRow(tr("Selected (%1):").arg(model->countMarked(Mark::Selected)), row(m_selectedEdit));
    form->addRow(tr("Rejected (%1):").arg(model->countMarked(Mark::Rejected)), row(m_rejectedEdit));
    form->addRow(QString(), m_trashBox);
    connect(m_trashBox, &QCheckBox::toggled, m_rejectedEdit, &QWidget::setDisabled);

    m_errors = new QPlainTextEdit;
    m_errors->setReadOnly(true);
    m_errors->hide();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    m_applyButton = buttons->addButton(tr("Move files"), QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &ApplyDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_errors, 1);
    layout->addStretch();
    layout->addWidget(buttons);
}

void ApplyDialog::browse(QLineEdit *edit)
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose folder"), edit->text());
    if (!dir.isEmpty()) edit->setText(dir);
}

QStringList ApplyDialog::moveAll(const QStringList &files, const QString &dir, bool toTrash) const
{
    QStringList errors;
    if (!toTrash && !QDir().mkpath(dir)) return {tr("Cannot create folder %1").arg(dir)};
    for (const QString &src : files) {
        if (toTrash) {
            if (!QFile::moveToTrash(src)) errors << tr("Cannot move %1 to the Trash").arg(src);
            continue;
        }
        const QString dst = QDir(dir).filePath(QFileInfo(src).fileName());
        if (QFileInfo::exists(dst)) {
            errors << tr("%1 already exists, skipped").arg(dst);
            continue;
        }
        // QFile::rename falls back to copy + remove across filesystems.
        QFile file(src);
        if (!file.rename(dst)) errors << tr("Cannot move %1: %2").arg(src, file.errorString());
    }
    return errors;
}

void ApplyDialog::apply()
{
    QStringList selected, rejected;
    for (int i = 0; i < m_model->count(); ++i) {
        const Shot &s = m_model->shot(i);
        if (s.mark == Mark::Selected) selected << s.files();
        else if (s.mark == Mark::Rejected) rejected << s.files();
    }

    QStringList errors = moveAll(selected, m_selectedEdit->text(), false);
    errors << moveAll(rejected, m_rejectedEdit->text(), m_trashBox->isChecked());

    if (errors.isEmpty()) {
        accept();
        return;
    }
    // Files that did move are gone from the folder; the caller reloads in any case.
    m_errors->setPlainText(errors.join('\n'));
    m_errors->show();
    m_applyButton->setEnabled(false);
}
