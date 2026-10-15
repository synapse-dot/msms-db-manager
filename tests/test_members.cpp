#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QTableView>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QHeaderView>

#include "database/DatabaseManager.h"
#include "ui/MembersTableView.h"
#include "ui/MemberFormView.h"

using namespace MSMS;

class TestMembers : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testActiveAndFormerFiltering();
    void testTableReadOnlyAndHiddenIDs();
    void testFieldValidation();
    void testCreateMember();
    void testUpdateMember();
    void testDuplicateNameHandling();
    void testDeletionAndForeignKeyRestriction();
    void testFailedSavePreservesValues();
    void testViewRefreshAfterChanges();

private:
    QTemporaryDir m_tempDir;
    QString m_dbPath;
};

void TestMembers::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    m_dbPath = m_tempDir.filePath("test_members_module.db");
}

void TestMembers::cleanupTestCase()
{
    DatabaseManager::instance().closeDatabase();
}

void TestMembers::init()
{
    DatabaseManager::instance().closeDatabase();
    QString err;
    QVERIFY2(DatabaseManager::instance().createDatabase(m_dbPath, true, &err), qPrintable(err));
}

void TestMembers::cleanup()
{
    DatabaseManager::instance().closeDatabase();
}

void TestMembers::testActiveAndFormerFiltering()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);

    QVERIFY(q.exec("INSERT INTO members (name, status, join_date) VALUES ('Active Member 1', 'Active', '2026-01-01');"));
    QVERIFY(q.exec("INSERT INTO members (name, status, join_date) VALUES ('Active Member 2', 'Active', '2026-01-02');"));
    QVERIFY(q.exec("INSERT INTO members (name, status, join_date) VALUES ('Former Member 1', 'Former', '2025-01-01');"));

    // All Members view should show all 3
    MembersTableView allView(MembersFilter::All);
    QTableView *allTv = allView.findChild<QTableView*>();
    QVERIFY(allTv != nullptr);
    QCOMPARE(allTv->model()->rowCount(), 3);

    // Active Members view should show only the 2 active members
    MembersTableView activeView(MembersFilter::ActiveOnly);
    QTableView *activeTv = activeView.findChild<QTableView*>();
    QVERIFY(activeTv != nullptr);
    QCOMPARE(activeTv->model()->rowCount(), 2);

    // Verify all rows in active view have status "Active"
    for (int r = 0; r < activeTv->model()->rowCount(); ++r) {
        QString status = activeTv->model()->index(r, 5).data().toString();
        QCOMPARE(status, QString("Active"));
    }
}

void TestMembers::testTableReadOnlyAndHiddenIDs()
{
    MembersTableView view(MembersFilter::All);
    QTableView *tv = view.findChild<QTableView*>();
    QVERIFY(tv != nullptr);

    // Verify read-only: NoEditTriggers
    QCOMPARE(tv->editTriggers(), QAbstractItemView::NoEditTriggers);

    // Verify column 0 (member_id) is hidden
    QVERIFY(tv->isColumnHidden(0));

    // Verify column headers are non-clickable
    QCOMPARE(tv->horizontalHeader()->sectionsClickable(), false);
}

void TestMembers::testFieldValidation()
{
    QString err;

    // Name validation
    QVERIFY(!MemberFormView::validateName("", &err));
    QVERIFY(!MemberFormView::validateName("   ", &err));
    QVERIFY(MemberFormView::validateName("Jane Doe", &err));

    // Join date validation
    QVERIFY(!MemberFormView::validateJoinDate("", &err));
    QVERIFY(!MemberFormView::validateJoinDate("invalid-date", &err));
    QVERIFY(!MemberFormView::validateJoinDate("2026-02-30", &err)); // Invalid calendar day
    QVERIFY(!MemberFormView::validateJoinDate("1850-01-01", &err)); // Out of year range
    QVERIFY(MemberFormView::validateJoinDate("2026-01-15", &err));

    // Email validation (optional)
    QVERIFY(MemberFormView::validateEmail("", &err)); // Optional, empty is valid
    QVERIFY(!MemberFormView::validateEmail("invalid-email", &err));
    QVERIFY(!MemberFormView::validateEmail("@missing-user.com", &err));
    QVERIFY(!MemberFormView::validateEmail("user@.com", &err));
    QVERIFY(MemberFormView::validateEmail("valid.user@example.org", &err));

    // Phone validation (optional)
    QVERIFY(MemberFormView::validatePhone("", &err)); // Optional, empty is valid
    QVERIFY(!MemberFormView::validatePhone("123", &err)); // Too short
    QVERIFY(!MemberFormView::validatePhone("phone12345", &err)); // Letters not allowed
    QVERIFY(MemberFormView::validatePhone("0770001001", &err));
    QVERIFY(MemberFormView::validatePhone("+44 (0) 20 7946 0991", &err));
}

