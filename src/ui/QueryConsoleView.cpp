#include "QueryConsoleView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QSqlQueryModel>
#include <QLabel>
#include <QHeaderView>

namespace MSMS {

// -------------------------------------------------------------
// QueryResultView
// -------------------------------------------------------------
QueryResultView::QueryResultView(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_queryHeaderLabel = new QLabel(tr("Query Result"), this);
    m_queryHeaderLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #89b4fa;");
    headerLayout->addWidget(m_queryHeaderLabel);

    headerLayout->addSpacing(8);

    m_statusBadge = new QLabel(tr("Ready"), this);
    m_statusBadge->setStyleSheet("background-color: #313244; color: #a6adc8; padding: 2px 8px; border-radius: 4px; font-size: 11px;");
    headerLayout->addWidget(m_statusBadge);
    headerLayout->addStretch();

    mainLayout->addLayout(headerLayout);

    m_statusMessageLabel = new QLabel(tr("Execute an SQL query using the query bar at the bottom."), this);
    m_statusMessageLabel->setStyleSheet("color: #bac2de; font-size: 13px;");
    m_statusMessageLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusMessageLabel);

    m_tableView = new QTableView(this);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionsClickable(false);
    mainLayout->addWidget(m_tableView, 1);
}

void QueryResultView::displaySelectResult(QSqlQueryModel *model, const QString &query, int rowCount)
{
    m_queryHeaderLabel->setText(tr("Query Result"));
    m_statusBadge->setText(tr("SELECT Success"));
    m_statusBadge->setStyleSheet("background-color: #313244; color: #a6e3a1; padding: 2px 8px; border-radius: 4px; font-weight: bold; font-size: 11px;");

    m_statusMessageLabel->setText(tr("SQL: %1\nReturned %2 row(s).").arg(query).arg(rowCount));
    m_statusMessageLabel->setStyleSheet("color: #a6e3a1; font-size: 12px;");

    m_tableView->setModel(model);
    m_tableView->setVisible(true);
    m_tableView->resizeColumnsToContents();
}

void QueryResultView::displayModificationResult(const QString &query, int rowsAffected)
{
    m_queryHeaderLabel->setText(tr("Query Executed"));
    m_statusBadge->setText(tr("Commit Success"));
    m_statusBadge->setStyleSheet("background-color: #313244; color: #a6e3a1; padding: 2px 8px; border-radius: 4px; font-weight: bold; font-size: 11px;");

    m_statusMessageLabel->setText(tr("SQL: %1\nTransaction committed. %2 row(s) affected.").arg(query).arg(rowsAffected));
    m_statusMessageLabel->setStyleSheet("color: #a6e3a1; font-size: 13px; font-weight: 500;");

    m_tableView->setVisible(false);
}

void QueryResultView::displayError(const QString &query, const QString &errorMessage)
{
    m_queryHeaderLabel->setText(tr("Query Error"));
    m_statusBadge->setText(tr("Failed"));
    m_statusBadge->setStyleSheet("background-color: #313244; color: #f38ba8; padding: 2px 8px; border-radius: 4px; font-weight: bold; font-size: 11px;");

    m_statusMessageLabel->setText(tr("SQL: %1\n\nSQLite Error:\n%2").arg(query, errorMessage));
    m_statusMessageLabel->setStyleSheet("color: #f38ba8; font-size: 13px; font-family: monospace; background-color: #181825; padding: 12px; border: 1px solid #f38ba8; border-radius: 4px;");

    m_tableView->setVisible(false);
}

// -------------------------------------------------------------
// QueryEntryBar
// -------------------------------------------------------------
QueryEntryBar::QueryEntryBar(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    QLabel *promptLabel = new QLabel(tr("SQL >"), this);
    promptLabel->setStyleSheet("font-weight: bold; color: #89b4fa; font-family: monospace;");
    layout->addWidget(promptLabel);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText(tr("Enter single SQL statement (e.g. SELECT * FROM members)..."));
    m_lineEdit->setStyleSheet("font-family: monospace;");
    connect(m_lineEdit, &QLineEdit::returnPressed, this, &QueryEntryBar::onExecuteClicked);
    layout->addWidget(m_lineEdit, 1);

    m_btnExecute = new QPushButton(tr("▶ Run"), this);
    m_btnExecute->setProperty("primary", true);
    m_btnExecute->setToolTip(tr("Execute SQL query"));
    connect(m_btnExecute, &QPushButton::clicked, this, &QueryEntryBar::onExecuteClicked);
    layout->addWidget(m_btnExecute);

    m_btnDialog = new QPushButton(tr("⛶ Console"), this);
    m_btnDialog->setToolTip(tr("Open or focus SQL query entry"));
    connect(m_btnDialog, &QPushButton::clicked, this, &QueryEntryBar::openDialogRequested);
    layout->addWidget(m_btnDialog);

    setStyleSheet("background-color: #181825; border-top: 1px solid #313244;");
}

void QueryEntryBar::focusInput()
{
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

QString QueryEntryBar::currentQuery() const
{
    return m_lineEdit->text().trimmed();
}

void QueryEntryBar::setQuery(const QString &query)
{
    m_lineEdit->setText(query);
}

void QueryEntryBar::onExecuteClicked()
{
    QString q = currentQuery();
    if (!q.isEmpty()) {
        emit executeQueryRequested(q);
    }
}

} // namespace MSMS
