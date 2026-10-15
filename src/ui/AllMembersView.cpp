#include "AllMembersView.h"
#include "database/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QSqlQueryModel>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QItemSelectionModel>

namespace MSMS {

AllMembersView::AllMembersView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    loadData();
}

void AllMembersView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Header Area
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QVBoxLayout *titleLayout = new QVBoxLayout();

    QLabel *titleLabel = new QLabel(tr("All Members"), this);
    titleLabel->setProperty("heading", true);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #89b4fa;");
    titleLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(tr("Read-only table of all active and former society members"), this);
    subtitleLabel->setProperty("subheading", true);
    titleLayout->addWidget(subtitleLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    // Action buttons in header
    m_btnEdit = new QPushButton(tr("✏️  Edit Member"), this);
    m_btnEdit->setEnabled(false);
    connect(m_btnEdit, &QPushButton::clicked, this, &AllMembersView::onEditClicked);
    headerLayout->addWidget(m_btnEdit);

    m_btnRefresh = new QPushButton(tr("🔄  Refresh"), this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &AllMembersView::refresh);
    headerLayout->addWidget(m_btnRefresh);

    mainLayout->addLayout(headerLayout);

    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchIcon = new QLabel(tr("🔍 Filter:"), this);
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Type to search within All Members (Ctrl+F)..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AllMembersView::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);

    mainLayout->addLayout(searchLayout);

    // Table view
    m_tableView = new QTableView(this);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // Strict read-only
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->verticalHeader()->setVisible(false);

    // Specification: Column headers do not act as clickable sort controls (5.2)
    m_tableView->horizontalHeader()->setSectionsClickable(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);

    mainLayout->addWidget(m_tableView, 1);

    // Empty state message label (centered if no records)
    m_emptyStateLabel = new QLabel(tr("No member records found in database."), this);
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setStyleSheet("color: #a6adc8; font-size: 14px; font-style: italic; padding: 20px;");
    m_emptyStateLabel->setVisible(false);
    mainLayout->addWidget(m_emptyStateLabel);

    // Status / summary row
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
    mainLayout->addWidget(m_statusLabel);
}

void AllMembersView::loadData()
{
    if (!DatabaseManager::instance().isOpen()) {
        m_statusLabel->setText(tr("Database not connected."));
        return;
    }

    if (!m_sqlModel) {
        m_sqlModel = new QSqlQueryModel(this);
    }

    // Query members table
    m_sqlModel->setQuery(
        "SELECT member_id, name, class, phone, email, status, join_date "
        "FROM members "
        "ORDER BY name COLLATE NOCASE ASC;",
        DatabaseManager::instance().database()
    );

    // Configure column headers
    m_sqlModel->setHeaderData(1, Qt::Horizontal, tr("Name"));
    m_sqlModel->setHeaderData(2, Qt::Horizontal, tr("Class"));
    m_sqlModel->setHeaderData(3, Qt::Horizontal, tr("Phone"));
    m_sqlModel->setHeaderData(4, Qt::Horizontal, tr("Email"));
    m_sqlModel->setHeaderData(5, Qt::Horizontal, tr("Status"));
    m_sqlModel->setHeaderData(6, Qt::Horizontal, tr("Join Date"));

    if (!m_proxyModel) {
        m_proxyModel = new QSortFilterProxyModel(this);
        m_proxyModel->setFilterKeyColumn(-1); // Search all columns
        m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    }
    m_proxyModel->setSourceModel(m_sqlModel);

    m_tableView->setModel(m_proxyModel);

    // Hide internal ID (member_id is column 0)
    m_tableView->hideColumn(0);

    // Adjust column sizes
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); // Name stretches
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    // Selection monitoring for Edit button
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AllMembersView::onSelectionChanged, Qt::UniqueConnection);

    int totalCount = m_sqlModel->rowCount();
    if (totalCount == 0) {
        m_emptyStateLabel->setVisible(true);
        m_tableView->setVisible(false);
        m_statusLabel->setText(tr("0 members found in database."));
    } else {
        m_emptyStateLabel->setVisible(false);
        m_tableView->setVisible(true);
        m_statusLabel->setText(tr("Total members: %1").arg(totalCount));
    }

    m_btnEdit->setEnabled(false);
}

void AllMembersView::refresh()
{
    loadData();
    if (!m_searchEdit->text().isEmpty()) {
        onSearchTextChanged(m_searchEdit->text());
    }
}

void AllMembersView::focusSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void AllMembersView::onSearchTextChanged(const QString &text)
{
    if (m_proxyModel) {
        m_proxyModel->setFilterFixedString(text);
        int visibleCount = m_proxyModel->rowCount();
        int totalCount = m_sqlModel ? m_sqlModel->rowCount() : 0;
        if (text.isEmpty()) {
            m_statusLabel->setText(tr("Total members: %1").arg(totalCount));
        } else {
            m_statusLabel->setText(tr("Showing %1 of %2 members").arg(visibleCount).arg(totalCount));
        }
    }
}

void AllMembersView::onSelectionChanged()
{
    bool hasSelection = m_tableView->selectionModel() && m_tableView->selectionModel()->hasSelection();
    m_btnEdit->setEnabled(hasSelection);
}

void AllMembersView::onEditClicked()
{
    if (!m_tableView->selectionModel() || !m_tableView->selectionModel()->hasSelection()) {
        return;
    }

    QModelIndex currentIndex = m_tableView->selectionModel()->selectedRows().first();
    // Retrieve member name from column 1
    QString memberName = m_proxyModel->data(m_proxyModel->index(currentIndex.row(), 1)).toString();

    QMessageBox::information(
        this,
        tr("New / Edit Member Form"),
        tr("Selected member: \"%1\"\n\nEditing member records via the 'New / Edit Member' form is scheduled for Milestone 2.").arg(memberName)
    );
}

} // namespace MSMS
