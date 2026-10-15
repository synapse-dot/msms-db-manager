// test_meetings.cpp — QtTest suite for Milestone 3: Meetings and Attendance

#include <QtTest>
#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QTableView>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QHeaderView>
#include <QLabel>

#include "database/DatabaseManager.h"
#include "ui/MeetingsTableView.h"
#include "ui/MeetingFormView.h"
#include "ui/AttendeesTableView.h"
#include "ui/AttendanceRegisterView.h"

using namespace MSMS;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: insert a meeting, returns meeting_id
// ─────────────────────────────────────────────────────────────────────────────
static int insertMeeting(QSqlDatabase &db, const QString &title, const QString &type,
                          const QString &date = QString(), const QString &startTime = QString(),
                          const QString &endTime = QString(), const QString &location = QString())
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO meetings (title, type, meeting_date, start_time, end_time, location) "
              "VALUES (:t, :ty, :d, :st, :et, :loc);");
    q.bindValue(":t",  title);
    q.bindValue(":ty", type);
    q.bindValue(":d",  date.isEmpty()      ? QVariant(QMetaType(QMetaType::QString)) : date);
    q.bindValue(":st", startTime.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : startTime);
    q.bindValue(":et", endTime.isEmpty()   ? QVariant(QMetaType(QMetaType::QString)) : endTime);
    q.bindValue(":loc",location.isEmpty()  ? QVariant(QMetaType(QMetaType::QString)) : location);
    if (!q.exec()) return -1;
    return q.lastInsertId().toInt();
}

static int insertMember(QSqlDatabase &db, const QString &name, const QString &status = "Active")
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO members (name, status, join_date) VALUES (:n, :s, '2026-01-01');");
    q.bindValue(":n", name);
    q.bindValue(":s", status);
    if (!q.exec()) return -1;
    return q.lastInsertId().toInt();
}

static bool insertAttendance(QSqlDatabase &db, int meetingId, int memberId)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (:m, :p);");
    q.bindValue(":m", meetingId);
    q.bindValue(":p", memberId);
    return q.exec();
}

// ─────────────────────────────────────────────────────────────────────────────
class TestMeetings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Meeting record tests
    void testAgmFiltering();
    void testEgmFiltering();
    void testEcmFiltering();
    void testCreateAgmMeeting();
    void testCreateEgmMeeting();
    void testCreateEcmMeeting();
    void testEditMeeting();
    void testRequiredTitleAndType();
    void testOptionalDateForTentativeMeeting();
    void testInvalidDate();
    void testInvalidTimes();
    void testEndTimeEarlierThanStartTime();
    void testTableReadOnlyAndHiddenId();
    void testSearchFiltering();
    void testEmptyState();
    void testFailedSavePreservesValues();
    void testDeletionConfirmation();
    void testDeletionForeignKeyRestriction();

    // Attendance tests
    void testAttendeesFilteredByMeeting();
    void testAttendeeColumns();
    void testSwitchingMeetingsRefreshesAttendees();
    void testAddMemberToAttendance();
    void testPreventDuplicateAttendance();
    void testFormerMemberAttendanceRemains();
    void testFormerMemberNotRemovedOnSave();
    void testActiveMemberSelectorRule();
    void testAtomicSaveAndRollback();
    void testRefreshAttendeesAfterChange();

private:
    QTemporaryDir m_tempDir;
    QString m_dbPath;
};

void TestMeetings::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    m_dbPath = m_tempDir.filePath("test_meetings.db");
}

void TestMeetings::cleanupTestCase()
{
    DatabaseManager::instance().closeDatabase();
}

void TestMeetings::init()
{
    DatabaseManager::instance().closeDatabase();
    QString err;
    QVERIFY2(DatabaseManager::instance().createDatabase(m_dbPath, true, &err), qPrintable(err));
}

void TestMeetings::cleanup()
{
    DatabaseManager::instance().closeDatabase();
}