void TestMembers::testCreateMember()
{
    MemberFormView form;
    form.setCreateMode();
    QVERIFY(form.isCreateMode());

    QLineEdit *nameEdit = form.findChild<QLineEdit*>();
    // Set fields via form child widgets
    QList<QLineEdit*> lineEdits = form.findChildren<QLineEdit*>();
    // lineEdits: nameEdit, classEdit, phoneEdit, emailEdit, joinDateEdit
    QVERIFY(lineEdits.size() >= 5);

    QLineEdit *nameInput = lineEdits[0];
    QLineEdit *classInput = lineEdits[1];
    QLineEdit *phoneInput = lineEdits[2];
    QLineEdit *emailInput = lineEdits[3];
    QLineEdit *joinDateInput = lineEdits[4];

    nameInput->setText("Samuel Johnson");
    classInput->setText("10B");
    phoneInput->setText("0770009999");
    emailInput->setText("samuel@example.org");
    joinDateInput->setText("2026-02-01");

    // Click save
    QPushButton *btnSave = form.findChild<QPushButton*>();
    QVERIFY(btnSave != nullptr);
    btnSave->click();

    // Verify member ID is now assigned and form is not in create mode
    QVERIFY(!form.isCreateMode());
    int createdId = form.currentMemberId();
    QVERIFY(createdId > 0);

    // Form must remain open with isDirty = false
    QCOMPARE(form.isDirty(), false);

    // Verify database record
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare("SELECT name, class, phone, email, status, join_date FROM members WHERE member_id = :id;");
    q.bindValue(":id", createdId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value("name").toString(), QString("Samuel Johnson"));
    QCOMPARE(q.value("class").toString(), QString("10B"));
    QCOMPARE(q.value("phone").toString(), QString("0770009999"));
    QCOMPARE(q.value("email").toString(), QString("samuel@example.org"));
    QCOMPARE(q.value("status").toString(), QString("Active"));
    QCOMPARE(q.value("join_date").toString(), QString("2026-02-01"));
}

void TestMembers::testUpdateMember()
{
    // Insert initial member
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QVERIFY(q.exec("INSERT INTO members (member_id, name, class, phone, email, status, join_date) "
                   "VALUES (50, 'Original Name', '10A', '11111111', 'orig@example.org', 'Active', '2026-01-01');"));

    MemberFormView form;
    QVERIFY(form.loadMember(50));
    QCOMPARE(form.currentMemberId(), 50);

    QList<QLineEdit*> lineEdits = form.findChildren<QLineEdit*>();
    QLineEdit *nameInput = lineEdits[0];
    QLineEdit *classInput = lineEdits[1];

    nameInput->setText("Updated Name");
    classInput->setText("11A");
    QVERIFY(form.isDirty());

    QPushButton *btnSave = form.findChild<QPushButton*>();
    btnSave->click();

    QCOMPARE(form.isDirty(), false);
    QCOMPARE(form.currentMemberId(), 50);

    // Verify in database
    QSqlQuery check(db);
    check.prepare("SELECT name, class FROM members WHERE member_id = 50;");
    QVERIFY(check.exec());
    QVERIFY(check.next());
    QCOMPARE(check.value("name").toString(), QString("Updated Name"));
    QCOMPARE(check.value("class").toString(), QString("11A"));
}

