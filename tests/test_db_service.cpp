#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include <catch2/catch_test_macros.hpp>

#include "db/DbService.h"
#include "util/ErrorReporter.h"

namespace {

#ifndef MSMS_PROJECT_ROOT
#define MSMS_PROJECT_ROOT "."
#endif

wxString GetSqlFilePath(const wxString& relativePath) {
    wxString full = wxString::Format("%s/%s", MSMS_PROJECT_ROOT, relativePath);
    wxFileName fn(full);
    fn.Normalize(wxPATH_NORM_ABSOLUTE);
    return fn.GetFullPath();
}

wxString ReadSqlFile(const wxString& relativePath) {
    wxString fullPath = GetSqlFilePath(relativePath);
    wxFile file(fullPath, wxFile::read);
    REQUIRE(file.IsOpened());
    wxString content;
    file.ReadAll(&content);
    return content;
}

int64_t GetRowCount(msms::DbService& db, const wxString& tableName) {
    wxString error;
    auto stmt = db.Prepare(wxString::Format("SELECT COUNT(*) FROM %s;", tableName), &error);
    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->Step());
    return stmt->GetInt64(0);
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Verify PRAGMA foreign_keys = ON is always enabled
// ---------------------------------------------------------------------------
TEST_CASE("1. Verify PRAGMA foreign_keys = ON is always enabled", "[db][foreign_keys]") {
    wxString dbPath = wxFileName::CreateTempFileName("msms_fk_test_");
    if (wxFileExists(dbPath)) {
        wxRemoveFile(dbPath);
    }

    msms::DbService db;
    wxString error;

    SECTION("New database has foreign keys enabled and verified") {
        REQUIRE(db.CreateNew(dbPath, &error));
        REQUIRE(db.IsOpen());

        auto stmt = db.Prepare("PRAGMA foreign_keys;", &error);
        REQUIRE(stmt != nullptr);
        REQUIRE(stmt->Step());
        REQUIRE(stmt->GetInt(0) == 1);
        db.Close();
    }

    SECTION("Existing database has foreign keys enabled upon opening") {
        REQUIRE(db.CreateNew(dbPath, &error));
        db.Close();

        REQUIRE(db.Open(dbPath, &error));
        REQUIRE(db.IsOpen());

        auto stmt = db.Prepare("PRAGMA foreign_keys;", &error);
        REQUIRE(stmt != nullptr);
        REQUIRE(stmt->Step());
        REQUIRE(stmt->GetInt(0) == 1);

        // Verify foreign key enforcement rejects orphaned child insertions
        bool orphanOk = db.Execute(
            "INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (999, 999);", &error);
        REQUIRE_FALSE(orphanOk);
        msms::ErrorInfo err = msms::ErrorReporter::Report(db);
        REQUIRE(err.rawMessage.Contains("FOREIGN KEY"));
        db.Close();
    }

    if (wxFileExists(dbPath)) {
        wxRemoveFile(dbPath);
    }
}

// ---------------------------------------------------------------------------
// 2. Verify table schema compatibility checks accept valid and reject altered/incomplete
// ---------------------------------------------------------------------------
TEST_CASE("2. Verify table schema compatibility checks accept valid and reject altered/incomplete "
          "schemas",
          "[db][compatibility]") {
    wxString validDbPath = wxFileName::CreateTempFileName("msms_valid_compat_");
    wxString missingTableDbPath = wxFileName::CreateTempFileName("msms_missing_table_");
    wxString missingColumnDbPath = wxFileName::CreateTempFileName("msms_missing_column_");

    if (wxFileExists(validDbPath))
        wxRemoveFile(validDbPath);
    if (wxFileExists(missingTableDbPath))
        wxRemoveFile(missingTableDbPath);
    if (wxFileExists(missingColumnDbPath))
        wxRemoveFile(missingColumnDbPath);

    wxString error;

    SECTION("Valid database created from schema is accepted") {
        msms::DbService db;
        REQUIRE(db.CreateNew(validDbPath, &error));
        REQUIRE(db.CheckCompatibility(&error));
        REQUIRE(db.CheckIntegrity(&error));
        db.Close();
    }

    SECTION("Database missing a required table is rejected") {
        // Create database omitting 'meeting_attendance'
        sqlite3* rawDb = nullptr;
        sqlite3_open_v2(missingTableDbPath.ToUTF8().data(), &rawDb,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
        sqlite3_exec(rawDb,
                     "CREATE TABLE members (member_id INTEGER PRIMARY KEY, name TEXT, class TEXT, "
                     "phone TEXT, email TEXT, status TEXT, join_date TEXT);",
                     nullptr, nullptr, nullptr);
        sqlite3_close_v2(rawDb);

        msms::DbService db;
        REQUIRE(db.Open(missingTableDbPath, &error));
        bool compat = db.CheckCompatibility(&error);
        REQUIRE_FALSE(compat);
        REQUIRE(error.Contains("Missing table"));
        db.Close();
    }

    SECTION("Database missing a required column is rejected") {
        // Create database where members table lacks 'join_date'
        sqlite3* rawDb = nullptr;
        sqlite3_open_v2(missingColumnDbPath.ToUTF8().data(), &rawDb,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
        sqlite3_exec(rawDb, msms::DbService::GetEmbeddedSchemaSql(), nullptr, nullptr, nullptr);
        // Alter table by dropping column (SQLite 3.35+)
        sqlite3_exec(rawDb, "ALTER TABLE members DROP COLUMN join_date;", nullptr, nullptr,
                     nullptr);
        sqlite3_close_v2(rawDb);

        msms::DbService db;
        REQUIRE(db.Open(missingColumnDbPath, &error));
        bool compat = db.CheckCompatibility(&error);
        REQUIRE_FALSE(compat);
        REQUIRE(error.Contains("is missing column 'join_date'"));
        db.Close();
    }

    if (wxFileExists(validDbPath))
        wxRemoveFile(validDbPath);
    if (wxFileExists(missingTableDbPath))
        wxRemoveFile(missingTableDbPath);
    if (wxFileExists(missingColumnDbPath))
        wxRemoveFile(missingColumnDbPath);
}

// ---------------------------------------------------------------------------
// 3. Verify database creation from schema.sql and seeding from seed.sql
// ---------------------------------------------------------------------------
TEST_CASE("3. Verify database creation from schema.sql and seeding from seed.sql",
          "[db][schema][seed]") {
    wxString dbPath = wxFileName::CreateTempFileName("msms_seed_test_");
    if (wxFileExists(dbPath))
        wxRemoveFile(dbPath);

    msms::DbService db;
    wxString error;

    // Create database initializing from schema.sql
    REQUIRE(db.CreateNew(dbPath, &error));
    REQUIRE(db.IsOpen());
    REQUIRE(db.CheckIntegrity(&error));
    REQUIRE(db.CheckCompatibility(&error));

    // Read seed.sql and apply to database
    wxString seedSql = ReadSqlFile("database/seed.sql");
    REQUIRE(!seedSql.IsEmpty());
    REQUIRE(db.Execute(seedSql, &error));

    // Verify row counts across all 9 tables per seed.sql
    CHECK(GetRowCount(db, "members") == 10);
    CHECK(GetRowCount(db, "meetings") == 5);
    CHECK(GetRowCount(db, "meeting_attendance") == 28);
    CHECK(GetRowCount(db, "projects") == 5);
    CHECK(GetRowCount(db, "project_members") == 15);
    CHECK(GetRowCount(db, "committee_appointments") == 6);
    CHECK(GetRowCount(db, "activities") == 6);
    CHECK(GetRowCount(db, "external_competitions") == 4);
    CHECK(GetRowCount(db, "competition_participants") == 10);

    // Verify integrity and compatibility on seeded database
    REQUIRE(db.CheckIntegrity(&error));
    REQUIRE(db.CheckCompatibility(&error));

    db.Close();
    if (wxFileExists(dbPath))
        wxRemoveFile(dbPath);
}

// ---------------------------------------------------------------------------
// 4. Verify SQLite online backup produces a bit-for-bit coherent, valid database
// ---------------------------------------------------------------------------
TEST_CASE("4. Verify SQLite online backup produces a bit-for-bit coherent, valid database",
          "[db][backup]") {
    wxString sourceDbPath = wxFileName::CreateTempFileName("msms_src_backup_");
    wxString backupDbPath = wxFileName::CreateTempFileName("msms_dst_backup_");

    if (wxFileExists(sourceDbPath))
        wxRemoveFile(sourceDbPath);
    if (wxFileExists(backupDbPath))
        wxRemoveFile(backupDbPath);

    msms::DbService srcDb;
    wxString error;
    REQUIRE(srcDb.CreateNew(sourceDbPath, &error));

    // Seed data into source database
    wxString seedSql = ReadSqlFile("database/seed.sql");
    REQUIRE(srcDb.Execute(seedSql, &error));

    // The service must never treat the active database as its own backup destination.
    REQUIRE_FALSE(srcDb.BackupTo(sourceDbPath, &error));
    CHECK(error.Contains("different"));
    CHECK(GetRowCount(srcDb, "members") == 10);
    error.Clear();

    // Perform online backup using BackupTo
    REQUIRE(srcDb.BackupTo(backupDbPath, &error));
    REQUIRE(wxFileExists(backupDbPath));

    // Open backup database independently
    msms::DbService bakDb;
    REQUIRE(bakDb.Open(backupDbPath, &error));
    REQUIRE(bakDb.CheckIntegrity(&error));
    REQUIRE(bakDb.CheckCompatibility(&error));

    // Verify exact record counts match across all 9 tables
    CHECK(GetRowCount(bakDb, "members") == GetRowCount(srcDb, "members"));
    CHECK(GetRowCount(bakDb, "meetings") == GetRowCount(srcDb, "meetings"));
    CHECK(GetRowCount(bakDb, "meeting_attendance") == GetRowCount(srcDb, "meeting_attendance"));
    CHECK(GetRowCount(bakDb, "projects") == GetRowCount(srcDb, "projects"));
    CHECK(GetRowCount(bakDb, "project_members") == GetRowCount(srcDb, "project_members"));
    CHECK(GetRowCount(bakDb, "committee_appointments") ==
          GetRowCount(srcDb, "committee_appointments"));
    CHECK(GetRowCount(bakDb, "activities") == GetRowCount(srcDb, "activities"));
    CHECK(GetRowCount(bakDb, "external_competitions") ==
          GetRowCount(srcDb, "external_competitions"));
    CHECK(GetRowCount(bakDb, "competition_participants") ==
          GetRowCount(srcDb, "competition_participants"));

    // Verify specific data coherence
    auto stmt =
        bakDb.Prepare("SELECT name, email, status FROM members WHERE member_id = 1;", &error);
    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->Step());
    CHECK(stmt->GetText(0) == "Alex Morgan");
    CHECK(stmt->GetText(1) == "alex.morgan@example.org");
    CHECK(stmt->GetText(2) == "Active");

    srcDb.Close();
    bakDb.Close();

    if (wxFileExists(sourceDbPath))
        wxRemoveFile(sourceDbPath);
    if (wxFileExists(backupDbPath))
        wxRemoveFile(backupDbPath);
}


// ---------------------------------------------------------------------------
// 4b. BackupTo must reject copies that fail the application schema validation
// ---------------------------------------------------------------------------
TEST_CASE("BackupTo only succeeds after verifying the copied database",
          "[db][backup][verification]") {
    wxString sourceDbPath = wxFileName::CreateTempFileName("msms_invalid_src_backup_");
    wxString backupDbPath = wxFileName::CreateTempFileName("msms_invalid_dst_backup_");

    if (wxFileExists(sourceDbPath))
        wxRemoveFile(sourceDbPath);
    if (wxFileExists(backupDbPath))
        wxRemoveFile(backupDbPath);

    // Seed a valid destination backup so a failed attempt must not destroy it.
    {
        msms::DbService existingBackup;
        wxString setupError;
        REQUIRE(existingBackup.CreateNew(backupDbPath, &setupError));
        REQUIRE(existingBackup.Execute(ReadSqlFile("database/seed.sql"), &setupError));
        REQUIRE(existingBackup.CheckIntegrity(&setupError));
        REQUIRE(existingBackup.CheckCompatibility(&setupError));
        existingBackup.Close();
    }

    // Create a valid SQLite file with the wrong application schema. Opening it
    // is permitted; BackupTo must reject the resulting copy at verification.
    sqlite3* rawDb = nullptr;
    REQUIRE(sqlite3_open_v2(sourceDbPath.ToUTF8().data(), &rawDb,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_exec(rawDb, "CREATE TABLE unrelated (id INTEGER PRIMARY KEY);",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_close_v2(rawDb) == SQLITE_OK);

    msms::DbService sourceDb;
    wxString error;
    REQUIRE(sourceDb.Open(sourceDbPath, &error));

    CHECK_FALSE(sourceDb.BackupTo(backupDbPath, &error));
    CHECK(error.Contains("schema check"));

    // The rejected copy must not replace the previously usable backup.
    msms::DbService preservedBackup;
    REQUIRE(preservedBackup.Open(backupDbPath, &error));
    REQUIRE(preservedBackup.CheckIntegrity(&error));
    REQUIRE(preservedBackup.CheckCompatibility(&error));
    CHECK(GetRowCount(preservedBackup, "members") == 10);
    preservedBackup.Close();

    sourceDb.Close();
    if (wxFileExists(sourceDbPath))
        wxRemoveFile(sourceDbPath);
    if (wxFileExists(backupDbPath))
        wxRemoveFile(backupDbPath);
}

// ---------------------------------------------------------------------------
// 5. Verify atomic restore safety copy creation and rollback behavior on corrupted input files
// ---------------------------------------------------------------------------
TEST_CASE(
    "5. Verify atomic restore safety copy creation and rollback behavior on corrupted input files",
    "[db][restore]") {
    wxString workingDbPath = wxFileName::CreateTempFileName("msms_working_restore_");
    wxString validBackupPath = wxFileName::CreateTempFileName("msms_valid_bak_");
    wxString corruptedBackupPath = wxFileName::CreateTempFileName("msms_corrupted_bak_");
    wxString incompatibleBackupPath = wxFileName::CreateTempFileName("msms_incompatible_bak_");

    if (wxFileExists(workingDbPath))
        wxRemoveFile(workingDbPath);
    if (wxFileExists(validBackupPath))
        wxRemoveFile(validBackupPath);
    if (wxFileExists(corruptedBackupPath))
        wxRemoveFile(corruptedBackupPath);
    if (wxFileExists(incompatibleBackupPath))
        wxRemoveFile(incompatibleBackupPath);

    msms::DbService db;
    wxString error;
    REQUIRE(db.CreateNew(workingDbPath, &error));

    // Seed initial state into working db and create valid backup
    wxString seedSql = ReadSqlFile("database/seed.sql");
    REQUIRE(db.Execute(seedSql, &error));
    REQUIRE(db.BackupTo(validBackupPath, &error));

    // Create a corrupted backup candidate file.
    {
        wxFile badFile(corruptedBackupPath, wxFile::write);
        badFile.Write("THIS IS NOT A VALID SQLITE DATABASE FILE HEADER!!");
        badFile.Close();
    }

    // Create a structurally valid SQLite file with an incompatible application schema.
    {
        sqlite3* rawDb = nullptr;
        REQUIRE(sqlite3_open_v2(incompatibleBackupPath.ToUTF8().data(), &rawDb,
                                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_exec(rawDb, "CREATE TABLE unrelated (id INTEGER PRIMARY KEY);",
                             nullptr, nullptr, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_close_v2(rawDb) == SQLITE_OK);
    }

    SECTION("Rollback behavior on corrupted input file preserves working database") {
        // Attempt restore with corrupted file
        bool restoreOk = db.RestoreFrom(corruptedBackupPath, &error);
        REQUIRE_FALSE(restoreOk);
        REQUIRE(db.IsOpen());

        // Working database must remain completely intact with all 10 members
        CHECK(GetRowCount(db, "members") == 10);
        REQUIRE(db.CheckIntegrity(&error));
        REQUIRE(db.CheckCompatibility(&error));

        // No staging files should be left behind after the rejected candidate.
        wxFileName workFn(workingDbPath);
        workFn.Normalize(wxPATH_NORM_ABSOLUTE);
        wxDir workDir(workFn.GetPath());
        wxString matchedFile;
        const wxString restoreTempPattern = "." + workFn.GetFullName() + ".restore.tmp.*";
        CHECK_FALSE(workDir.GetFirst(&matchedFile, restoreTempPattern, wxDIR_FILES));
    }

    SECTION("Restoring from the active database is rejected without changes") {
        CHECK_FALSE(db.RestoreFrom(workingDbPath, &error));
        CHECK(error.Contains("currently open database"));
        REQUIRE(db.IsOpen());
        CHECK(GetRowCount(db, "members") == 10);
        REQUIRE(db.CheckIntegrity(&error));
        REQUIRE(db.CheckCompatibility(&error));
    }

    SECTION("An incompatible candidate is rejected before making a safety copy") {
        CHECK_FALSE(db.RestoreFrom(incompatibleBackupPath, &error));
        CHECK(error.Contains("valid MSMS database"));
        REQUIRE(db.IsOpen());
        CHECK(GetRowCount(db, "members") == 10);

        wxFileName workFn(workingDbPath);
        workFn.Normalize(wxPATH_NORM_ABSOLUTE);
        wxDir workDir(workFn.GetPath());
        wxString matchedFile;
        const wxString safetyPattern = workFn.GetFullName() + ".backup.*";
        CHECK_FALSE(workDir.GetFirst(&matchedFile, safetyPattern, wxDIR_FILES));
    }

    SECTION("Restore is refused while another DbService connection holds the database open") {
        msms::DbService otherInstance;
        REQUIRE(otherInstance.Open(workingDbPath, &error));
        REQUIRE(otherInstance.IsOpen());

        CHECK_FALSE(db.RestoreFrom(validBackupPath, &error));
        CHECK(error.Contains("another MSMS Manager instance"));
        REQUIRE(db.IsOpen());
        REQUIRE(otherInstance.IsOpen());
        CHECK(GetRowCount(db, "members") == 10);
        CHECK(GetRowCount(otherInstance, "members") == 10);

        wxFileName workFn(workingDbPath);
        workFn.Normalize(wxPATH_NORM_ABSOLUTE);
        wxDir workDir(workFn.GetPath());
        wxString matchedFile;
        const wxString safetyPattern = workFn.GetFullName() + ".backup.*";
        CHECK_FALSE(workDir.GetFirst(&matchedFile, safetyPattern, wxDIR_FILES));

        otherInstance.Close();
        REQUIRE(db.RestoreFrom(validBackupPath, &error));
        REQUIRE(db.IsOpen());
        CHECK(GetRowCount(db, "members") == 10);
    }

    SECTION("Successful restore creates timestamped safety copy and recovers state") {
        // Mutate working database: add a new member and delete one activity
        REQUIRE(db.Execute("INSERT INTO members (name, status, join_date) VALUES ('Temporary "
                           "User', 'Active', '2026-05-01');",
                           &error));
        REQUIRE(db.Execute("DELETE FROM activities WHERE activity_id = 6;", &error));
        CHECK(GetRowCount(db, "members") == 11);
        CHECK(GetRowCount(db, "activities") == 5);

        // Perform restore from valid backup (which had 10 members, 6 activities)
        REQUIRE(db.RestoreFrom(validBackupPath, &error));
        REQUIRE(db.IsOpen());

        // Working database state is restored
        CHECK(GetRowCount(db, "members") == 10);
        CHECK(GetRowCount(db, "activities") == 6);

        // Verify foreign keys are ON
        auto fkStmt = db.Prepare("PRAGMA foreign_keys;", &error);
        REQUIRE(fkStmt != nullptr);
        REQUIRE(fkStmt->Step());
        CHECK(fkStmt->GetInt(0) == 1);

        // Verify safety copy was created alongside working database
        wxFileName workFn(workingDbPath);
        workFn.Normalize(wxPATH_NORM_ABSOLUTE);
        wxDir dir(workFn.GetPath());
        wxString matchedFile;
        wxString pattern = workFn.GetFullName() + ".backup.*";
        bool foundSafetyCopy = false;

        bool hasFile = dir.GetFirst(&matchedFile, pattern, wxDIR_FILES);
        while (hasFile) {
            foundSafetyCopy = true;
            // The safety copy should contain the mutated state (11 members)
            msms::DbService safetyDb;
            wxString safetyFullPath = workFn.GetPathWithSep() + matchedFile;
            if (safetyDb.Open(safetyFullPath, &error)) {
                CHECK(GetRowCount(safetyDb, "members") == 11);
                safetyDb.Close();
            }
            wxRemoveFile(safetyFullPath);
            hasFile = dir.GetNext(&matchedFile);
        }
        CHECK(foundSafetyCopy);
    }

    db.Close();
    if (wxFileExists(workingDbPath))
        wxRemoveFile(workingDbPath);
    if (wxFileExists(validBackupPath))
        wxRemoveFile(validBackupPath);
    if (wxFileExists(corruptedBackupPath))
        wxRemoveFile(corruptedBackupPath);
    if (wxFileExists(incompatibleBackupPath))
        wxRemoveFile(incompatibleBackupPath);
}

// ---------------------------------------------------------------------------
// 6. Verify foreign-key RESTRICT prevention on deletion of referenced records
// ---------------------------------------------------------------------------
TEST_CASE("6. Verify foreign-key RESTRICT prevention on deletion of referenced records",
          "[db][restrict]") {
    wxString dbPath = wxFileName::CreateTempFileName("msms_restrict_test_");
    if (wxFileExists(dbPath))
        wxRemoveFile(dbPath);

    msms::DbService db;
    wxString error;
    REQUIRE(db.CreateNew(dbPath, &error));

    // Seed database
    wxString seedSql = ReadSqlFile("database/seed.sql");
    REQUIRE(db.Execute(seedSql, &error));

    SECTION("meeting_attendance blocks member or meeting deletion") {
        // Member 1 has attendance records in meeting 1, 2, 3, 4, 5
        bool deleteMemberOk = db.Execute("DELETE FROM members WHERE member_id = 1;", &error);
        REQUIRE_FALSE(deleteMemberOk);
        msms::ErrorInfo memberErr = msms::ErrorReporter::ReportDeleteError(db, "members", 1);
        CHECK(memberErr.rawMessage.Contains("FOREIGN KEY"));
        CHECK(memberErr.plainExplanation.Contains("meeting_attendance blocks member deletion"));

        // Meeting 1 has attendees (members 1, 2, 3, 4, 5, 6, 8, 9)
        bool deleteMeetingOk = db.Execute("DELETE FROM meetings WHERE meeting_id = 1;", &error);
        REQUIRE_FALSE(deleteMeetingOk);
        msms::ErrorInfo meetingErr = msms::ErrorReporter::ReportDeleteError(db, "meetings", 1);
        CHECK(meetingErr.rawMessage.Contains("FOREIGN KEY"));
        CHECK(meetingErr.plainExplanation.Contains("meeting_attendance blocks meeting deletion"));
    }

    SECTION("project_members blocks member or project deletion") {
        // Project 1 has team members (1, 2, 3, 6)
        bool deleteProjectOk = db.Execute("DELETE FROM projects WHERE project_id = 1;", &error);
        REQUIRE_FALSE(deleteProjectOk);
        msms::ErrorInfo projectErr = msms::ErrorReporter::ReportDeleteError(db, "projects", 1);
        CHECK(projectErr.rawMessage.Contains("FOREIGN KEY"));
        CHECK(projectErr.plainExplanation.Contains("project_members blocks project deletion"));

        // Member 2 is in project 1 and project 3
        bool deleteMemberOk = db.Execute("DELETE FROM members WHERE member_id = 2;", &error);
        REQUIRE_FALSE(deleteMemberOk);
        msms::ErrorInfo memberErr = msms::ErrorReporter::ReportDeleteError(db, "members", 2);
        CHECK(memberErr.rawMessage.Contains("FOREIGN KEY"));
        // Member 2 is in attendance first, or in project members
        CHECK(memberErr.plainExplanation.Contains("blocks member deletion"));
    }

    SECTION("committee_appointments blocks member deletion") {
        // Member 7 ('Morgan Ellis') holds appointment 'President' from 2025-01-10 to 2026-01-14
        // Member 7 has no attendance records and no project assignments
        bool deleteMemberOk = db.Execute("DELETE FROM members WHERE member_id = 7;", &error);
        REQUIRE_FALSE(deleteMemberOk);
        msms::ErrorInfo memberErr = msms::ErrorReporter::ReportDeleteError(db, "members", 7);
        CHECK(memberErr.rawMessage.Contains("FOREIGN KEY"));
        CHECK(memberErr.plainExplanation.Contains("blocks member deletion"));
    }

    SECTION("competition_participants blocks member or competition deletion") {
        // Competition 1 ('National Science Competition') has participants (1, 3, 9)
        bool deleteCompOk =
            db.Execute("DELETE FROM external_competitions WHERE competition_id = 1;", &error);
        REQUIRE_FALSE(deleteCompOk);
        msms::ErrorInfo compErr =
            msms::ErrorReporter::ReportDeleteError(db, "external_competitions", 1);
        CHECK(compErr.rawMessage.Contains("FOREIGN KEY"));
        CHECK(compErr.plainExplanation.Contains(
            "competition_participants blocks competition deletion"));
    }

    db.Close();
    if (wxFileExists(dbPath))
        wxRemoveFile(dbPath);
}