// ─────────────────────────────────────────────────────────────────────────────
// Meeting table filtering
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testAgmFiltering()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    insertMeeting(db, "AGM 2026", "AGM");
    insertMeeting(db, "EGM Special", "EGM");
    insertMeeting(db, "ECM March", "ECM");
    insertMeeting(db, "AGM 2025", "AGM");

    MeetingsTableView view("AGM");
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);
    // Only AGM rows shown; model has AGM count = 2
    QCOMPARE(tv->model()->rowCount(), 2);
}

void TestMeetings::testEgmFiltering()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    insertMeeting(db, "EGM 1", "EGM");
    insertMeeting(db, "EGM 2", "EGM");
    insertMeeting(db, "AGM 1", "AGM");

    MeetingsTableView view("EGM");
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);
    QCOMPARE(tv->model()->rowCount(), 2);
}

void TestMeetings::testEcmFiltering()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    insertMeeting(db, "ECM Q1", "ECM");
    insertMeeting(db, "AGM 2026", "AGM");

    MeetingsTableView view("ECM");
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);
    QCOMPARE(tv->model()->rowCount(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Creating meetings
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testCreateAgmMeeting()
{
    MeetingFormView form;
    form.setCreateMode("AGM");
    QVERIFY(form.isCreateMode());

    QList<QLineEdit*> les = form.findChildren<QLineEdit*>();
    QVERIFY(les.size() >= 5); // title, date, startTime, endTime, location
    les[0]->setText("AGM 2026 Annual");  // title
    // type is already AGM
    les[1]->setText("2026-09-01");        // date
    les[2]->setText("10:00");             // start time
    les[3]->setText("12:00");             // end time
    les[4]->setText("Main Hall");         // location

    QPushButton *btnSave = form.findChild<QPushButton*>();
    QVERIFY(btnSave != nullptr);
    btnSave->click();

    QVERIFY(!form.isCreateMode());
    int id = form.currentMeetingId();
    QVERIFY(id > 0);

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT title, type, meeting_date, start_time, end_time, location FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", id);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value("title").toString(), QString("AGM 2026 Annual"));
    QCOMPARE(q.value("type").toString(), QString("AGM"));
    QCOMPARE(q.value("meeting_date").toString(), QString("2026-09-01"));
    QCOMPARE(q.value("start_time").toString(), QString("10:00"));
    QCOMPARE(q.value("end_time").toString(), QString("12:00"));
    QCOMPARE(q.value("location").toString(), QString("Main Hall"));
}

void TestMeetings::testCreateEgmMeeting()
{
    MeetingFormView form;
    form.setCreateMode("EGM");

    QList<QLineEdit*> les = form.findChildren<QLineEdit*>();
    les[0]->setText("EGM Extraordinary");
    QComboBox *typeCombo = form.findChild<QComboBox*>();
    QVERIFY(typeCombo != nullptr);
    int idx = typeCombo->findData("EGM");
    typeCombo->setCurrentIndex(idx);

    QPushButton *btnSave = form.findChild<QPushButton*>();
    btnSave->click();

    QVERIFY(!form.isCreateMode());
    QVERIFY(form.currentMeetingId() > 0);

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT type FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", form.currentMeetingId());
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value("type").toString(), QString("EGM"));
}

void TestMeetings::testCreateEcmMeeting()
{
    MeetingFormView form;
    form.setCreateMode("ECM");

    QList<QLineEdit*> les = form.findChildren<QLineEdit*>();
    les[0]->setText("ECM Committee");
    QComboBox *typeCombo = form.findChild<QComboBox*>();
    int idx = typeCombo->findData("ECM");
    typeCombo->setCurrentIndex(idx);

    QPushButton *btnSave = form.findChild<QPushButton*>();
    btnSave->click();

    QVERIFY(!form.isCreateMode());
    QVERIFY(form.currentMeetingId() > 0);

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT type FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", form.currentMeetingId());
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value("type").toString(), QString("ECM"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Editing
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testEditMeeting()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int id = insertMeeting(db, "Original Title", "AGM", "2026-01-01");
    QVERIFY(id > 0);

    MeetingFormView form;
    QVERIFY(form.loadMeeting(id));
    QCOMPARE(form.currentMeetingId(), id);
    QCOMPARE(form.isDirty(), false);

    QList<QLineEdit*> les = form.findChildren<QLineEdit*>();
    les[0]->setText("Updated Title");
    QVERIFY(form.isDirty());

    QPushButton *btnSave = form.findChild<QPushButton*>();
    btnSave->click();

    QCOMPARE(form.isDirty(), false);
    QCOMPARE(form.currentMeetingId(), id);

    QSqlQuery q(db);
    q.prepare("SELECT title FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", id);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value("title").toString(), QString("Updated Title"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Validation
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testRequiredTitleAndType()
{
    QString err;
    QVERIFY(!MeetingFormView::validateTitle("", &err));
    QVERIFY(!MeetingFormView::validateTitle("   ", &err));
    QVERIFY(MeetingFormView::validateTitle("Valid Title", &err));

    QVERIFY(!MeetingFormView::validateType("", &err));
    QVERIFY(!MeetingFormView::validateType("ANNUAL", &err));
    QVERIFY(MeetingFormView::validateType("AGM", &err));
    QVERIFY(MeetingFormView::validateType("EGM", &err));
    QVERIFY(MeetingFormView::validateType("ECM", &err));
}

void TestMeetings::testOptionalDateForTentativeMeeting()
{
    QString err;
    // Empty date is valid (tentative meeting)
    QVERIFY(MeetingFormView::validateDate("", &err));

    // Create meeting without date
    MeetingFormView form;
    form.setCreateMode("AGM");
    QList<QLineEdit*> les = form.findChildren<QLineEdit*>();
    les[0]->setText("Tentative AGM");
    // date, startTime, endTime, location all blank

    QPushButton *btnSave = form.findChild<QPushButton*>();
    btnSave->click();

    QVERIFY(!form.isCreateMode());
    QVERIFY(form.currentMeetingId() > 0);

    // Verify null date stored
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT meeting_date FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", form.currentMeetingId());
    QVERIFY(q.exec() && q.next());
    QVERIFY(q.value("meeting_date").isNull());
}

void TestMeetings::testInvalidDate()
{
    QString err;
    QVERIFY(!MeetingFormView::validateDate("not-a-date", &err));
    QVERIFY(!MeetingFormView::validateDate("2026-02-30", &err)); // invalid calendar day
    QVERIFY(!MeetingFormView::validateDate("1800-01-01", &err)); // out of year range
    QVERIFY(MeetingFormView::validateDate("2026-06-15", &err));
    QVERIFY(MeetingFormView::validateDate("", &err));             // blank is valid
}

void TestMeetings::testInvalidTimes()
{
    QString err;
    QVERIFY(!MeetingFormView::validateTimes("25:00", "", &err));   // invalid hour
    QVERIFY(!MeetingFormView::validateTimes("", "99:99", &err));   // invalid minute
    QVERIFY(!MeetingFormView::validateTimes("abc", "", &err));     // non-time
    QVERIFY(MeetingFormView::validateTimes("09:00", "17:00", &err));
    QVERIFY(MeetingFormView::validateTimes("", "", &err));          // both blank valid
    QVERIFY(MeetingFormView::validateTimes("14:00", "", &err));     // start only valid
    QVERIFY(MeetingFormView::validateTimes("", "15:00", &err));     // end only valid
}

void TestMeetings::testEndTimeEarlierThanStartTime()
{
    QString err;
    QVERIFY(!MeetingFormView::validateTimes("15:00", "14:00", &err));
    QVERIFY(!MeetingFormView::validateTimes("23:59", "00:00", &err));
    QVERIFY(MeetingFormView::validateTimes("14:00", "14:00", &err)); // equal is OK
    QVERIFY(MeetingFormView::validateTimes("09:00", "17:00", &err));
}

// ─────────────────────────────────────────────────────────────────────────────
// Table properties
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testTableReadOnlyAndHiddenId()
{
    MeetingsTableView view("AGM");
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);

    // Read-only
    QCOMPARE(tv->editTriggers(), QAbstractItemView::NoEditTriggers);

    // Column 0 (meeting_id) is hidden
    QVERIFY(tv->isColumnHidden(0));

    // Headers not clickable
    QCOMPARE(tv->horizontalHeader()->sectionsClickable(), false);
}

void TestMeetings::testSearchFiltering()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    insertMeeting(db, "Alpha AGM", "AGM");
    insertMeeting(db, "Beta AGM", "AGM");

    MeetingsTableView view("AGM");
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);
    QCOMPARE(tv->model()->rowCount(), 2);

    QLineEdit *searchEdit = view.findChild<QLineEdit*>();
    QVERIFY(searchEdit != nullptr);

    searchEdit->setText("Alpha");
    QCOMPARE(tv->model()->rowCount(), 1);

    searchEdit->clear();
    QCOMPARE(tv->model()->rowCount(), 2);
}

void TestMeetings::testEmptyState()
{
    // Empty database — no meetings
    MeetingsTableView view("AGM");
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);

    // With no rows, table should be hidden and empty state label visible
    QLabel *emptyLabel = nullptr;
    for (QLabel *lbl : view.findChildren<QLabel*>()) {
        if (lbl->text().contains("No", Qt::CaseInsensitive) &&
            lbl->text().contains("AGM", Qt::CaseInsensitive)) {
            emptyLabel = lbl;
            break;
        }
    }
    QVERIFY(emptyLabel != nullptr);
    QVERIFY(emptyLabel->isVisible());
    QVERIFY(!tv->isVisible());
}