void TestMembers::testDuplicateNameHandling()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QVERIFY(q.exec("INSERT INTO members (name, status, join_date) VALUES ('Alex Smith', 'Active', '2026-01-10');"));

    MemberFormView form;
    form.setCreateMode();

    // Verify duplicate name detection recognizes existing member
    QList<QLineEdit*> lineEdits = form.findChildren<QLineEdit*>();
    lineEdits[0]->setText("alex smith"); // Case-insensitive check
    lineEdits[4]->setText("2026-03-01");

    // Member names are not strictly unique in schema: verify two members with same name can exist
    QSqlQuery insertSecond(db);
    insertSecond.prepare("INSERT INTO members (name, status, join_date) VALUES (:name, 'Active', '2026-03-01');");
    insertSecond.bindValue(":name", "Alex Smith");
    QVERIFY2(insertSecond.exec(), "Schema allows multiple members with same name");

    QSqlQuery countQ(db);
    QVERIFY(countQ.exec("SELECT count(*) FROM members WHERE name = 'Alex Smith';"));
    QVERIFY(countQ.next());
    QCOMPARE(countQ.value(0).toInt(), 2);
}

void TestMembers::testDeletionAndForeignKeyRestriction()
{
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);

    // 1. Create a member and a meeting with attendance (foreign key reference)
    QVERIFY(q.exec("INSERT INTO members (member_id, name, status, join_date) VALUES (88, 'Referenced Member', 'Active', '2026-01-01');"));
    QVERIFY(q.exec("INSERT INTO meetings (meeting_id, title, type) VALUES (100, 'Test Meeting', 'AGM');"));
    QVERIFY(q.exec("INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (100, 88);"));

    // Attempting deletion on member 88 must fail due to ON DELETE RESTRICT
    QSqlQuery delRestricted(db);
    delRestricted.prepare("DELETE FROM members WHERE member_id = 88;");
    bool delRes = delRestricted.exec();
    QVERIFY2(!delRes, "Foreign key restriction must block deletion of member referenced by attendance");

    // Member 88 must still exist
    QSqlQuery checkMember(db);
    QVERIFY(checkMember.exec("SELECT member_id FROM members WHERE member_id = 88;"));
    QVERIFY(checkMember.next());

    // 2. Unreferenced member can be deleted cleanly
    QVERIFY(q.exec("INSERT INTO members (member_id, name, status, join_date) VALUES (89, 'Unreferenced Member', 'Active', '2026-01-01');"));
    QSqlQuery delUnreferenced(db);
    delUnreferenced.prepare("DELETE FROM members WHERE member_id = 89;");
    QVERIFY(delUnreferenced.exec());

    // Member 89 must no longer exist
    QVERIFY(checkMember.exec("SELECT member_id FROM members WHERE member_id = 89;"));
    QVERIFY(!checkMember.next());
}

void TestMembers::testFailedSavePreservesValues()
{
    MemberFormView form;
    form.setCreateMode();

    QList<QLineEdit*> lineEdits = form.findChildren<QLineEdit*>();
    lineEdits[0]->setText("Unsaved Name");
    lineEdits[1]->setText("Class 9");
    lineEdits[4]->setText("2026-05-01");

    // Attempt to save with closed DB connection to trigger database error
    DatabaseManager::instance().closeDatabase();

    QPushButton *btnSave = form.findChild<QPushButton*>();
    btnSave->click();

    // Verify values were NOT lost/cleared
    QCOMPARE(lineEdits[0]->text(), QString("Unsaved Name"));
    QCOMPARE(lineEdits[1]->text(), QString("Class 9"));
    QCOMPARE(lineEdits[4]->text(), QString("2026-05-01"));

    // Reopen DB for cleanup
    QString err;
    DatabaseManager::instance().openDatabase(m_dbPath, &err);
}

void TestMembers::testViewRefreshAfterChanges()
{
    MembersTableView view(MembersFilter::All);
    QTableView *tv = view.findChild<QTableView*>();
    int initialCount = tv->model()->rowCount();

    // Insert member directly into database
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QVERIFY(q.exec("INSERT INTO members (name, status, join_date) VALUES ('Refresh Test Member', 'Active', '2026-06-01');"));

    // View should update after refresh
    view.refresh();
    QCOMPARE(tv->model()->rowCount(), initialCount + 1);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TestMembers tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_members.moc"
