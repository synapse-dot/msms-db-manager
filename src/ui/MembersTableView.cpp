#include "MembersTableView.h"
#include "database/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QSqlQueryModel>
#include <QSortFilterProxyModel>
#include <QSqlRecord>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QItemSelectionModel>

namespace MSMS {

MembersTableView::MembersTableView(MembersFilter filter, QWidget *parent)
    : QWidget(parent)
    , m_filter(filter)
{
    setupUi();
    loadData();
}

void MembersTableView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Header Area
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QVBoxLayout *titleLayout = new QVBoxLayout();

    m_titleLabel = new QLabel(this);
    m_titleLabel->setProperty("heading", true);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setProperty("subheading", true);

    if (m_filter == MembersFilter::All) {
        m_titleLabel->setText(tr("All Members"));
        m_subtitleLabel->setText(tr("Read-only table of all active and former society members"));
    } else {
        m_titleLabel->setText(tr("Active Members"));
        m_subtitleLabel->setText(tr("Read-only table of currently active society members"));
    }

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    // Action buttons in header
    m_btnNew = new QPushButton(tr("➕ New Member"), this);
    connect(m_btnNew, &QPushButton::clicked, this, &MembersTableView::newMemberRequested);
    headerLayout->addWidget(m_btnNew);

    m_btnEdit = new QPushButton(tr("✏️ Edit Member"), this);
    m_btnEdit->setEnabled(false);
    connect(m_btnEdit, &QPushButton::clicked, this, &MembersTableView::onEditClicked);
    headerLayout->addWidget(m_btnEdit);

    m_btnRefresh = new QPushButton(tr("🔄 Refresh"), this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &MembersTableView::refresh);
    headerLayout->addWidget(m_btnRefresh);

    mainLayout->addLayout(headerLayout);

    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchIcon = new QLabel(tr("Search:"), this);
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(
        m_filter == MembersFilter::All
            ? tr("Filter all members (Ctrl+F)...")
            : tr("Filter active members (Ctrl+F)...")
    );
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MembersTableView::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);

    mainLayout->addLayout(searchLayout);

    // Table view
    m_tableView = new QTableView(this);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // Strict read-only
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->verticalHeader()->setVisible(false);

    // Specification: predefined sorting only, non-clickable header
    m_tableView->horizontalHeader()->setSectionsClickable(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);

    connect(m_tableView, &QTableView::doubleClicked, this, &MembersTableView::onRowDoubleClicked);

    mainLayout->addWidget(m_tableView, 1);

    // Empty state message label
    m_emptyStateLabel = new QLabel(this);
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setStyleSheet("color: palette(placeholder-text); font-size: 14px; font-style: italic; padding: 24px;");
    m_emptyStateLabel->setText(
        m_filter == MembersFilter::All
            ? tr("No member records found in database.")
            : tr("No active members found in database.")
    );
    m_emptyStateLabel->setVisible(false);
    mainLayout->addWidget(m_emptyStateLabel);

    // Status / summary row
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: palette(placeholder-text); font-size: 12px;");
    mainLayout->addWidget(m_statusLabel);
}

void MembersTableView::loadData()
{
    if (!DatabaseManager::instance().isOpen()) {
        m_statusLabel->setText(tr("Database not connected."));
        return;
    }

    if (!m_sqlModel) {
        m_sqlModel = new QSqlQueryModel(this);
    }

    QString queryStr = (m_filter == MembersFilter::All)
        ? "SELECT member_id, name, class, phone, email, status, join_date "
          "FROM members "
          "ORDER BY name COLLATE NOCASE ASC;"
        : "SELECT member_id, name, class, phone, email, status, join_date "
          "FROM members "
          "WHERE status = 'Active' "
          "ORDER BY name COLLATE NOCASE ASC;";

    m_sqlModel->setQuery(queryStr, DatabaseManager::instance().database());

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
            this, &MembersTableView::onSelectionChanged, Qt::UniqueConnection);

    int totalCount = m_sqlModel->rowCount();
    if (totalCount == 0) {
        m_emptyStateLabel->setVisible(true);
        m_tableView->setVisible(false);
        m_statusLabel->setText(
            m_filter == MembersFilter::All
                ? tr("0 members found in database.")
                : tr("0 active members found in database.")
        );
    } else {
        m_emptyStateLabel->setVisible(false);
        m_tableView->setVisible(true);
        m_statusLabel->setText(
            m_filter == MembersFilter::All
                ? tr("Total members: %1").arg(totalCount)
                : tr("Active members: %1").arg(totalCount)
        );
    }

    m_btnEdit->setEnabled(false);
}

void MembersTableView::refresh()
{
    loadData();
    if (!m_searchEdit->text().isEmpty()) {
        onSearchTextChanged(m_searchEdit->text());
    }
}

void MembersTableView::focusSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void MembersTableView::onSearchTextChanged(const QString &text)
{
    if (m_proxyModel) {
        m_proxyModel->setFilterFixedString(text);
        int visibleCount = m_proxyModel->rowCount();
        int totalCount = m_sqlModel ? m_sqlModel->rowCount() : 0;
        if (text.isEmpty()) {
            m_statusLabel->setText(
                m_filter == MembersFilter::All
                    ? tr("Total members: %1").arg(totalCount)
                    : tr("Active members: %1").arg(totalCount)
            );
        } else {
            m_statusLabel->setText(tr("Showing %1 of %2 members").arg(visibleCount).arg(totalCount));
        }
    }
}

void MembersTableView::onSelectionChanged()
{
    bool hasSelection = m_tableView->selectionModel() && m_tableView->selectionModel()->hasSelection();
    m_btnEdit->setEnabled(hasSelection);
}

void MembersTableView::onEditClicked()
{
    if (!m_tableView->selectionModel() || !m_tableView->selectionModel()->hasSelection()) {
        return;
    }

    QModelIndex proxyIndex = m_tableView->selectionModel()->selectedRows().first();
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    int memberId = m_sqlModel->record(sourceIndex.row()).value("member_id").toInt();

    emit editMemberRequested(memberId);
}

void MembersTableView::onRowDoubleClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    int memberId = m_sqlModel->record(sourceIndex.row()).value("member_id").toInt();

    emit editMemberRequested(memberId);
}

} // namespace MSMS