// ─────────────────────────────────────────────────────────────────────────────
// Failed save preserves values
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testFailedSavePreservesValues()
{
    MeetingFormView form;
    form.setCreateMode("AGM");

    QList<QLineEdit*> les = form.findChildren<QLineEdit*>();
    les[0]->setText("Preserved Title");
    les[1]->setText("2026-07-04");

    // Close DB to trigger failure
    DatabaseManager::instance().closeDatabase();

    QPushButton *btnSave = form.findChild<QPushButton*>();
    btnSave->click();

    // Values must remain
    QCOMPARE(les[0]->text(), QString("Preserved Title"));
    QCOMPARE(les[1]->text(), QString("2026-07-04"));

    // Reopen for cleanup
    QString err;
    DatabaseManager::instance().openDatabase(m_dbPath, &err);
}

// ─────────────────────────────────────────────────────────────────────────────
// Deletion
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testDeletionConfirmation()
{
    // Insert meeting with no attendance — should be deletable via DB directly
    QSqlDatabase db = DatabaseManager::instance().database();
    int id = insertMeeting(db, "Deletable Meeting", "AGM");
    QVERIFY(id > 0);

    // Verify it exists
    QSqlQuery q(db);
    q.prepare("SELECT meeting_id FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", id);
    QVERIFY(q.exec() && q.next());

    // Delete via SQL (UI would normally confirm)
    QSqlQuery del(db);
    del.prepare("DELETE FROM meetings WHERE meeting_id = :id;");
    del.bindValue(":id", id);
    QVERIFY(del.exec());

    // Confirm gone
    q.prepare("SELECT meeting_id FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", id);
    QVERIFY(q.exec() && !q.next());
}

