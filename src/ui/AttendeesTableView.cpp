#include "AttendeesTableView.h"
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
#include <QSqlQuery>
#include <QSqlError>
#include <QFrame>

namespace MSMS {

AttendeesTableView::AttendeesTableView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refresh();
}

void AttendeesTableView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Top Header: Meeting details & actions
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QVBoxLayout *titleLayout = new QVBoxLayout();

    m_meetingTitleLabel = new QLabel(tr("Meeting Attendees"), this);
    m_meetingTitleLabel->setProperty("heading", true);
    m_meetingTitleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");

    m_meetingDetailsLabel = new QLabel(this);
    m_meetingDetailsLabel->setProperty("subheading", true);
    m_meetingDetailsLabel->setWordWrap(true);

    titleLayout->addWidget(m_meetingTitleLabel);
    titleLayout->addWidget(m_meetingDetailsLabel);
    headerLayout->addLayout(titleLayout, 1);

    m_btnManageRegister = new QPushButton(tr("📋 Attendance Register..."), this);
    m_btnManageRegister->setToolTip(tr("Open attendance register for this meeting"));
    connect(m_btnManageRegister, &QPushButton::clicked, this, [this]() {
        if (m_currentMeetingId > 0) {
            emit openRegisterRequested(m_currentMeetingId);
        }
    });
    headerLayout->addWidget(m_btnManageRegister);

    m_btnRefresh = new QPushButton(tr("🔄 Refresh"), this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &AttendeesTableView::refresh);
    headerLayout->addWidget(m_btnRefresh);

    mainLayout->addLayout(headerLayout);

    // Filter/search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchIcon = new QLabel(tr("Filter Attendees:"), this);
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter attendees for this meeting (Ctrl+F)..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AttendeesTableView::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);

    mainLayout->addLayout(searchLayout);

    // Table view
    m_tableView = new QTableView(this);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // Strict read-only
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->verticalHeader()->setVisible(false);

    m_tableView->horizontalHeader()->setSectionsClickable(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);

    mainLayout->addWidget(m_tableView, 1);

    // Empty state labels
    m_emptyAttendeesLabel = new QLabel(tr("No attendees recorded for this meeting. Click 'Attendance Register...' to record attendance."), this);
    m_emptyAttendeesLabel->setAlignment(Qt::AlignCenter);
    m_emptyAttendeesLabel->setStyleSheet("color: palette(placeholder-text); font-size: 14px; font-style: italic; padding: 24px;");
    m_emptyAttendeesLabel->setVisible(false);
    mainLayout->addWidget(m_emptyAttendeesLabel);

    m_noMeetingsLabel = new QLabel(tr("No meetings recorded in the database. Please create a meeting first."), this);
    m_noMeetingsLabel->setAlignment(Qt::AlignCenter);
    m_noMeetingsLabel->setStyleSheet("color: palette(placeholder-text); font-size: 14px; font-style: italic; padding: 24px;");
    m_noMeetingsLabel->setVisible(false);
    mainLayout->addWidget(m_noMeetingsLabel);

    // Bottom Navigation Bar with < > and Meeting ID entry box as specified in design.md
    QHBoxLayout *bottomNavLayout = new QHBoxLayout();

    m_btnPrev = new QPushButton(tr("◀ Previous Meeting"), this);
    connect(m_btnPrev, &QPushButton::clicked, this, &AttendeesTableView::onPrevMeeting);
    bottomNavLayout->addWidget(m_btnPrev);

    m_btnNext = new QPushButton(tr("Next Meeting ▶"), this);
    connect(m_btnNext, &QPushButton::clicked, this, &AttendeesTableView::onNextMeeting);
    bottomNavLayout->addWidget(m_btnNext);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: palette(placeholder-text); font-size: 12px; margin-left: 8px;");
    bottomNavLayout->addWidget(m_statusLabel, 1);

    // Meeting ID entry box at bottom right
    QLabel *jumpLabel = new QLabel(tr("Go to Meeting ID:"), this);
    bottomNavLayout->addWidget(jumpLabel);

    m_meetingIdInput = new QLineEdit(this);
    m_meetingIdInput->setFixedWidth(70);
    m_meetingIdInput->setPlaceholderText(tr("ID"));
    connect(m_meetingIdInput, &QLineEdit::returnPressed, this, &AttendeesTableView::onMeetingIdEntered);
    bottomNavLayout->addWidget(m_meetingIdInput);

    QPushButton *btnGo = new QPushButton(tr("Go"), this);
    connect(btnGo, &QPushButton::clicked, this, &AttendeesTableView::onMeetingIdEntered);
    bottomNavLayout->addWidget(btnGo);

    mainLayout->addLayout(bottomNavLayout);
}

