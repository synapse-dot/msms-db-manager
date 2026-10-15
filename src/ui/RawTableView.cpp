#include "RawTableView.h"
#include "database/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QSqlQueryModel>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>

namespace MSMS {

RawTableView::RawTableView(const QString &tableName, QWidget *parent)
    : QWidget(parent)
    , m_tableName(tableName)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QVBoxLayout *titleLayout = new QVBoxLayout();

    m_titleLabel = new QLabel(QString("Raw Table: %1").arg(tableName), this);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #89b4fa;");
    titleLayout->addWidget(m_titleLabel);

    QLabel *descLabel = new QLabel(tr("Raw database inspector view (read-only)"), this);
    descLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
    titleLayout->addWidget(descLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    QPushButton *btnRefresh = new QPushButton(tr("🔄  Refresh"), this);
    connect(btnRefresh, &QPushButton::clicked, this, &RawTableView::refresh);
    headerLayout->addWidget(btnRefresh);

    mainLayout->addLayout(headerLayout);

    m_tableView = new QTableView(this);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->horizontalHeader()->setSectionsClickable(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    mainLayout->addWidget(m_tableView, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
    mainLayout->addWidget(m_statusLabel);

    loadTable(tableName);
}

void RawTableView::loadTable(const QString &tableName)
{
    m_tableName = tableName;
    m_titleLabel->setText(QString("Raw Table: %1").arg(tableName));

    if (!DatabaseManager::instance().isOpen()) {
        m_statusLabel->setText(tr("Database not connected."));
        return;
    }

    if (!m_model) {
        m_model = new QSqlQueryModel(this);
    }

    m_model->setQuery(QString("SELECT * FROM \"%1\";").arg(tableName), DatabaseManager::instance().database());
    m_tableView->setModel(m_model);
    m_tableView->resizeColumnsToContents();

    int count = m_model->rowCount();
    m_statusLabel->setText(tr("%1 record(s) in table '%2'").arg(count).arg(tableName));
}

void RawTableView::refresh()
{
    loadTable(m_tableName);
}

} // namespace MSMS