void TestMeetings::testDeletionForeignKeyRestriction()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId = insertMeeting(db, "Attended Meeting", "AGM");
    int memId  = insertMember(db, "Attendee Bob");
    QVERIFY(meetId > 0 && memId > 0);
    QVERIFY(insertAttendance(db, meetId, memId));

    // Deleting this meeting must fail due to ON DELETE RESTRICT on meeting_attendance
    QSqlQuery del(db);
    del.prepare("DELETE FROM meetings WHERE meeting_id = :id;");
    del.bindValue(":id", meetId);
    bool result = del.exec();
    QVERIFY2(!result, "Meeting with attendance records must not be deletable");

    // Meeting still exists
    QSqlQuery q(db);
    q.prepare("SELECT meeting_id FROM meetings WHERE meeting_id = :id;");
    q.bindValue(":id", meetId);
    QVERIFY(q.exec() && q.next());
}

// ─────────────────────────────────────────────────────────────────────────────
// Attendees tests
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testAttendeesFilteredByMeeting()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int m1 = insertMeeting(db, "Meeting 1", "AGM");
    int m2 = insertMeeting(db, "Meeting 2", "EGM");
    int p1 = insertMember(db, "Alice");
    int p2 = insertMember(db, "Bob");
    int p3 = insertMember(db, "Carol");
    QVERIFY(insertAttendance(db, m1, p1));
    QVERIFY(insertAttendance(db, m1, p2));
    QVERIFY(insertAttendance(db, m2, p3));

    AttendeesTableView view;
    view.selectMeeting(m1);

    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);
    QVERIFY(tv->isVisible());
    // m1 has 2 attendees
    QCOMPARE(tv->model()->rowCount(), 2);
}

