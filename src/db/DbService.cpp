#include "DbService.h"

#include <wx/filefn.h>
#include <wx/filename.h>

namespace msms {

// ---------------------------------------------------------------------------
// Statement Implementation
// ---------------------------------------------------------------------------

Statement::Statement(sqlite3_stmt* stmt) : m_stmt(stmt) {}

Statement::~Statement() {
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
        m_stmt = nullptr;
    }
}

Statement::Statement(Statement&& other) noexcept : m_stmt(other.m_stmt) {
    other.m_stmt = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
        }
        m_stmt = other.m_stmt;
        other.m_stmt = nullptr;
    }
    return *this;
}

bool Statement::BindInt(int index, int val) {
    if (!m_stmt)
        return false;
    return sqlite3_bind_int(m_stmt, index, val) == SQLITE_OK;
}

bool Statement::BindInt64(int index, int64_t val) {
    if (!m_stmt)
        return false;
    return sqlite3_bind_int64(m_stmt, index, static_cast<sqlite3_int64>(val)) == SQLITE_OK;
}

bool Statement::BindDouble(int index, double val) {
    if (!m_stmt)
        return false;
    return sqlite3_bind_double(m_stmt, index, val) == SQLITE_OK;
}

bool Statement::BindText(int index, const wxString& val) {
    if (!m_stmt)
        return false;
    const wxScopedCharBuffer utf8 = val.ToUTF8();
    return sqlite3_bind_text(m_stmt, index, utf8.data(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool Statement::BindNull(int index) {
    if (!m_stmt)
        return false;
    return sqlite3_bind_null(m_stmt, index) == SQLITE_OK;
}

bool Statement::Step() {
    if (!m_stmt)
        return false;
    int rc = sqlite3_step(m_stmt);
    return rc == SQLITE_ROW;
}

bool Statement::Reset() {
    if (!m_stmt)
        return false;
    return sqlite3_reset(m_stmt) == SQLITE_OK;
}

bool Statement::IsNull(int col) const {
    if (!m_stmt)
        return true;
    return sqlite3_column_type(m_stmt, col) == SQLITE_NULL;
}

int Statement::GetInt(int col) const {
    if (!m_stmt)
        return 0;
    return sqlite3_column_int(m_stmt, col);
}

int64_t Statement::GetInt64(int col) const {
    if (!m_stmt)
        return 0;
    return static_cast<int64_t>(sqlite3_column_int64(m_stmt, col));
}

double Statement::GetDouble(int col) const {
    if (!m_stmt)
        return 0.0;
    return sqlite3_column_double(m_stmt, col);
}

wxString Statement::GetText(int col) const {
    if (!m_stmt || IsNull(col))
        return wxString();
    const auto* txt = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, col));
    return txt ? wxString::FromUTF8(txt) : wxString();
}

int Statement::GetColumnCount() const {
    if (!m_stmt)
        return 0;
    return sqlite3_column_count(m_stmt);
}

wxString Statement::GetColumnName(int col) const {
    if (!m_stmt)
        return wxString();
    const char* name = sqlite3_column_name(m_stmt, col);
    return name ? wxString::FromUTF8(name) : wxString();
}

// ---------------------------------------------------------------------------
// DbService Implementation
// ---------------------------------------------------------------------------

DbService::DbService() = default;

DbService::~DbService() {
    Close();
}

void DbService::Close() {
    if (m_db) {
        if (m_inTransaction) {
            Rollback(nullptr);
        }
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
    m_dbPath.Clear();
    m_inTransaction = false;
}

bool DbService::Open(const wxString& dbPath, wxString* outError) {
    Close();

    if (!wxFileExists(dbPath)) {
        if (outError) {
            *outError = wxString::Format("Database file does not exist: %s", dbPath);
        }
        return false;
    }

    const wxScopedCharBuffer utf8Path = dbPath.ToUTF8();
    int rc = sqlite3_open_v2(utf8Path.data(), &m_db, SQLITE_OPEN_READWRITE, nullptr);
    if (rc != SQLITE_OK || !m_db) {
        if (outError) {
            *outError = m_db ? wxString::FromUTF8(sqlite3_errmsg(m_db))
                             : wxString::Format("Failed to open database (SQLite code: %d)", rc);
        }
        Close();
        return false;
    }

    m_dbPath = dbPath;

    if (!EnableForeignKeys(outError) || !VerifyForeignKeys(outError)) {
        Close();
        return false;
    }

    return true;
}

bool DbService::CreateNew(const wxString& dbPath, wxString* outError) {
    Close();

    const wxScopedCharBuffer utf8Path = dbPath.ToUTF8();
    int rc = sqlite3_open_v2(utf8Path.data(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             nullptr);
    if (rc != SQLITE_OK || !m_db) {
        if (outError) {
            *outError = m_db ? wxString::FromUTF8(sqlite3_errmsg(m_db))
                             : wxString::Format("Failed to create database (SQLite code: %d)", rc);
        }
        Close();
        return false;
    }

    m_dbPath = dbPath;

    if (!EnableForeignKeys(outError) || !VerifyForeignKeys(outError)) {
        Close();
        return false;
    }

    if (!InitializeSchema(outError)) {
        Close();
        return false;
    }

    return true;
}

bool DbService::EnableForeignKeys(wxString* outError) {
    return Execute("PRAGMA foreign_keys = ON;", outError);
}

bool DbService::VerifyForeignKeys(wxString* outError) {
    auto stmt = Prepare("PRAGMA foreign_keys;", outError);
    if (!stmt)
        return false;

    if (stmt->Step()) {
        int fkStatus = stmt->GetInt(0);
        if (fkStatus == 1) {
            return true;
        }
    }

    if (outError) {
        *outError =
            "Foreign key enforcement could not be confirmed (PRAGMA foreign_keys returned 0).";
    }
    return false;
}

bool DbService::InitializeSchema(wxString* outError) {
    return Execute(GetEmbeddedSchemaSql(), outError);
}

bool DbService::CheckCompatibility(wxString* outError) const {
    if (!IsOpen()) {
        if (outError)
            *outError = "Database is not open.";
        return false;
    }

    // 1. Check all expected tables exist
    const auto& required = GetRequiredSchema();
    std::set<std::string> existingTables;

    auto stmt = Prepare("SELECT name FROM sqlite_master WHERE type = 'table';", outError);
    if (!stmt)
        return false;

    while (stmt->Step()) {
        existingTables.insert(stmt->GetText(0).ToStdString());
    }

    for (const auto& [tableName, columns] : required) {
        if (existingTables.find(tableName) == existingTables.end()) {
            if (outError) {
                *outError = wxString::Format("Compatibility check failed: Missing table '%s'",
                                             wxString::FromUTF8(tableName.c_str()));
            }
            return false;
        }

        // 2. Check each column exists in the table
        wxString tableInfoSql = wxString::Format("PRAGMA table_info(%s);", tableName);
        auto colStmt = Prepare(tableInfoSql, outError);
        if (!colStmt)
            return false;

        std::set<std::string> existingColumns;
        while (colStmt->Step()) {
            // column 1 in PRAGMA table_info is column name
            existingColumns.insert(colStmt->GetText(1).ToStdString());
        }

        for (const auto& colName : columns) {
            if (existingColumns.find(colName) == existingColumns.end()) {
                if (outError) {
                    *outError = wxString::Format(
                        "Compatibility check failed: Table '%s' is missing column '%s'",
                        wxString::FromUTF8(tableName.c_str()), wxString::FromUTF8(colName.c_str()));
                }
                return false;
            }
        }
    }

    return true;
}

bool DbService::CheckIntegrity(wxString* outError) const {
    if (!IsOpen()) {
        if (outError)
            *outError = "Database is not open.";
        return false;
    }

    auto stmt = Prepare("PRAGMA integrity_check;", outError);
    if (!stmt)
        return false;

    if (stmt->Step()) {
        wxString result = stmt->GetText(0);
        if (result.CmpNoCase("ok") == 0) {
            return true;
        }
        if (outError) {
            *outError = wxString::Format("Database integrity check failed: %s", result);
        }
        return false;
    }

    if (outError) {
        *outError = "PRAGMA integrity_check returned no results.";
    }
    return false;
}

std::unique_ptr<Statement> DbService::Prepare(const wxString& sql, wxString* outError) const {
    if (!IsOpen()) {
        if (outError)
            *outError = "Database is not open.";
        return nullptr;
    }

    sqlite3_stmt* stmt = nullptr;
    const wxScopedCharBuffer utf8 = sql.ToUTF8();
    int rc = sqlite3_prepare_v2(m_db, utf8.data(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK || !stmt) {
        if (outError) {
            *outError = GetLastErrorMessage();
        }
        return nullptr;
    }

    return std::make_unique<Statement>(stmt);
}

bool DbService::Execute(const wxString& sql, wxString* outError) {
    if (!IsOpen()) {
        if (outError)
            *outError = "Database is not open.";
        return false;
    }

    char* errmsg = nullptr;
    const wxScopedCharBuffer utf8 = sql.ToUTF8();
    int rc = sqlite3_exec(m_db, utf8.data(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        if (outError) {
            *outError = errmsg ? wxString::FromUTF8(errmsg) : GetLastErrorMessage();
        }
        if (errmsg) {
            sqlite3_free(errmsg);
        }
        return false;
    }

    return true;
}

bool DbService::Begin(wxString* outError) {
    if (m_inTransaction) {
        if (outError)
            *outError = "Transaction already active.";
        return false;
    }
    bool ok = Execute("BEGIN IMMEDIATE;", outError);
    if (ok) {
        m_inTransaction = true;
    }
    return ok;
}

bool DbService::Commit(wxString* outError) {
    if (!m_inTransaction) {
        if (outError)
            *outError = "No active transaction to commit.";
        return false;
    }
    bool ok = Execute("COMMIT;", outError);
    m_inTransaction = false;
    return ok;
}

bool DbService::Rollback(wxString* outError) {
    if (!m_inTransaction) {
        if (outError)
            *outError = "No active transaction to rollback.";
        return false;
    }
    bool ok = Execute("ROLLBACK;", outError);
    m_inTransaction = false;
    return ok;
}

int DbService::GetLastErrorCode() const {
    return m_db ? sqlite3_errcode(m_db) : SQLITE_OK;
}

wxString DbService::GetLastErrorMessage() const {
    return m_db ? wxString::FromUTF8(sqlite3_errmsg(m_db)) : wxString();
}

int64_t DbService::GetLastInsertRowId() const {
    return m_db ? static_cast<int64_t>(sqlite3_last_insert_rowid(m_db)) : 0;
}

bool DbService::BackupTo(const wxString& destPath, wxString* outError) {
    if (!IsOpen()) {
        if (outError) {
            *outError = "Source database is not open.";
        }
        return false;
    }

    sqlite3* destDb = nullptr;
    const wxScopedCharBuffer utf8Dest = destPath.ToUTF8();
    int rc = sqlite3_open_v2(utf8Dest.data(), &destDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             nullptr);
    if (rc != SQLITE_OK || !destDb) {
        if (outError) {
            *outError = destDb ? wxString::FromUTF8(sqlite3_errmsg(destDb))
                               : "Failed to open destination backup database file.";
        }
        if (destDb) {
            sqlite3_close_v2(destDb);
        }
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(destDb, "main", m_db, "main");
    if (!backup) {
        if (outError) {
            *outError = wxString::FromUTF8(sqlite3_errmsg(destDb));
        }
        sqlite3_close_v2(destDb);
        return false;
    }

    // Copy pages in chunks of 100 pages per step
    do {
        rc = sqlite3_backup_step(backup, 100);
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            sqlite3_sleep(25);
        }
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

    int finishRc = sqlite3_backup_finish(backup);

    bool success = (rc == SQLITE_DONE) && (finishRc == SQLITE_OK);
    if (!success && outError) {
        *outError = wxString::FromUTF8(sqlite3_errmsg(destDb));
    }

    sqlite3_close_v2(destDb);
    return success;
}

bool DbService::RestoreFrom(const wxString& backupPath, wxString* outError) {
    if (!IsOpen()) {
        if (outError)
            *outError = "Active database is not open.";
        return false;
    }

    // 1. Verify candidate backup database
    DbService backupChecker;
    if (!backupChecker.Open(backupPath, outError)) {
        return false;
    }
    if (!backupChecker.CheckIntegrity(outError)) {
        return false;
    }
    if (!backupChecker.CheckCompatibility(outError)) {
        return false;
    }
    backupChecker.Close();

    // 2. Prepare timestamped safety copy alongside working database
    wxFileName currentFn(m_dbPath);
    wxString timestamp = wxDateTime::Now().Format("%Y%m%d_%H%M%S");
    wxString safetyPath = wxString::Format("%s/%s.backup.%s", currentFn.GetPath(),
                                           currentFn.GetFullName(), timestamp);

    wxString currentDbPath = m_dbPath;

    // Create safety copy via online backup
    if (!BackupTo(safetyPath, outError)) {
        if (outError) {
            *outError = wxString::Format("Failed to create safety backup: %s", *outError);
        }
        return false;
    }

    // 3. Close active database for atomic replace
    Close();

    wxString tempPath = wxString::Format("%s.tmp_restore", currentDbPath);
    if (wxFileExists(tempPath)) {
        wxRemoveFile(tempPath);
    }

    if (!wxCopyFile(backupPath, tempPath)) {
        if (outError) {
            *outError = "Failed to copy backup file to temporary location.";
        }
        // Reopen current database
        Open(currentDbPath, nullptr);
        return false;
    }

    if (!wxRenameFile(tempPath, currentDbPath, true /* overwrite */)) {
        if (outError) {
            *outError = "Atomic rename of temporary file to working database failed.";
        }
        wxRemoveFile(tempPath);
        Open(currentDbPath, nullptr);
        return false;
    }

    // 4. Reopen and verify working database
    if (!Open(currentDbPath, outError)) {
        return false;
    }

    return true;
}

const char* DbService::GetEmbeddedSchemaSql() {
    return R"SQL(
PRAGMA foreign_keys = ON;

-- 1. Members
CREATE TABLE IF NOT EXISTS members (
    member_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    class TEXT,
    phone TEXT,
    email TEXT,
    status TEXT NOT NULL DEFAULT 'Active'
        CHECK (status IN ('Active', 'Former')),
    join_date TEXT NOT NULL 
);

-- 2. Formal meetings
CREATE TABLE IF NOT EXISTS meetings (
    meeting_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    type TEXT NOT NULL
        CHECK (type IN ('AGM', 'EGM', 'ECM')),
    meeting_date TEXT,
    start_time TEXT,
    end_time TEXT,
    location TEXT,
    CHECK (start_time IS NULL OR end_time IS NULL OR end_time >= start_time)
);

-- 3. Meeting attendance
CREATE TABLE IF NOT EXISTS meeting_attendance (
    meeting_id INTEGER NOT NULL,
    member_id INTEGER NOT NULL,
    PRIMARY KEY (meeting_id, member_id),
    FOREIGN KEY (meeting_id)
        REFERENCES meetings(meeting_id) ON DELETE RESTRICT,
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- 4. Projects
CREATE TABLE IF NOT EXISTS projects (
    project_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'Planned'
        CHECK (status IN (
            'Planned', 'Ongoing', 'Completed', 'Cancelled'
        )),
    start_date TEXT,
    target_end_date TEXT,
    actual_end_date TEXT,
    CHECK (
        start_date IS NULL
        OR target_end_date IS NULL
        OR target_end_date >= start_date
    ),
    CHECK (
        start_date IS NULL
        OR actual_end_date IS NULL
        OR actual_end_date >= start_date
    )
);

-- 5. Project members
CREATE TABLE IF NOT EXISTS project_members (
    project_id INTEGER NOT NULL,
    member_id INTEGER NOT NULL,
    role TEXT NOT NULL DEFAULT 'Member' CHECK (role in ('Member','Lead')),
    PRIMARY KEY (project_id, member_id),
    FOREIGN KEY (project_id)
        REFERENCES projects(project_id) ON DELETE RESTRICT,
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- 6. Committee appointments
CREATE TABLE IF NOT EXISTS committee_appointments (
    start_date TEXT NOT NULL,
    office TEXT NOT NULL,
    member_id INTEGER NOT NULL,
    end_date TEXT,
    PRIMARY KEY (start_date, office),
    CHECK (end_date IS NULL OR end_date >= start_date),
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- 7. Society-run activities
CREATE TABLE IF NOT EXISTS activities (
    activity_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    type TEXT NOT NULL,
    activity_date TEXT,
    start_time TEXT,
    end_time TEXT,
    location TEXT,
    CHECK (start_time IS NULL OR end_time IS NULL OR end_time >= start_time)
);

-- 8. External competitions
CREATE TABLE IF NOT EXISTS external_competitions (
    competition_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    year INTEGER NOT NULL,
    UNIQUE (title, year)
);

-- 9. External competition participants
CREATE TABLE IF NOT EXISTS competition_participants (
    competition_id INTEGER NOT NULL,
    member_id INTEGER NOT NULL,
    role TEXT NOT NULL,
    PRIMARY KEY (competition_id, member_id),
    FOREIGN KEY (competition_id)
        REFERENCES external_competitions(competition_id)
        ON DELETE RESTRICT,
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- Indexes for common lookups
CREATE INDEX IF NOT EXISTS idx_members_status
    ON members(status);

CREATE INDEX IF NOT EXISTS idx_attendance_member
    ON meeting_attendance(member_id);

CREATE INDEX IF NOT EXISTS idx_project_members_member
    ON project_members(member_id);

CREATE INDEX IF NOT EXISTS idx_committee_member
    ON committee_appointments(member_id);

CREATE INDEX IF NOT EXISTS idx_competition_participants_member
    ON competition_participants(member_id);
)SQL";
}

const std::map<std::string, std::set<std::string>>& DbService::GetRequiredSchema() {
    static const std::map<std::string, std::set<std::string>> s_schema = {
        {"members", {"member_id", "name", "class", "phone", "email", "status", "join_date"}},
        {"meetings",
         {"meeting_id", "title", "type", "meeting_date", "start_time", "end_time", "location"}},
        {"meeting_attendance", {"meeting_id", "member_id"}},
        {"projects",
         {"project_id", "title", "status", "start_date", "target_end_date", "actual_end_date"}},
        {"project_members", {"project_id", "member_id", "role"}},
        {"committee_appointments", {"start_date", "office", "member_id", "end_date"}},
        {"activities",
         {"activity_id", "title", "type", "activity_date", "start_time", "end_time", "location"}},
        {"external_competitions", {"competition_id", "title", "year"}},
        {"competition_participants", {"competition_id", "member_id", "role"}}};
    return s_schema;
}

} // namespace msms
