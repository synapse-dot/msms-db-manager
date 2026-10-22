#include <wx/dir.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include <catch2/catch_test_macros.hpp>

#include "db/DbService.h"

TEST_CASE("DbService - Online Backup API (BackupTo)", "[db][backup]") {
    wxString sourceDbPath = wxFileName::CreateTempFileName("msms_test_src_");
    wxString backupDbPath = wxFileName::CreateTempFileName("msms_test_bak_");

    // Clean up temporary files before starting test
    if (wxFileExists(sourceDbPath))
        wxRemoveFile(sourceDbPath);
    if (wxFileExists(backupDbPath))
        wxRemoveFile(backupDbPath);

    msms::DbService db;
    wxString error;

    SECTION("Backup fails when source database is not open") {
        REQUIRE_FALSE(db.BackupTo(backupDbPath, &error));
        REQUIRE(error.Contains("not open"));
    }

    SECTION("Online backup creates an exact, valid copy of the database") {
        REQUIRE(db.CreateNew(sourceDbPath, &error));
        REQUIRE(db.IsOpen());

        // Insert test records
        REQUIRE(db.Execute("INSERT INTO members (name, status, join_date) VALUES ('Alice Tan', "
                           "'Active', '2024-01-15');",
                           &error));
        REQUIRE(db.Execute("INSERT INTO meetings (title, type, meeting_date) VALUES ('Annual "
                           "General Meeting', 'AGM', '2024-03-20');",
                           &error));

        // Perform online backup using BackupTo
        REQUIRE(db.BackupTo(backupDbPath, &error));
        REQUIRE(wxFileExists(backupDbPath));

        // Verify the backup database independently
        msms::DbService backupDb;
        REQUIRE(backupDb.Open(backupDbPath, &error));
        REQUIRE(backupDb.CheckIntegrity(&error));
        REQUIRE(backupDb.CheckCompatibility(&error));

        // Verify records in backup database
        auto stmt = backupDb.Prepare(
            "SELECT name, status, join_date FROM members WHERE name = 'Alice Tan';", &error);
        REQUIRE(stmt != nullptr);
        REQUIRE(stmt->Step());
        REQUIRE(stmt->GetText(0) == "Alice Tan");
        REQUIRE(stmt->GetText(1) == "Active");
        REQUIRE(stmt->GetText(2) == "2024-01-15");
        REQUIRE_FALSE(stmt->Step());

        auto meetStmt =
            backupDb.Prepare("SELECT title, type FROM meetings WHERE type = 'AGM';", &error);
        REQUIRE(meetStmt != nullptr);
        REQUIRE(meetStmt->Step());
        REQUIRE(meetStmt->GetText(0) == "Annual General Meeting");
        REQUIRE(meetStmt->GetText(1) == "AGM");
        REQUIRE_FALSE(meetStmt->Step());

        backupDb.Close();
    }

    // Cleanup
    db.Close();
    if (wxFileExists(sourceDbPath))
        wxRemoveFile(sourceDbPath);
    if (wxFileExists(backupDbPath))
        wxRemoveFile(backupDbPath);
}

