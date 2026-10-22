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