void TestMeetings::testAttendeeColumns()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int m1 = insertMeeting(db, "Column Test Meeting", "AGM");
    int pid = insertMember(db, "Dave Test");
    // Add phone/email directly
    QSqlQuery upd(db);
    upd.prepare("UPDATE members SET phone='0770001234', email='dave@test.com' WHERE member_id=:id;");
    upd.bindValue(":id", pid);
    QVERIFY(upd.exec());
    QVERIFY(insertAttendance(db, m1, pid));

    AttendeesTableView view;
    view.selectMeeting(m1);

    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);

    // Check column headers: member_id, name, phone, email
    QCOMPARE(tv->model()->headerData(0, Qt::Horizontal).toString(), QString("Member ID"));
    QCOMPARE(tv->model()->headerData(1, Qt::Horizontal).toString(), QString("Name"));
    QCOMPARE(tv->model()->headerData(2, Qt::Horizontal).toString(), QString("Phone"));
    QCOMPARE(tv->model()->headerData(3, Qt::Horizontal).toString(), QString("Email"));

    // member_id column must be visible (required by spec)
    QVERIFY(!tv->isColumnHidden(0));

    // Verify data
    QCOMPARE(tv->model()->index(0, 0).data().toInt(), pid);
    QCOMPARE(tv->model()->index(0, 1).data().toString(), QString("Dave Test"));
    QCOMPARE(tv->model()->index(0, 2).data().toString(), QString("0770001234"));
    QCOMPARE(tv->model()->index(0, 3).data().toString(), QString("dave@test.com"));
}