void AttendeesTableView::loadMeetingList()
{
    m_meetingIds.clear();

    if (!DatabaseManager::instance().isOpen()) return;

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    // Order meetings chronologically/predictably
    q.exec("SELECT meeting_id FROM meetings ORDER BY meeting_date DESC, start_time DESC, meeting_id DESC;");

    while (q.next()) {
        m_meetingIds.append(q.value(0).toInt());
    }

    if (m_meetingIds.isEmpty()) {
        m_currentIndex = -1;
        m_currentMeetingId = -1;
    } else {
        // If current meeting ID is in the list, keep it; otherwise default to first
        int idx = m_meetingIds.indexOf(m_currentMeetingId);
        if (idx >= 0) {
            m_currentIndex = idx;
        } else {
            m_currentIndex = 0;
            m_currentMeetingId = m_meetingIds.first();
        }
    }
}

void AttendeesTableView::loadCurrentMeeting()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_meetingIds.size()) {
        m_currentMeetingId = -1;
        m_meetingTitleLabel->setText(tr("Meeting Attendees"));
        m_meetingDetailsLabel->setText(QString());
        m_noMeetingsLabel->setVisible(true);
        m_emptyAttendeesLabel->setVisible(false);
        m_tableView->setVisible(false);
        m_btnPrev->setEnabled(false);
        m_btnNext->setEnabled(false);
        m_btnManageRegister->setEnabled(false);
        m_statusLabel->setText(tr("No meetings available."));
        return;
    }

    m_currentMeetingId = m_meetingIds[m_currentIndex];
    m_noMeetingsLabel->setVisible(false);
    m_btnManageRegister->setEnabled(true);

    // Update navigation button states
    m_btnPrev->setEnabled(m_currentIndex > 0);
    m_btnNext->setEnabled(m_currentIndex < m_meetingIds.size() - 1);

    // Query meeting metadata
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery qm(db);
    qm.prepare("SELECT title, type, meeting_date, start_time, end_time, location FROM meetings WHERE meeting_id = :id;");
    qm.bindValue(":id", m_currentMeetingId);

    QString title = tr("Meeting");
    QString details;
    if (qm.exec() && qm.next()) {
        title = qm.value("title").toString();
        QString type = qm.value("type").toString();
        QString date = qm.value("meeting_date").toString();
        QString st = qm.value("start_time").toString();
        QString et = qm.value("end_time").toString();
        QString loc = qm.value("location").toString();

        QString timeStr;
        if (!st.isEmpty() && !et.isEmpty()) {
            timeStr = QString("%1 – %2").arg(st, et);
        } else if (!st.isEmpty()) {
            timeStr = st;
        }

        QStringList parts;
        parts.append(QString("Type: %1").arg(type));
        if (!date.isEmpty()) parts.append(QString("Date: %1").arg(date));
        if (!timeStr.isEmpty()) parts.append(QString("Time: %1").arg(timeStr));
        if (!loc.isEmpty()) parts.append(QString("Location: %1").arg(loc));
        parts.append(QString("Meeting ID: %1").arg(m_currentMeetingId));

        details = parts.join(QString("  •  "));
    }

    m_meetingTitleLabel->setText(QString("Attendees: %1").arg(title));
    m_meetingDetailsLabel->setText(details);
    m_meetingIdInput->setText(QString::number(m_currentMeetingId));

    // Query attendees (including Active and Former members)
    if (!m_sqlModel) {
        m_sqlModel = new QSqlQueryModel(this);
    }

    QSqlQuery qa(db);
    qa.prepare(
        "SELECT m.member_id, m.name, m.phone, m.email "
        "FROM meeting_attendance a "
        "JOIN members m ON a.member_id = m.member_id "
        "WHERE a.meeting_id = :meeting_id "
        "ORDER BY m.name COLLATE NOCASE ASC;"
    );
    qa.bindValue(":meeting_id", m_currentMeetingId);
    qa.exec();

    m_sqlModel->setQuery(std::move(qa));

    m_sqlModel->setHeaderData(0, Qt::Horizontal, tr("Member ID"));
    m_sqlModel->setHeaderData(1, Qt::Horizontal, tr("Name"));
    m_sqlModel->setHeaderData(2, Qt::Horizontal, tr("Phone"));
    m_sqlModel->setHeaderData(3, Qt::Horizontal, tr("Email"));

    if (!m_proxyModel) {
        m_proxyModel = new QSortFilterProxyModel(this);
        m_proxyModel->setFilterKeyColumn(-1);
        m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    }
    m_proxyModel->setSourceModel(m_sqlModel);
    m_tableView->setModel(m_proxyModel);

    // In Attendees view, Member ID is explicitly required and visible!
    m_tableView->showColumn(0);

    m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    int attendeeCount = m_sqlModel->rowCount();
    if (attendeeCount == 0) {
        m_emptyAttendeesLabel->setVisible(true);
        m_tableView->setVisible(false);
    } else {
        m_emptyAttendeesLabel->setVisible(false);
        m_tableView->setVisible(true);
    }

    m_statusLabel->setText(tr("Meeting %1 of %2  (Total attendees: %3)")
                               .arg(m_currentIndex + 1)
                               .arg(m_meetingIds.size())
                               .arg(attendeeCount));
}

