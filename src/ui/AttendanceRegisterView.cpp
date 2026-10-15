#include "AttendanceRegisterView.h"
#include "database/DatabaseManager.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QScrollArea>
#include <QFrame>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>

namespace MSMS {

AttendanceRegisterView::AttendanceRegisterView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    loadMeetings();
}

void AttendanceRegisterView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(tr("Attendance Register"), this);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Form panel
    QFrame *formPanel = new QFrame(this);
    QVBoxLayout *panelLayout = new QVBoxLayout(formPanel);
    panelLayout->setContentsMargins(20, 20, 20, 20);
    panelLayout->setSpacing(16);

    // Meeting selector
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setSpacing(10);

    m_meetingCombo = new QComboBox(formPanel);
    connect(m_meetingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AttendanceRegisterView::onMeetingChanged);
    formLayout->addRow(tr("Meeting:"), m_meetingCombo);
    panelLayout->addLayout(formLayout);

    m_meetingDetailsLabel = new QLabel(formPanel);
    m_meetingDetailsLabel->setStyleSheet("color: palette(mid); font-size: 12px;");
    m_meetingDetailsLabel->setWordWrap(true);
    panelLayout->addWidget(m_meetingDetailsLabel);

    // Member list
    QLabel *listLabel = new QLabel(tr("Members (check to mark as attended):"), formPanel);
    listLabel->setStyleSheet("font-weight: bold;");
    panelLayout->addWidget(listLabel);

    m_memberList = new QListWidget(formPanel);
    m_memberList->setSelectionMode(QAbstractItemView::NoSelection);
    m_memberList->setMinimumHeight(240);
    connect(m_memberList, &QListWidget::itemChanged,
            this, &AttendanceRegisterView::onItemChanged);
    panelLayout->addWidget(m_memberList, 1);

    m_emptyLabel = new QLabel(tr("No meetings available. Create a meeting first."), formPanel);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: palette(placeholder-text); font-style: italic; padding: 20px;");
    m_emptyLabel->setVisible(false);
    panelLayout->addWidget(m_emptyLabel);

    // Status message
    m_statusMessageLabel = new QLabel(formPanel);
    m_statusMessageLabel->setWordWrap(true);
    m_statusMessageLabel->setVisible(false);
    panelLayout->addWidget(m_statusMessageLabel);

    // Actions
    QHBoxLayout *actionsLayout = new QHBoxLayout();
    m_btnSave = new QPushButton(tr("💾 Save"), formPanel);
    m_btnSave->setProperty("primary", true);
    connect(m_btnSave, &QPushButton::clicked, this, &AttendanceRegisterView::onSaveClicked);
    actionsLayout->addWidget(m_btnSave);

    m_btnCancel = new QPushButton(tr("Cancel"), formPanel);
    connect(m_btnCancel, &QPushButton::clicked, this, &AttendanceRegisterView::onCancelClicked);
    actionsLayout->addWidget(m_btnCancel);
    actionsLayout->addStretch();
    panelLayout->addLayout(actionsLayout);

    mainLayout->addWidget(formPanel);
    mainLayout->addStretch();
}

