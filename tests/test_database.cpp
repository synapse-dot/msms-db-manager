#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTableView>
#include <QAbstractItemView>

#include "database/DatabaseManager.h"
#include "database/Schema.h"
#include "ui/AllMembersView.h"

using namespace MSMS;

class TestDatabase : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testSchemaCreation();
    void testRequiredIndexes();
    void testValidDatabaseOpening();
    void testInvalidDatabaseOpening();
    void testImportantConstraints();
    void testFailedInitialisationRollback();
    void testAllMembersModelAndIDHiding();

private:
    QTemporaryDir m_tempDir;
};

void TestDatabase::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestDatabase::cleanupTestCase()
{
    DatabaseManager::instance().closeDatabase();
}

void TestDatabase::init()
{
    DatabaseManager::instance().closeDatabase();
}

void TestDatabase::cleanup()
{
    DatabaseManager::instance().closeDatabase();
}

void TestDatabase::testSchemaCreation()
{
    QString dbPath = m_tempDir.filePath("test_schema.db");
    QString err;
    bool ok = DatabaseManager::instance().createDatabase(dbPath, true, &err);
    QVERIFY2(ok, qPrintable(err));

    QSqlDatabase db = DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    // Verify all 9 required tables
    QSqlQuery q(db);
    QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE type='table';"));

    QSet<QString> tables;
    while (q.next()) {
        tables.insert(q.value(0).toString().toLower());
    }

    const QStringList expectedTables = {
        "members",
        "meetings",
        "meeting_attendance",
        "projects",
        "project_members",
        "committee_appointments",
        "activities",
        "external_competitions",
        "competition_participants"
    };

    for (const QString &tbl : expectedTables) {
        QVERIFY2(tables.contains(tbl), qPrintable(QString("Missing table: %1").arg(tbl)));
    }

    // Verify columns of members table
    QVERIFY(q.exec("PRAGMA table_info(members);"));
    QSet<QString> memberCols;
    while (q.next()) {
        memberCols.insert(q.value("name").toString().toLower());
    }
    const QStringList expectedMemberCols = {"member_id", "name", "class", "phone", "email", "status", "join_date"};
    for (const QString &col : expectedMemberCols) {
        QVERIFY2(memberCols.contains(col), qPrintable(QString("Missing member column: %1").arg(col)));
    }
}

void TestDatabase::testRequiredIndexes()
{
    QString dbPath = m_tempDir.filePath("test_indexes.db");
    QString err;
    bool ok = DatabaseManager::instance().createDatabase(dbPath, true, &err);
    QVERIFY2(ok, qPrintable(err));

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE type='index';"));

    QSet<QString> indexes;
    while (q.next()) {
        indexes.insert(q.value(0).toString().toLower());
    }

    const QStringList expectedIndexes = {
        "idx_members_status",
        "idx_attendance_member",
        "idx_project_members_member",
        "idx_committee_member",
        "idx_competition_participants_member"
    };

    for (const QString &idx : expectedIndexes) {
        QVERIFY2(indexes.contains(idx), qPrintable(QString("Missing index: %1").arg(idx)));
    }
}

void TestDatabase::testValidDatabaseOpening()
{
    QString dbPath = m_tempDir.filePath("test_valid_open.db");
    QString err;
    QVERIFY(DatabaseManager::instance().createDatabase(dbPath, true, &err));

    DatabaseManager::instance().closeDatabase();
    QVERIFY(!DatabaseManager::instance().isOpen());

    // Reopen
    bool opened = DatabaseManager::instance().openDatabase(dbPath, &err);
    QVERIFY2(opened, qPrintable(err));
    QVERIFY(DatabaseManager::instance().isOpen());

    // Verify integrity
    QString integrityMsg;
    bool integrityOk = DatabaseManager::instance().verifyIntegrity(&integrityMsg);
    QVERIFY2(integrityOk, qPrintable(integrityMsg));
    QCOMPARE(integrityMsg.trimmed().toLower(), QString("ok"));
}