void AttendeesTableView::refresh()
{
    loadMeetingList();
    loadCurrentMeeting();
    if (!m_searchEdit->text().isEmpty()) {
        onSearchTextChanged(m_searchEdit->text());
    }
}

void AttendeesTableView::selectMeeting(int meetingId)
{
    loadMeetingList();
    int idx = m_meetingIds.indexOf(meetingId);
    if (idx >= 0) {
        m_currentIndex = idx;
        m_currentMeetingId = meetingId;
        loadCurrentMeeting();
    }
}

void AttendeesTableView::focusSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void AttendeesTableView::onSearchTextChanged(const QString &text)
{
    if (m_proxyModel) {
        m_proxyModel->setFilterFixedString(text);
        int visibleCount = m_proxyModel->rowCount();
        int totalCount = m_sqlModel ? m_sqlModel->rowCount() : 0;
        if (text.isEmpty()) {
            m_statusLabel->setText(tr("Meeting %1 of %2  (Total attendees: %3)")
                                       .arg(m_currentIndex + 1)
                                       .arg(m_meetingIds.size())
                                       .arg(totalCount));
        } else {
            m_statusLabel->setText(tr("Showing %1 of %2 attendees").arg(visibleCount).arg(totalCount));
        }
    }
}

void AttendeesTableView::onPrevMeeting()
{
    if (m_currentIndex > 0) {
        m_currentIndex--;
        loadCurrentMeeting();
    }
}

void AttendeesTableView::onNextMeeting()
{
    if (m_currentIndex < m_meetingIds.size() - 1) {
        m_currentIndex++;
        loadCurrentMeeting();
    }
}

void AttendeesTableView::onMeetingIdEntered()
{
    bool ok = false;
    int id = m_meetingIdInput->text().trimmed().toInt(&ok);
    if (ok) {
        selectMeeting(id);
    }
}

} // namespace MSMS