void AttendanceRegisterView::loadMeetings()
{
    m_loading = true;
    m_meetingCombo->clear();

    if (!DatabaseManager::instance().isOpen()) {
        m_emptyLabel->setVisible(true);
        m_memberList->setVisible(false);
        m_btnSave->setEnabled(false);
        m_loading = false;
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.exec("SELECT meeting_id, title, type, meeting_date FROM meetings "
           "ORDER BY meeting_date DESC, meeting_id DESC;");

    while (q.next()) {
        int id = q.value("meeting_id").toInt();
        QString title = q.value("title").toString();
        QString type = q.value("type").toString();
        QString date = q.value("meeting_date").toString();

        QString label;
        if (!date.isEmpty()) {
            label = QString("[%1] %2 (%3)").arg(type, title, date);
        } else {
            label = QString("[%1] %2").arg(type, title);
        }
        m_meetingCombo->addItem(label, id);
    }

    bool hasMeetings = m_meetingCombo->count() > 0;
    m_emptyLabel->setVisible(!hasMeetings);
    m_memberList->setVisible(hasMeetings);
    m_btnSave->setEnabled(hasMeetings);

    m_loading = false;

    if (hasMeetings) {
        // Trigger load of first meeting
        onMeetingChanged(0);
    } else {
        m_meetingDetailsLabel->clear();
        setDirty(false);
    }
}

int AttendanceRegisterView::currentMeetingId() const
{
    if (m_meetingCombo->count() == 0) return -1;
    return m_meetingCombo->currentData().toInt();
}

void AttendanceRegisterView::onMeetingChanged(int /*comboIndex*/)
{
    if (m_loading) return;

    int meetingId = currentMeetingId();
    if (meetingId <= 0) return;

    // Show meeting details
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery qm(db);
    qm.prepare("SELECT title, type, meeting_date, start_time, end_time, location "
               "FROM meetings WHERE meeting_id = :id;");
    qm.bindValue(":id", meetingId);
    if (qm.exec() && qm.next()) {
        QString type = qm.value("type").toString();
        QString date = qm.value("meeting_date").toString();
        QString st   = qm.value("start_time").toString();
        QString et   = qm.value("end_time").toString();
        QString loc  = qm.value("location").toString();
        QStringList parts;
        parts << QString("Type: %1").arg(type);
        if (!date.isEmpty()) parts << QString("Date: %1").arg(date);
        if (!st.isEmpty() && !et.isEmpty()) parts << QString("Time: %1 – %2").arg(st, et);
        else if (!st.isEmpty()) parts << QString("Time: %1").arg(st);
        if (!loc.isEmpty()) parts << QString("Location: %1").arg(loc);
        m_meetingDetailsLabel->setText(parts.join("  •  "));
    }

    loadRegister();
}

void AttendanceRegisterView::loadRegister()
{
    m_loading = true;

    int meetingId = currentMeetingId();
    m_savedAttendeeIds.clear();
    m_formerMemberIds.clear();

    QSqlDatabase db = DatabaseManager::instance().database();

    // Get existing attendees (including Former members)
    QSqlQuery qa(db);
    qa.prepare(
        "SELECT m.member_id, m.name, m.status "
        "FROM meeting_attendance a "
        "JOIN members m ON a.member_id = m.member_id "
        "WHERE a.meeting_id = :mid "
        "ORDER BY m.name COLLATE NOCASE ASC;"
    );
    qa.bindValue(":mid", meetingId);
    qa.exec();

    while (qa.next()) {
        int mid = qa.value("member_id").toInt();
        m_savedAttendeeIds.insert(mid);
        if (qa.value("status").toString() == "Former") {
            m_formerMemberIds.insert(mid);
        }
    }

    // Build member list: active members + former members in saved set
    // First collect active members
    QSqlQuery qact(db);
    qact.exec("SELECT member_id, name FROM members WHERE status = 'Active' ORDER BY name COLLATE NOCASE ASC;");

    QList<QPair<int,QString>> activeMembers;
    while (qact.next()) {
        activeMembers.append({qact.value(0).toInt(), qact.value(1).toString()});
    }

    // Collect former members that already have a saved attendance record
    QSqlQuery qfmr(db);
    qfmr.exec("SELECT member_id, name FROM members WHERE status = 'Former' ORDER BY name COLLATE NOCASE ASC;");
    QList<QPair<int,QString>> formerWithAttendance;
    while (qfmr.next()) {
        int fid = qfmr.value(0).toInt();
        if (m_savedAttendeeIds.contains(fid)) {
            formerWithAttendance.append({fid, qfmr.value(1).toString()});
        }
    }

    // Populate list widget
    disconnect(m_memberList, &QListWidget::itemChanged, this, &AttendanceRegisterView::onItemChanged);
    m_memberList->clear();

    auto addItem = [&](int memberId, const QString &name, bool checked, bool isFormer) {
        QString label = isFormer ? QString("%1 (Former member — existing attendance)").arg(name) : name;
        QListWidgetItem *item = new QListWidgetItem(label, m_memberList);
        item->setData(Qt::UserRole, memberId);
        item->setData(Qt::UserRole + 1, isFormer);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        if (isFormer) {
            // Visually distinguish former members; disable unchecking to prevent accidental removal
            item->setForeground(QApplication::palette().color(QPalette::Mid));
            // Former members cannot be unchecked from the list — they must remain
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        }
    };

    // Add active members
    for (auto &[id, name] : activeMembers) {
        addItem(id, name, m_savedAttendeeIds.contains(id), false);
    }

    // Add former members (already checked, read-only)
    if (!formerWithAttendance.isEmpty()) {
        // Separator item
        QListWidgetItem *sep = new QListWidgetItem(tr("── Former Members (historical attendance preserved) ──"), m_memberList);
        sep->setFlags(Qt::NoItemFlags);
        sep->setForeground(QApplication::palette().color(QPalette::Mid));
        QFont sepFont = sep->font();
        sepFont.setItalic(true);
        sep->setFont(sepFont);
        for (auto &[id, name] : formerWithAttendance) {
            addItem(id, name, true, true);
        }
    }

    connect(m_memberList, &QListWidget::itemChanged, this, &AttendanceRegisterView::onItemChanged);

    m_loading = false;
    setDirty(false);

    m_statusMessageLabel->setVisible(false);
}

void AttendanceRegisterView::onItemChanged(QListWidgetItem * /*item*/)
{
    if (m_loading) return;
    setDirty(true);
}

void AttendanceRegisterView::setDirty(bool dirty)
{
    m_isDirty = dirty;
    if (dirty) {
        m_titleLabel->setText(tr("Attendance Register *"));
    } else {
        m_titleLabel->setText(tr("Attendance Register"));
    }
}

bool AttendanceRegisterView::maybeConfirmDiscard()
{
    if (!m_isDirty) return true;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("You have unsaved changes in the Attendance Register.\n\nDiscard changes?"),
        QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Cancel
    );

    if (reply == QMessageBox::Discard) {
        setDirty(false);
        return true;
    }
    return false;
}