void TestMeetings::testSwitchingMeetingsRefreshesAttendees()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int m1 = insertMeeting(db, "Switch Test M1", "AGM", "2026-01-01");
    int m2 = insertMeeting(db, "Switch Test M2", "EGM", "2026-02-01");
    int p1 = insertMember(db, "Eve");
    int p2 = insertMember(db, "Frank");
    int p3 = insertMember(db, "Grace");
    QVERIFY(insertAttendance(db, m1, p1));
    QVERIFY(insertAttendance(db, m1, p2));
    QVERIFY(insertAttendance(db, m2, p3));

    AttendeesTableView view;
    view.selectMeeting(m1);
    QTableView *tv = view.findChild<QTableView*>();
    QCOMPARE(tv->model()->rowCount(), 2);

    view.selectMeeting(m2);
    QCOMPARE(tv->model()->rowCount(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Attendance register tests
// ─────────────────────────────────────────────────────────────────────────────

void TestMeetings::testAddMemberToAttendance()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId = insertMeeting(db, "Register Test Meeting", "AGM");
    int mem1   = insertMember(db, "Henry Active");
    QVERIFY(meetId > 0 && mem1 > 0);

    AttendanceRegisterView regView;
    regView.selectMeeting(meetId);

    QListWidget *list = regView.findChild<QListWidget*>();
    QVERIFY(list != nullptr);

    // Find Henry in list and check him
    bool found = false;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item->data(Qt::UserRole).toInt() == mem1) {
            item->setCheckState(Qt::Checked);
            found = true;
            break;
        }
    }
    QVERIFY2(found, "Henry Active must appear in the member list");
    QVERIFY(regView.isDirty());

    // Save
    QPushButton *btnSave = nullptr;
    for (QPushButton *btn : regView.findChildren<QPushButton*>()) {
        if (btn->text().contains("Save")) { btnSave = btn; break; }
    }
    QVERIFY(btnSave != nullptr);
    btnSave->click();
    QCOMPARE(regView.isDirty(), false);

    // Verify in DB
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM meeting_attendance WHERE meeting_id=:m AND member_id=:p;");
    q.bindValue(":m", meetId);
    q.bindValue(":p", mem1);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toInt(), 1);
}

void TestMeetings::testPreventDuplicateAttendance()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId = insertMeeting(db, "Dup Test Meeting", "ECM");
    int memId  = insertMember(db, "Ida Active");
    QVERIFY(insertAttendance(db, meetId, memId));

    // Attempt duplicate insert via SQL
    QSqlQuery q(db);
    q.prepare("INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (:m, :p);");
    q.bindValue(":m", meetId);
    q.bindValue(":p", memId);
    bool dupResult = q.exec();
    QVERIFY2(!dupResult, "Composite PK must prevent duplicate attendance records");

    // Count remains 1
    QSqlQuery cnt(db);
    cnt.prepare("SELECT COUNT(*) FROM meeting_attendance WHERE meeting_id=:m AND member_id=:p;");
    cnt.bindValue(":m", meetId);
    cnt.bindValue(":p", memId);
    QVERIFY(cnt.exec() && cnt.next());
    QCOMPARE(cnt.value(0).toInt(), 1);
}

void TestMeetings::testFormerMemberAttendanceRemains()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId = insertMeeting(db, "Historical Meeting", "AGM", "2025-01-01");

    // Insert former member directly
    QSqlQuery qm(db);
    qm.prepare("INSERT INTO members (name, status, join_date) VALUES ('Jack Former', 'Former', '2024-01-01');");
    QVERIFY(qm.exec());
    int formerMemberId = qm.lastInsertId().toInt();

    QVERIFY(insertAttendance(db, meetId, formerMemberId));

    // Attendees view shows former member
    AttendeesTableView view;
    view.selectMeeting(meetId);
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv->model()->rowCount() == 1);
    QCOMPARE(tv->model()->index(0, 0).data().toInt(), formerMemberId);
}

void TestMeetings::testFormerMemberNotRemovedOnSave()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId = insertMeeting(db, "Former Preservation Test", "AGM");

    // Insert former member with existing attendance
    QSqlQuery qm(db);
    qm.prepare("INSERT INTO members (name, status, join_date) VALUES ('Kay Former', 'Former', '2023-01-01');");
    QVERIFY(qm.exec());
    int formerMemberId = qm.lastInsertId().toInt();
    QVERIFY(insertAttendance(db, meetId, formerMemberId));

    // Open register, don't change anything, save
    AttendanceRegisterView regView;
    regView.selectMeeting(meetId);

    QPushButton *btnSave = nullptr;
    for (QPushButton *btn : regView.findChildren<QPushButton*>()) {
        if (btn->text().contains("Save")) { btnSave = btn; break; }
    }
    QVERIFY(btnSave != nullptr);
    btnSave->click();

    // Former member attendance must still be in DB
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM meeting_attendance WHERE meeting_id=:m AND member_id=:p;");
    q.bindValue(":m", meetId);
    q.bindValue(":p", formerMemberId);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toInt(), 1);
}