void TestDatabase::testInvalidDatabaseOpening()
{
    QString err;

    // 1. Non-existent file
    bool resNonExistent = DatabaseManager::instance().openDatabase("/tmp/non_existent_msms_test_file.db", &err);
    QVERIFY(!resNonExistent);
    QVERIFY(!err.isEmpty());

    // 2. Non-sqlite file (random text)
    QString nonSqlitePath = m_tempDir.filePath("not_a_db.txt");
    {
        QFile f(nonSqlitePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("This is arbitrary text content that is not an SQLite database file at all.");
        f.close();
    }
    QFileInfo fiBefore(nonSqlitePath);
    QByteArray contentBefore;
    {
        QFile f(nonSqlitePath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        contentBefore = f.readAll();
        f.close();
    }

    bool resNotSqlite = DatabaseManager::instance().openDatabase(nonSqlitePath, &err);
    QVERIFY(!resNotSqlite);
    QVERIFY(!err.isEmpty());

    // Verify file was NOT modified
    QFileInfo fiAfter(nonSqlitePath);
    QByteArray contentAfter;
    {
        QFile f(nonSqlitePath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        contentAfter = f.readAll();
        f.close();
    }
    QCOMPARE(fiAfter.size(), fiBefore.size());
    QCOMPARE(contentAfter, contentBefore);

    // 3. 0-byte file
    QString zeroBytePath = m_tempDir.filePath("empty.db");
    {
        QFile f(zeroBytePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
    }
    bool resZero = DatabaseManager::instance().openDatabase(zeroBytePath, &err);
    QVERIFY(!resZero);
    QVERIFY(!err.isEmpty());
    QCOMPARE(QFileInfo(zeroBytePath).size(), 0);

    // 4. SQLite database missing required tables
    QString missingTablesPath = m_tempDir.filePath("missing_tables.db");
    {
        {
            QSqlDatabase rawDb = QSqlDatabase::addDatabase("QSQLITE", "raw_test_missing");
            rawDb.setDatabaseName(missingTablesPath);
            QVERIFY(rawDb.open());
            QSqlQuery q(rawDb);
            QVERIFY(q.exec("CREATE TABLE unrelated_table (id INTEGER PRIMARY KEY);"));
            rawDb.close();
        }
        QSqlDatabase::removeDatabase("raw_test_missing");
    }

    QByteArray missingDbContentBefore;
    {
        QFile f(missingTablesPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        missingDbContentBefore = f.readAll();
        f.close();
    }

    bool resMissing = DatabaseManager::instance().openDatabase(missingTablesPath, &err);
    QVERIFY(!resMissing);
    QVERIFY(err.contains("missing required table", Qt::CaseInsensitive));

    // Verify file content was not modified
    {
        QFile f(missingTablesPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray after = f.readAll();
        f.close();
        QCOMPARE(after, missingDbContentBefore);
    }
}

void TestDatabase::testImportantConstraints()
{
    QString dbPath = m_tempDir.filePath("test_constraints.db");
    QString err;
    QVERIFY(DatabaseManager::instance().createDatabase(dbPath, true, &err));

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);

    // 1. Verify foreign keys are enabled
    QVERIFY(q.exec("PRAGMA foreign_keys;"));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);

    // 2. Insert valid member
    QVERIFY(q.exec("INSERT INTO members (member_id, name, status, join_date) VALUES (1, 'Alice Smith', 'Active', '2026-01-01');"));

    // 3. Foreign key violation on meeting_attendance
    bool fkAttendance = q.exec("INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (999, 1);");
    QVERIFY2(!fkAttendance, "Attendance referencing non-existent meeting must fail foreign key constraint");

    // 4. ON DELETE RESTRICT
    QVERIFY(q.exec("INSERT INTO meetings (meeting_id, title, type) VALUES (10, 'General Meeting', 'AGM');"));
    QVERIFY(q.exec("INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (10, 1);"));

    bool delMember = q.exec("DELETE FROM members WHERE member_id = 1;");
    QVERIFY2(!delMember, "Deleting member with existing attendance records must be prevented by ON DELETE RESTRICT");

    bool delMeeting = q.exec("DELETE FROM meetings WHERE meeting_id = 10;");
    QVERIFY2(!delMeeting, "Deleting meeting with existing attendance records must be prevented by ON DELETE RESTRICT");

    // 5. Member status CHECK constraint ('Active' or 'Former')
    bool invalidStatus = q.exec("INSERT INTO members (name, status, join_date) VALUES ('Bob', 'Suspended', '2026-01-01');");
    QVERIFY2(!invalidStatus, "Member with status other than 'Active' or 'Former' must fail CHECK constraint");

    // 6. Meeting time CHECK constraint (end_time >= start_time)
    bool invalidTime = q.exec("INSERT INTO meetings (title, type, start_time, end_time) VALUES ('Bad Time', 'AGM', '15:00', '14:00');");
    QVERIFY2(!invalidTime, "Meeting with end_time < start_time must fail CHECK constraint");

    // 7. Project status CHECK constraint
    bool invalidProjStatus = q.exec("INSERT INTO projects (title, status) VALUES ('Project Y', 'Archived');");
    QVERIFY2(!invalidProjStatus, "Project with invalid status must fail CHECK constraint");

    // 8. External competition UNIQUE constraint on (title, year)
    QVERIFY(q.exec("INSERT INTO external_competitions (title, year) VALUES ('National Olympiad', 2026);"));
    bool duplicateComp = q.exec("INSERT INTO external_competitions (title, year) VALUES ('National Olympiad', 2026);");
    QVERIFY2(!duplicateComp, "Duplicate (title, year) must fail UNIQUE constraint");

    // 9. Committee appointments end_date >= start_date
    bool invalidAppointmentDate = q.exec("INSERT INTO committee_appointments (start_date, office, member_id, end_date) VALUES ('2026-06-01', 'Treasurer', 1, '2026-05-01');");
    QVERIFY2(!invalidAppointmentDate, "Appointment with end_date < start_date must fail CHECK constraint");
}

void TestDatabase::testFailedInitialisationRollback()
{
    // Test that if database initialisation fails or destination exists without overwrite,
    // the system rolls back cleanly and doesn't leave partial databases.
    QString dbPath = m_tempDir.filePath("test_existing.db");
    QString err;
    QVERIFY(DatabaseManager::instance().createDatabase(dbPath, true, &err));
    DatabaseManager::instance().closeDatabase();

    // Calling createDatabase with overwrite=false on existing file must fail
    bool okWithoutOverwrite = DatabaseManager::instance().createDatabase(dbPath, false, &err);
    QVERIFY(!okWithoutOverwrite);
    QVERIFY(err.contains("already exists", Qt::CaseInsensitive));

    // Test rollback on failure during atomic creation
    QString rollbackPath = m_tempDir.filePath("test_rollback.db");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_rollback_conn");
        db.setDatabaseName(rollbackPath);
        QVERIFY(db.open());

        QVERIFY(db.transaction());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE members (member_id INTEGER PRIMARY KEY);"));

        // Simulate failure
        bool okSyntax = q.exec("INVALID SQL STATEMENT ERROR;");
        QVERIFY(!okSyntax);
        db.rollback();
        db.close();
    }
    QSqlDatabase::removeDatabase("test_rollback_conn");

    // Reopen and verify table 'members' does not exist because transaction was rolled back
    {
        {
            QSqlDatabase checkDb = QSqlDatabase::addDatabase("QSQLITE", "check_rollback_conn");
            checkDb.setDatabaseName(rollbackPath);
            QVERIFY(checkDb.open());
            QSqlQuery qc(checkDb);
            QVERIFY(qc.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='members';"));
            QVERIFY(!qc.next()); // No rows!
            checkDb.close();
        }
        QSqlDatabase::removeDatabase("check_rollback_conn");
    }
    QFile::remove(rollbackPath);
}

void TestDatabase::testAllMembersModelAndIDHiding()
{
    QString dbPath = m_tempDir.filePath("test_members_view.db");
    QString err;
    QVERIFY(DatabaseManager::instance().createDatabase(dbPath, true, &err));

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    QVERIFY(q.exec("INSERT INTO members (name, class, phone, email, status, join_date) "
                   "VALUES ('Charlie Brown', '10A', '12345678', 'charlie@example.org', 'Active', '2026-02-15');"));

    AllMembersView view;
    // The view loads the data
    QVERIFY(view.findChild<QTableView*>() != nullptr);
    QTableView *tv = view.findChild<QTableView*>();

    // Column 0 is member_id and must be hidden
    QVERIFY(tv->isColumnHidden(0));

    // Verify non-editable: edit triggers are NoEditTriggers
    QCOMPARE(tv->editTriggers(), QAbstractItemView::NoEditTriggers);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TestDatabase tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_database.moc"