TEST_CASE("DbService - Safe Atomic Restore (RestoreFrom)", "[db][restore]") {
    wxString liveDbPath = wxFileName::CreateTempFileName("msms_live_");
    wxString validBackupPath = wxFileName::CreateTempFileName("msms_bak_valid_");
    wxString invalidBackupPath = wxFileName::CreateTempFileName("msms_bak_invalid_");

    if (wxFileExists(liveDbPath))
        wxRemoveFile(liveDbPath);
    if (wxFileExists(validBackupPath))
        wxRemoveFile(validBackupPath);
    if (wxFileExists(invalidBackupPath))
        wxRemoveFile(invalidBackupPath);

    msms::DbService db;
    wxString error;

    // Create live database with initial state (Alice Tan)
    REQUIRE(db.CreateNew(liveDbPath, &error));
    REQUIRE(db.Execute("INSERT INTO members (name, status, join_date) VALUES ('Alice Tan', "
                       "'Active', '2024-01-01');",
                       &error));

    // Create valid backup representing this initial state
    REQUIRE(db.BackupTo(validBackupPath, &error));

    // Now modify live database with newer state (Bob Lim)
    REQUIRE(db.Execute(
        "INSERT INTO members (name, status, join_date) VALUES ('Bob Lim', 'Active', '2024-02-01');",
        &error));

    SECTION("Restore fails when active database is not open") {
        msms::DbService closedDb;
        REQUIRE_FALSE(closedDb.RestoreFrom(validBackupPath, &error));
        REQUIRE(error.Contains("not open"));
    }

    SECTION("Restore fails when backup candidate fails validation") {
        // Create an invalid database (empty file or missing required tables)
        {
            msms::DbService invalidDb;
            if (wxFileExists(invalidBackupPath)) {
                wxRemoveFile(invalidBackupPath);
            }
            // Open without schema
            sqlite3* rawDb = nullptr;
            sqlite3_open_v2(invalidBackupPath.ToUTF8().data(), &rawDb,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
            sqlite3_exec(rawDb, "CREATE TABLE dummy (id INT);", nullptr, nullptr, nullptr);
            sqlite3_close_v2(rawDb);
        }

        // Live database should reject restore and remain unchanged
        REQUIRE_FALSE(db.RestoreFrom(invalidBackupPath, &error));
        REQUIRE(error.Contains("failed compatibility check"));

        // Verify live database still contains Bob Lim
        auto stmt = db.Prepare("SELECT name FROM members WHERE name = 'Bob Lim';", &error);
        REQUIRE(stmt != nullptr);
        REQUIRE(stmt->Step());
        REQUIRE(stmt->GetText(0) == "Bob Lim");
    }

    SECTION("Successful restore replaces database atomically and creates safety copy") {
        REQUIRE(db.RestoreFrom(validBackupPath, &error));
        REQUIRE(db.IsOpen());

        // Verify live database restored to Alice Tan only (Bob Lim is gone)
        auto aliceStmt = db.Prepare("SELECT name FROM members WHERE name = 'Alice Tan';", &error);
        REQUIRE(aliceStmt != nullptr);
        REQUIRE(aliceStmt->Step());
        REQUIRE(aliceStmt->GetText(0) == "Alice Tan");

        auto bobStmt = db.Prepare("SELECT name FROM members WHERE name = 'Bob Lim';", &error);
        REQUIRE(bobStmt != nullptr);
        REQUIRE_FALSE(bobStmt->Step());

        // Verify PRAGMA foreign_keys is ON in restored database
        auto fkStmt = db.Prepare("PRAGMA foreign_keys;", &error);
        REQUIRE(fkStmt != nullptr);
        REQUIRE(fkStmt->Step());
        REQUIRE(fkStmt->GetInt(0) == 1);

        // Verify that timestamped safety copy exists alongside live database
        wxFileName liveFn(liveDbPath);
        liveFn.Normalize(wxPATH_NORM_ABSOLUTE);
        wxString dirPath = liveFn.GetPath();
        wxString filePrefix = liveFn.GetFullName() + ".backup.";

        wxDir dir(dirPath);
        wxString matchedFile;
        bool foundSafetyCopy = false;
        bool hasFile = dir.GetFirst(&matchedFile, filePrefix + "*", wxDIR_FILES);
        while (hasFile) {
            foundSafetyCopy = true;
            // Clean up the created safety copy
            wxRemoveFile(dirPath + "/" + matchedFile);
            hasFile = dir.GetNext(&matchedFile);
        }
        REQUIRE(foundSafetyCopy);
    }

    // Cleanup
    db.Close();
    if (wxFileExists(liveDbPath))
        wxRemoveFile(liveDbPath);
    if (wxFileExists(validBackupPath))
        wxRemoveFile(validBackupPath);
    if (wxFileExists(invalidBackupPath))
        wxRemoveFile(invalidBackupPath);
}
