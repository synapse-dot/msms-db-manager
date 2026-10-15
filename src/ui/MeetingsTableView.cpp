#include "MeetingsTableView.h"
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

MeetingsTableView::MeetingsTableView(const QString &meetingType, QWidget *parent)
    : QWidget(parent)
    , m_meetingType(meetingType)
{
    setupUi();
    loadData();
}

void MeetingsTableView::setupUi()
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

    QString typeFull;
    if (m_meetingType == "AGM") {
        m_titleLabel->setText(tr("Annual General Meetings"));
        m_subtitleLabel->setText(tr("Read-only table of Annual General Meetings (AGM)"));
    } else if (m_meetingType == "EGM") {
        m_titleLabel->setText(tr("Extraordinary General Meetings"));
        m_subtitleLabel->setText(tr("Read-only table of Extraordinary General Meetings (EGM)"));
    } else if (m_meetingType == "ECM") {
        m_titleLabel->setText(tr("Executive Committee Meetings"));
        m_subtitleLabel->setText(tr("Read-only table of Executive Committee Meetings (ECM)"));
    } else {
        m_titleLabel->setText(tr("Meetings"));
        m_subtitleLabel->setText(tr("Read-only table of meetings"));
    }

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    // Action buttons in header
    m_btnNew = new QPushButton(tr("➕ New Meeting"), this);
    connect(m_btnNew, &QPushButton::clicked, this, [this]() {
        emit newMeetingRequested(m_meetingType);
    });
    headerLayout->addWidget(m_btnNew);

    m_btnEdit = new QPushButton(tr("✏️ Edit Meeting"), this);
    m_btnEdit->setEnabled(false);
    connect(m_btnEdit, &QPushButton::clicked, this, &MeetingsTableView::onEditClicked);
    headerLayout->addWidget(m_btnEdit);

    m_btnRefresh = new QPushButton(tr("🔄 Refresh"), this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &MeetingsTableView::refresh);
    headerLayout->addWidget(m_btnRefresh);

    mainLayout->addLayout(headerLayout);

    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchIcon = new QLabel(tr("Search:"), this);
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter %1 records (Ctrl+F)...").arg(m_meetingType));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MeetingsTableView::onSearchTextChanged);
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

    connect(m_tableView, &QTableView::doubleClicked, this, &MeetingsTableView::onRowDoubleClicked);

    mainLayout->addWidget(m_tableView, 1);

    // Empty state message label
    m_emptyStateLabel = new QLabel(this);
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setStyleSheet("color: palette(placeholder-text); font-size: 14px; font-style: italic; padding: 24px;");
    m_emptyStateLabel->setText(tr("No %1 meetings found in database.").arg(m_meetingType));
    m_emptyStateLabel->setVisible(false);
    mainLayout->addWidget(m_emptyStateLabel);

    // Status / summary row
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: palette(placeholder-text); font-size: 12px;");
    mainLayout->addWidget(m_statusLabel);
}

void MeetingsTableView::loadData()
{
    if (!DatabaseManager::instance().isOpen()) {
        m_statusLabel->setText(tr("Database not connected."));
        return;
    }

    if (!m_sqlModel) {
        m_sqlModel = new QSqlQueryModel(this);
    }

    // Deterministic predefined sorting
    QString queryStr = QString(
        "SELECT meeting_id, title, type, meeting_date, start_time, end_time, location "
        "FROM meetings "
        "WHERE type = '%1' "
        "ORDER BY meeting_date DESC, start_time DESC, title COLLATE NOCASE ASC;"
    ).arg(m_meetingType);

    m_sqlModel->setQuery(queryStr, DatabaseManager::instance().database());

    // Configure column headers
    m_sqlModel->setHeaderData(1, Qt::Horizontal, tr("Title"));
    m_sqlModel->setHeaderData(2, Qt::Horizontal, tr("Type"));
    m_sqlModel->setHeaderData(3, Qt::Horizontal, tr("Date"));
    m_sqlModel->setHeaderData(4, Qt::Horizontal, tr("Start Time"));
    m_sqlModel->setHeaderData(5, Qt::Horizontal, tr("End Time"));
    m_sqlModel->setHeaderData(6, Qt::Horizontal, tr("Location"));

    if (!m_proxyModel) {
        m_proxyModel = new QSortFilterProxyModel(this);
        m_proxyModel->setFilterKeyColumn(-1); // Search all columns
        m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    }
    m_proxyModel->setSourceModel(m_sqlModel);

    m_tableView->setModel(m_proxyModel);

    // Hide internal ID (meeting_id is column 0)
    m_tableView->hideColumn(0);

    // Adjust column sizes
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); // Title stretches
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    // Selection monitoring for Edit button
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MeetingsTableView::onSelectionChanged, Qt::UniqueConnection);

    int totalCount = m_sqlModel->rowCount();
    if (totalCount == 0) {
        m_emptyStateLabel->setVisible(true);
        m_tableView->setVisible(false);
        m_statusLabel->setText(tr("0 %1 meetings found in database.").arg(m_meetingType));
    } else {
        m_emptyStateLabel->setVisible(false);
        m_tableView->setVisible(true);
        m_statusLabel->setText(tr("Total %1 meetings: %2").arg(m_meetingType).arg(totalCount));
    }

    m_btnEdit->setEnabled(false);
}

void MeetingsTableView::refresh()
{
    loadData();
    if (!m_searchEdit->text().isEmpty()) {
        onSearchTextChanged(m_searchEdit->text());
    }
}

void MeetingsTableView::focusSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void MeetingsTableView::onSearchTextChanged(const QString &text)
{
    if (m_proxyModel) {
        m_proxyModel->setFilterFixedString(text);
        int visibleCount = m_proxyModel->rowCount();
        int totalCount = m_sqlModel ? m_sqlModel->rowCount() : 0;
        if (text.isEmpty()) {
            m_statusLabel->setText(tr("Total %1 meetings: %2").arg(m_meetingType).arg(totalCount));
        } else {
            m_statusLabel->setText(tr("Showing %1 of %2 %3 meetings").arg(visibleCount).arg(totalCount).arg(m_meetingType));
        }
    }
}

void MeetingsTableView::onSelectionChanged()
{
    bool hasSelection = m_tableView->selectionModel() && m_tableView->selectionModel()->hasSelection();
    m_btnEdit->setEnabled(hasSelection);
}

void MeetingsTableView::onEditClicked()
{
    if (!m_tableView->selectionModel() || !m_tableView->selectionModel()->hasSelection()) {
        return;
    }

    QModelIndex proxyIndex = m_tableView->selectionModel()->selectedRows().first();
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    int meetingId = m_sqlModel->record(sourceIndex.row()).value("meeting_id").toInt();

    emit editMeetingRequested(meetingId);
}

void MeetingsTableView::onRowDoubleClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    int meetingId = m_sqlModel->record(sourceIndex.row()).value("meeting_id").toInt();

    emit editMeetingRequested(meetingId);
}

} // namespace MSMS