void AttendanceRegisterView::onSaveClicked()
{
    int meetingId = currentMeetingId();
    if (meetingId <= 0) return;

    // Collect checked member IDs from the list (active members only from checkboxes)
    QSet<int> newAttendeeIds;
    for (int i = 0; i < m_memberList->count(); ++i) {
        QListWidgetItem *item = m_memberList->item(i);
        if (!(item->flags() & Qt::ItemIsUserCheckable)) continue; // Skip former/separator items
        if (item->checkState() == Qt::Checked) {
            newAttendeeIds.insert(item->data(Qt::UserRole).toInt());
        }
    }

    // Always preserve former-member attendance (they are not in the active checkbox list)
    for (int fid : m_formerMemberIds) {
        newAttendeeIds.insert(fid);
    }

    // Determine adds and removes (never remove former members)
    QSet<int> toAdd = newAttendeeIds - m_savedAttendeeIds;
    QSet<int> toRemove = m_savedAttendeeIds - newAttendeeIds;
    // Safety: never remove former members
    for (int fid : m_formerMemberIds) {
        toRemove.remove(fid);
    }

    if (toAdd.isEmpty() && toRemove.isEmpty()) {
        // Nothing to do
        setDirty(false);
        m_statusMessageLabel->setText(tr("✓ No changes to save."));
        m_statusMessageLabel->setStyleSheet("color: palette(mid); font-style: italic; padding: 4px;");
        m_statusMessageLabel->setVisible(true);
        return;
    }

    QSqlDatabase db = DatabaseManager::instance().database();

    // Use a transaction
    if (!db.transaction()) {
        QMessageBox::critical(this, tr("Database Error"), tr("Failed to start transaction: %1").arg(db.lastError().text()));
        return;
    }

    bool ok = true;
    QString errMsg;

    // Removals
    for (int mid : toRemove) {
        QSqlQuery qd(db);
        qd.prepare("DELETE FROM meeting_attendance WHERE meeting_id = :mid AND member_id = :mem;");
        qd.bindValue(":mid", meetingId);
        qd.bindValue(":mem", mid);
        if (!qd.exec()) {
            ok = false;
            errMsg = qd.lastError().text();
            break;
        }
    }

    // Additions
    if (ok) {
        for (int mid : toAdd) {
            QSqlQuery qi(db);
            qi.prepare("INSERT OR IGNORE INTO meeting_attendance (meeting_id, member_id) VALUES (:mid, :mem);");
            qi.bindValue(":mid", meetingId);
            qi.bindValue(":mem", mid);
            if (!qi.exec()) {
                ok = false;
                errMsg = qi.lastError().text();
                break;
            }
        }
    }

    if (!ok) {
        db.rollback();
        m_statusMessageLabel->setText(tr("Save failed: %1").arg(errMsg));
        m_statusMessageLabel->setStyleSheet("color: #d32f2f; font-weight: bold; padding: 4px;");
        m_statusMessageLabel->setVisible(true);
        QMessageBox::critical(this, tr("Database Error"),
                              tr("Failed to save attendance register:\n\n%1\n\nChanges rolled back.").arg(errMsg));
        return;
    }

    db.commit();

    // Update snapshot
    m_savedAttendeeIds = newAttendeeIds;
    // Reload to sync state (in case of former members, etc.)
    loadRegister();

    setDirty(false);
    m_statusMessageLabel->setText(tr("✓ Attendance register saved successfully."));
    m_statusMessageLabel->setStyleSheet("color: #2e7d32; font-weight: bold; padding: 4px;");
    m_statusMessageLabel->setVisible(true);

    emit attendanceChanged(meetingId);
}

void AttendanceRegisterView::onCancelClicked()
{
    if (m_isDirty) {
        if (!maybeConfirmDiscard()) return;
    }

    // Reload to restore state from DB
    loadRegister();
    emit cancelRequested();
}

void AttendanceRegisterView::selectMeeting(int meetingId)
{
    // Find and select the matching combo entry
    for (int i = 0; i < m_meetingCombo->count(); ++i) {
        if (m_meetingCombo->itemData(i).toInt() == meetingId) {
            m_meetingCombo->setCurrentIndex(i);
            return;
        }
    }
    // If not found, reload meetings and try again
    loadMeetings();
    for (int i = 0; i < m_meetingCombo->count(); ++i) {
        if (m_meetingCombo->itemData(i).toInt() == meetingId) {
            m_meetingCombo->setCurrentIndex(i);
            return;
        }
    }
}

} // namespace MSMS
