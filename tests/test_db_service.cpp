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

#include "util/ErrorReporter.h"

TEST_CASE("ErrorReporter - SQLite Error Interception & Foreign Key Diagnosis",
          "[util][error_reporter]") {
    wxString dbPath = wxFileName::CreateTempFileName("msms_err_test_");
    if (wxFileExists(dbPath))
        wxRemoveFile(dbPath);

    msms::DbService db;
    wxString error;
    REQUIRE(db.CreateNew(dbPath, &error));

    // Seed test data
    REQUIRE(db.Execute("INSERT INTO members (member_id, name, status, join_date) VALUES (1, 'Alice "
                       "Tan', 'Active', '2024-01-01');",
                       &error));
    REQUIRE(db.Execute("INSERT INTO members (member_id, name, status, join_date) VALUES (2, 'Bob "
                       "Lim', 'Active', '2024-01-01');",
                       &error));
    REQUIRE(db.Execute("INSERT INTO members (member_id, name, status, join_date) VALUES (3, 'Carol "
                       "Ng', 'Active', '2024-01-01');",
                       &error));
    REQUIRE(db.Execute("INSERT INTO members (member_id, name, status, join_date) VALUES (4, 'Dan "
                       "Wong', 'Active', '2024-01-01');",
                       &error));

    REQUIRE(db.Execute(
        "INSERT INTO meetings (meeting_id, title, type) VALUES (10, 'AGM 2024', 'AGM');", &error));
    REQUIRE(db.Execute("INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (10, 1);",
                       &error));

    REQUIRE(db.Execute(
        "INSERT INTO projects (project_id, title, status) VALUES (20, 'Robotics', 'Planned');",
        &error));
    REQUIRE(db.Execute(
        "INSERT INTO project_members (project_id, member_id, role) VALUES (20, 2, 'Lead');",
        &error));

    REQUIRE(db.Execute("INSERT INTO committee_appointments (start_date, office, member_id) VALUES "
                       "('2024-01-01', 'President', 3);",
                       &error));

    REQUIRE(db.Execute("INSERT INTO external_competitions (competition_id, title, year) VALUES "
                       "(30, 'Math Olympiad', 2024);",
                       &error));
    REQUIRE(db.Execute("INSERT INTO competition_participants (competition_id, member_id, role) "
                       "VALUES (30, 4, 'Competitor');",
                       &error));

    SECTION("meeting_attendance blocks member and meeting deletion") {
        // Attempt deleting Alice Tan (member 1)
        bool deleteMemberOk = db.Execute("DELETE FROM members WHERE member_id = 1;", &error);
        REQUIRE_FALSE(deleteMemberOk);
        msms::ErrorInfo memberErr = msms::ErrorReporter::ReportDeleteError(db, "members", 1);
        REQUIRE(memberErr.rawMessage.Contains("FOREIGN KEY"));
        REQUIRE(memberErr.plainExplanation.Contains("meeting_attendance blocks member deletion"));

        // Attempt deleting Meeting 10
        bool deleteMeetingOk = db.Execute("DELETE FROM meetings WHERE meeting_id = 10;", &error);
        REQUIRE_FALSE(deleteMeetingOk);
        msms::ErrorInfo meetingErr = msms::ErrorReporter::ReportDeleteError(db, "meetings", 10);
        REQUIRE(meetingErr.rawMessage.Contains("FOREIGN KEY"));
        REQUIRE(meetingErr.plainExplanation.Contains("meeting_attendance blocks meeting deletion"));
    }

    SECTION("project_members blocks member and project deletion") {
        // Attempt deleting Bob Lim (member 2)
        bool deleteMemberOk = db.Execute("DELETE FROM members WHERE member_id = 2;", &error);
        REQUIRE_FALSE(deleteMemberOk);
        msms::ErrorInfo memberErr = msms::ErrorReporter::ReportDeleteError(db, "members", 2);
        REQUIRE(memberErr.rawMessage.Contains("FOREIGN KEY"));
        REQUIRE(memberErr.plainExplanation.Contains("project_members blocks member deletion"));

        // Attempt deleting Project 20
        bool deleteProjectOk = db.Execute("DELETE FROM projects WHERE project_id = 20;", &error);
        REQUIRE_FALSE(deleteProjectOk);
        msms::ErrorInfo projectErr = msms::ErrorReporter::ReportDeleteError(db, "projects", 20);
        REQUIRE(projectErr.rawMessage.Contains("FOREIGN KEY"));
        REQUIRE(projectErr.plainExplanation.Contains("project_members blocks project deletion"));
    }

    SECTION("committee_appointments blocks member deletion") {
        // Attempt deleting Carol Ng (member 3)
        bool deleteMemberOk = db.Execute("DELETE FROM members WHERE member_id = 3;", &error);
        REQUIRE_FALSE(deleteMemberOk);
        msms::ErrorInfo memberErr = msms::ErrorReporter::ReportDeleteError(db, "members", 3);
        REQUIRE(memberErr.rawMessage.Contains("FOREIGN KEY"));
        REQUIRE(
            memberErr.plainExplanation.Contains("committee_appointments blocks member deletion"));
    }

    SECTION("competition_participants blocks member and competition deletion") {
        // Attempt deleting Dan Wong (member 4)
        bool deleteMemberOk = db.Execute("DELETE FROM members WHERE member_id = 4;", &error);
        REQUIRE_FALSE(deleteMemberOk);
        msms::ErrorInfo memberErr = msms::ErrorReporter::ReportDeleteError(db, "members", 4);
        REQUIRE(memberErr.rawMessage.Contains("FOREIGN KEY"));
        REQUIRE(
            memberErr.plainExplanation.Contains("competition_participants blocks member deletion"));

        // Attempt deleting Competition 30
        bool deleteCompOk =
            db.Execute("DELETE FROM external_competitions WHERE competition_id = 30;", &error);
        REQUIRE_FALSE(deleteCompOk);
        msms::ErrorInfo compErr =
            msms::ErrorReporter::ReportDeleteError(db, "external_competitions", 30);
        REQUIRE(compErr.rawMessage.Contains("FOREIGN KEY"));
        REQUIRE(compErr.plainExplanation.Contains(
            "competition_participants blocks competition deletion"));
    }

    SECTION("Unique constraint violation interception") {
        // Duplicate (title, year) in external_competitions
        bool dupOk = db.Execute(
            "INSERT INTO external_competitions (title, year) VALUES ('Math Olympiad', 2024);",
            &error);
        REQUIRE_FALSE(dupOk);
        msms::ErrorInfo err = msms::ErrorReporter::Report(db);
        REQUIRE(err.rawMessage.Contains("UNIQUE"));
        REQUIRE(
            err.plainExplanation.Contains("A competition with this title and year already exists"));
    }

    SECTION("Check constraint violation interception") {
        // Invalid meeting start/end time order (end before start)
        bool checkOk = db.Execute("INSERT INTO meetings (title, type, start_time, end_time) VALUES "
                                  "('EGM', 'EGM', '15:00', '14:00');",
                                  &error);
        REQUIRE_FALSE(checkOk);
        msms::ErrorInfo err = msms::ErrorReporter::Report(db);
        REQUIRE(err.rawMessage.Contains("CHECK"));
        REQUIRE(err.plainExplanation.Contains("violate the required constraints"));
    }

    // Cleanup
    db.Close();
    if (wxFileExists(dbPath))
        wxRemoveFile(dbPath);
}