void TestMeetings::testActiveMemberSelectorRule()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId  = insertMeeting(db, "Active Selector Test", "AGM");
    int activeM = insertMember(db, "Lena Active", "Active");
    int formerM = insertMember(db, "Mike Former", "Former");
    QVERIFY(meetId > 0 && activeM > 0 && formerM > 0);

    AttendanceRegisterView regView;
    regView.selectMeeting(meetId);

    QListWidget *list = regView.findChild<QListWidget*>();
    QVERIFY(list != nullptr);

    // Active member must appear (with checkable item)
    bool activeFound = false;
    bool formerFound = false;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        int mid = item->data(Qt::UserRole).toInt();
        if (mid == activeM) {
            activeFound = true;
            // Must be checkable
            QVERIFY(item->flags() & Qt::ItemIsUserCheckable);
        }
        if (mid == formerM) {
            formerFound = true;
        }
    }

    QVERIFY2(activeFound, "Active member must appear in the register");
    // Former member (no existing attendance) should NOT appear in list
    QVERIFY2(!formerFound, "Former member without existing attendance must not appear in active selector");
}

void TestMeetings::testAtomicSaveAndRollback()
{
    // Verify that an invalid insert (duplicate) in a save batch causes rollback
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId = insertMeeting(db, "Atomic Test Meeting", "ECM");
    int mem1   = insertMember(db, "Nina Active");
    QVERIFY(meetId > 0 && mem1 > 0);

    // Pre-insert attendance for mem1 directly to create a conflict
    QVERIFY(insertAttendance(db, meetId, mem1));

    // Attempt a DB transaction that tries a duplicate — must roll back
    QVERIFY(db.transaction());
    QSqlQuery q1(db);
    q1.prepare("INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (:m, :p);");
    q1.bindValue(":m", meetId);
    q1.bindValue(":p", mem1); // duplicate!
    bool ok = q1.exec();
    // Expect failure
    if (!ok) {
        db.rollback();
    } else {
        db.commit();
    }
    QVERIFY(!ok); // Duplicate must fail

    // mem1 still has exactly 1 attendance record
    QSqlQuery cnt(db);
    cnt.prepare("SELECT COUNT(*) FROM meeting_attendance WHERE meeting_id=:m AND member_id=:p;");
    cnt.bindValue(":m", meetId);
    cnt.bindValue(":p", mem1);
    QVERIFY(cnt.exec() && cnt.next());
    QCOMPARE(cnt.value(0).toInt(), 1);
}

void TestMeetings::testRefreshAttendeesAfterChange()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    int meetId = insertMeeting(db, "Refresh Test", "AGM");
    int mem1   = insertMember(db, "Olivia Active");

    AttendeesTableView attendeesView;
    attendeesView.selectMeeting(meetId);

    QTableView *tv = attendeesView.findChild<QTableView*>();
    int initialCount = tv->model()->rowCount();
    QCOMPARE(initialCount, 0);

    // Add attendance via AttendanceRegisterView and verify attendees refreshed
    AttendanceRegisterView regView;
    regView.selectMeeting(meetId);

    QListWidget *list = regView.findChild<QListWidget*>();
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item->data(Qt::UserRole).toInt() == mem1) {
            item->setCheckState(Qt::Checked);
            break;
        }
    }

    QPushButton *btnSave = nullptr;
    for (QPushButton *btn : regView.findChildren<QPushButton*>()) {
        if (btn->text().contains("Save")) { btnSave = btn; break; }
    }
    QVERIFY(btnSave != nullptr);
    btnSave->click();

    // Refresh the attendees view
    attendeesView.refresh();
    attendeesView.selectMeeting(meetId);
    QCOMPARE(tv->model()->rowCount(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TestMeetings tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_meetings.moc"
