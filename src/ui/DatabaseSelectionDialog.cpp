#include "DatabaseSelectionDialog.h"
#include "database/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFrame>

namespace MSMS {

DatabaseSelectionDialog::DatabaseSelectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("MSMS Manager — Select Database"));
    setMinimumWidth(480);
    setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Title / Header
    QLabel *titleLabel = new QLabel(tr("MSMS Manager"), this);
    titleLabel->setProperty("heading", true);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    mainLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel(
        tr("Welcome to MSMS Manager. An active SQLite database is required to manage membership and society records.\n\nPlease choose an option below to get started:"),
        this
    );
    descLabel->setWordWrap(true);
    descLabel->setProperty("subheading", true);
    mainLayout->addWidget(descLabel);

    // Options Frame
    QFrame *optionsFrame = new QFrame(this);
    optionsFrame->setProperty("panel", true);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsFrame);
    optionsLayout->setSpacing(12);
    optionsLayout->setContentsMargins(16, 16, 16, 16);

    m_btnOpenExisting = new QPushButton(tr("📁  Open Existing Database..."), optionsFrame);
    m_btnOpenExisting->setMinimumHeight(40);
    m_btnOpenExisting->setCursor(Qt::PointingHandCursor);
    connect(m_btnOpenExisting, &QPushButton::clicked, this, &DatabaseSelectionDialog::onOpenExistingClicked);
    optionsLayout->addWidget(m_btnOpenExisting);

    m_btnCreateNew = new QPushButton(tr("✨  Create New Database..."), optionsFrame);
    m_btnCreateNew->setMinimumHeight(40);
    m_btnCreateNew->setCursor(Qt::PointingHandCursor);
    m_btnCreateNew->setProperty("primary", true);
    connect(m_btnCreateNew, &QPushButton::clicked, this, &DatabaseSelectionDialog::onCreateNewClicked);
    optionsLayout->addWidget(m_btnCreateNew);

    mainLayout->addWidget(optionsFrame);

    // Bottom Action / Cancel
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_btnCancel = new QPushButton(tr("Exit"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(bottomLayout);
}

QString DatabaseSelectionDialog::selectedPath() const
{
    return m_selectedPath;
}

void DatabaseSelectionDialog::onOpenExistingClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open MSMS Database"),
        QString(),
        tr("SQLite Databases (*.db *.sqlite);;All Files (*)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    QString err;
    if (DatabaseManager::instance().openDatabase(filePath, &err)) {
        m_selectedPath = filePath;
        accept();
    } else {
        QMessageBox::critical(
            this,
            tr("Incompatible Database"),
            tr("Failed to open the selected database:\n\n%1\n\nPlease select a valid MSMS database or create a new one.")
                .arg(err)
        );
    }
}

void DatabaseSelectionDialog::onCreateNewClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Create New MSMS Database"),
        "msms.db",
        tr("SQLite Databases (*.db *.sqlite);;All Files (*)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo fi(filePath);
    bool overwrite = false;
    if (fi.exists()) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            tr("File Already Exists"),
            tr("The destination file already exists:\n\n%1\n\nCreating a new database will OVERWRITE this file and permanently destroy all existing data.\n\nAre you sure you want to proceed?")
                .arg(filePath),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply != QMessageBox::Yes) {
            return;
        }
        overwrite = true;
    }

    QString err;
    if (DatabaseManager::instance().createDatabase(filePath, overwrite, &err)) {
        m_selectedPath = filePath;
        accept();
    } else {
        QMessageBox::critical(
            this,
            tr("Failed to Create Database"),
            tr("An error occurred while creating the new database:\n\n%1").arg(err)
        );
    }
}

} // namespace MSMS
