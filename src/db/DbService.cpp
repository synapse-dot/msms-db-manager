#include "DbService.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <wx/datetime.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace msms {
namespace {

wxString NormalizeFullPath(const wxString& path) {
    wxFileName file(path);
    file.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    return file.GetFullPath();
}

bool SameFileIdentity(const wxString& first, const wxString& second) {
    if (wxFileName(NormalizeFullPath(first)).SameAs(wxFileName(NormalizeFullPath(second)))) {
        return true;
    }
#ifdef _WIN32
    const wxScopedWCharBuffer firstWide = first.wc_str();
    const wxScopedWCharBuffer secondWide = second.wc_str();
    HANDLE firstHandle = CreateFileW(firstWide.data(), FILE_READ_ATTRIBUTES,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (firstHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    HANDLE secondHandle = CreateFileW(secondWide.data(), FILE_READ_ATTRIBUTES,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (secondHandle == INVALID_HANDLE_VALUE) {
        CloseHandle(firstHandle);
        return false;
    }
    BY_HANDLE_FILE_INFORMATION firstInfo{};
    BY_HANDLE_FILE_INFORMATION secondInfo{};
    const bool firstOk = GetFileInformationByHandle(firstHandle, &firstInfo) != 0;
    const bool secondOk = GetFileInformationByHandle(secondHandle, &secondInfo) != 0;
    CloseHandle(firstHandle);
    CloseHandle(secondHandle);
    return firstOk && secondOk &&
           firstInfo.dwVolumeSerialNumber == secondInfo.dwVolumeSerialNumber &&
           firstInfo.nFileIndexHigh == secondInfo.nFileIndexHigh &&
           firstInfo.nFileIndexLow == secondInfo.nFileIndexLow;
#else
    const wxScopedCharBuffer firstUtf8 = NormalizeFullPath(first).ToUTF8();
    const wxScopedCharBuffer secondUtf8 = NormalizeFullPath(second).ToUTF8();
    struct stat firstInfo {};
    struct stat secondInfo {};
    if (::stat(firstUtf8.data(), &firstInfo) != 0 || ::stat(secondUtf8.data(), &secondInfo) != 0) {
        return false;
    }
    return firstInfo.st_dev == secondInfo.st_dev && firstInfo.st_ino == secondInfo.st_ino;
#endif
}

wxString PathLockFileName(const wxString& dbPath) {
    const wxFileName databaseFile(NormalizeFullPath(dbPath));
    return databaseFile.GetPathWithSep() + "." + databaseFile.GetFullName() + ".msms.lock";
}

void RemoveEphemeralPathLock(const wxString& dbPath) {
    // Only call this for a unique temporary database path after its DbService
    // connection has been closed; permanent database lock files must remain in place.
    (void)wxRemoveFile(PathLockFileName(dbPath));
}

bool AtomicReplaceFile(const wxString& stagedPath, const wxString& workingPath,
                       wxString* outError) {
#ifdef _WIN32
    const wxScopedWCharBuffer stagedWide = stagedPath.wc_str();
    const wxScopedWCharBuffer workingWide = workingPath.wc_str();
    if (MoveFileExW(stagedWide.data(), workingWide.data(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    if (outError) {
        *outError = wxString::Format("Atomic file replacement failed (Windows error %lu).",
                                     static_cast<unsigned long>(GetLastError()));
    }
#else
    const wxScopedCharBuffer stagedUtf8 = stagedPath.ToUTF8();
    const wxScopedCharBuffer workingUtf8 = workingPath.ToUTF8();
    if (std::rename(stagedUtf8.data(), workingUtf8.data()) == 0) {
        return true;
    }
    if (outError) {
        *outError = wxString::Format("Atomic file replacement failed (system error %d: %s).",
                                     errno, wxString::FromUTF8(std::strerror(errno)));
    }
#endif
    return false;
}

bool TryNativePathLock(void* handle, int fd, bool exclusive, wxString* outError) {
#ifdef _WIN32
    if (!handle) {
        if (outError) {
            *outError = "The database coordination lock is not open.";
        }
        return false;
    }
    OVERLAPPED range{};
    DWORD flags = LOCKFILE_FAIL_IMMEDIATELY;
    if (exclusive) {
        flags |= LOCKFILE_EXCLUSIVE_LOCK;
    }
    if (LockFileEx(reinterpret_cast<HANDLE>(handle), flags, 0, 1, 0, &range)) {
        return true;
    }
    if (outError) {
        *outError = wxString::Format(
            "The database is open in another application instance or a restore is already in "
            "progress (Windows lock error %lu).",
            static_cast<unsigned long>(GetLastError()));
    }
#else
    if (fd < 0) {
        if (outError) {
            *outError = "The database coordination lock is not open.";
        }
        return false;
    }
    const int operation = (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB;
    if (::flock(fd, operation) == 0) {
        return true;
    }
    if (outError) {
        *outError = wxString::Format(
            "The database is open in another application instance or a restore is already in "
            "progress (system lock error %d: %s).",
            errno, wxString::FromUTF8(std::strerror(errno)));
    }
#endif
    return false;
}

bool UnlockNativePathLock(void* handle, int fd, wxString* outError) {
#ifdef _WIN32
    if (!handle) {
        return true;
    }
    OVERLAPPED range{};
    if (UnlockFileEx(reinterpret_cast<HANDLE>(handle), 0, 1, 0, &range)) {
        return true;
    }
    if (outError) {
        *outError = wxString::Format("Could not release the database coordination lock "
                                     "(Windows error %lu).",
                                     static_cast<unsigned long>(GetLastError()));
    }
#else
    if (fd < 0) {
        return true;
    }
    if (::flock(fd, LOCK_UN) == 0) {
        return true;
    }
    if (outError) {
        *outError = wxString::Format("Could not release the database coordination lock "
                                     "(system error %d: %s).",
                                     errno, wxString::FromUTF8(std::strerror(errno)));
    }
#endif
    return false;
}

} // namespace

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

bool DbService::AcquirePathLock(const wxString& dbPath, bool exclusive, wxString* outError) {
    const wxString lockPath = PathLockFileName(dbPath);
    ReleasePathLock();

#ifdef _WIN32
    const wxScopedWCharBuffer wideLockPath = lockPath.wc_str();
    HANDLE handle = CreateFileW(wideLockPath.data(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        if (outError) {
            *outError = wxString::Format(
                "Could not open database coordination lock (Windows error %lu).",
                                         static_cast<unsigned long>(GetLastError()));
        }
        return false;
    }
    m_nativePathLockHandle = reinterpret_cast<void*>(handle);
#else
    const wxScopedCharBuffer lockPathUtf8 = lockPath.ToUTF8();
    const int fd = ::open(lockPathUtf8.data(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        if (outError) {
            *outError = wxString::Format(
                "Could not open database coordination lock (system error %d: %s).",
                                         errno, wxString::FromUTF8(std::strerror(errno)));
        }
        return false;
    }
    m_nativePathLockFd = fd;
#endif

    wxString lockError;
    if (!TryNativePathLock(m_nativePathLockHandle, m_nativePathLockFd, exclusive, &lockError)) {
#ifdef _WIN32
        CloseHandle(reinterpret_cast<HANDLE>(m_nativePathLockHandle));
        m_nativePathLockHandle = nullptr;
#else
        ::close(m_nativePathLockFd);
        m_nativePathLockFd = -1;
#endif
        if (outError) {
            *outError = lockError;
        }
        return false;
    }

    m_pathLockHeld = true;
    m_pathLockExclusive = exclusive;
    return true;
}

bool DbService::ChangePathLockMode(bool exclusive, wxString* outError) {
    if (!m_pathLockHeld) {
        if (outError) {
            *outError = "No database coordination lock is held.";
        }
        return false;
    }
    if (m_pathLockExclusive == exclusive) {
        return true;
    }

    const bool previousMode = m_pathLockExclusive;
    wxString unlockError;
    if (!UnlockNativePathLock(m_nativePathLockHandle, m_nativePathLockFd, &unlockError)) {
        if (outError) {
            *outError = unlockError;
        }
        return false;
    }
    m_pathLockHeld = false;

    wxString targetError;
    if (TryNativePathLock(m_nativePathLockHandle, m_nativePathLockFd, exclusive, &targetError)) {
        m_pathLockHeld = true;
        m_pathLockExclusive = exclusive;
        return true;
    }

    // Lock-mode transitions are not atomic on every supported OS. Reacquire the
    // previous mode before returning so a failed restore upgrade does not leave
    // an open SQLite connection without its normal shared path lock.
    wxString previousError;
    if (TryNativePathLock(m_nativePathLockHandle, m_nativePathLockFd, previousMode,
                          &previousError)) {
        m_pathLockHeld = true;
        m_pathLockExclusive = previousMode;
        if (outError) {
            *outError = targetError;
        }
        return false;
    }

    if (outError) {
        *outError = targetError +
                    "\nThe previous path lock could not be reacquired: " + previousError;
    }
    return false;
}

void DbService::ReleasePathLock() {
    if (m_pathLockHeld) {
        (void)UnlockNativePathLock(m_nativePathLockHandle, m_nativePathLockFd, nullptr);
        m_pathLockHeld = false;
    }
#ifdef _WIN32
    if (m_nativePathLockHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_nativePathLockHandle));
        m_nativePathLockHandle = nullptr;
    }
#else
    if (m_nativePathLockFd >= 0) {
        ::close(m_nativePathLockFd);
        m_nativePathLockFd = -1;
    }
#endif
    m_pathLockExclusive = false;
}

void DbService::CloseConnectionPreservingPathLock() {
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

void DbService::Close() {
    CloseConnectionPreservingPathLock();
    ReleasePathLock();
}

bool DbService::Open(const wxString& dbPath, wxString* outError) {
    Close();

    if (!wxFileExists(dbPath)) {
        if (outError) {
            *outError = wxString::Format("Database file does not exist: %s", dbPath);
        }
        return false;
    }

    if (!AcquirePathLock(dbPath, false, outError)) {
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

    // Exclusively lock the target while initialising it so another app instance
    // cannot open the partially-created database.
    if (!AcquirePathLock(dbPath, true, outError)) {
        return false;
    }

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

    // Do not transition from exclusive to shared while the connection remains
    // open; close under the exclusive lock, release it, then reopen with a shared
    // lock so another process can never write to an orphaned database handle.
    CloseConnectionPreservingPathLock();
    ReleasePathLock();
    return Open(dbPath, outError);
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
    // Keep the transaction marked active when COMMIT fails, allowing the RAII
    // Transaction guard to attempt a rollback instead of abandoning partial work.
    if (ok) {
        m_inTransaction = false;
    }
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

    if (destPath.IsEmpty()) {
        if (outError) {
            *outError = "A destination path is required for the database backup.";
        }
        return false;
    }

    wxFileName sourceFile(m_dbPath);
    wxFileName destinationFile(destPath);
    sourceFile.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    destinationFile.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    if (sourceFile.SameAs(destinationFile)) {
        if (outError) {
            *outError = "The backup destination must be different from the currently open database.";
        }
        return false;
    }

    const wxString finalPath = destinationFile.GetFullPath();
    const wxString tempPrefix =
        destinationFile.GetPathWithSep() + "." + destinationFile.GetFullName() + ".tmp.";
    const wxString tempPath = wxFileName::CreateTempFileName(tempPrefix);
    if (tempPath.IsEmpty()) {
        if (outError) {
            *outError = "Could not create a temporary backup file beside the destination.";
        }
        return false;
    }

    // Build the backup at a temporary path. An existing destination must not be
    // replaced until the copied database has been closed and fully verified.
    sqlite3* destDb = nullptr;
    const wxScopedCharBuffer utf8Dest = tempPath.ToUTF8();
    int rc = sqlite3_open_v2(utf8Dest.data(), &destDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             nullptr);
    if (rc != SQLITE_OK || !destDb) {
        if (outError) {
            *outError = destDb ? wxString::FromUTF8(sqlite3_errmsg(destDb))
                               : "Failed to open temporary backup database file.";
        }
        if (destDb) {
            sqlite3_close_v2(destDb);
        }
        wxRemoveFile(tempPath);
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(destDb, "main", m_db, "main");
    if (!backup) {
        if (outError) {
            *outError = wxString::FromUTF8(sqlite3_errmsg(destDb));
        }
        sqlite3_close_v2(destDb);
        wxRemoveFile(tempPath);
        return false;
    }

    // Copy pages in chunks. Count BUSY/LOCKED retries across the entire
    // operation: successful page-copy steps must not reset the overall budget.
    // This bounds total lock-retry time even when locks recur intermittently.
    int totalBusyRetries = 0;
    do {
        rc = sqlite3_backup_step(backup, 100);
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            ++totalBusyRetries;
            if (totalBusyRetries >= 200) {
                break;
            }
            sqlite3_sleep(25);
        }
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

    const int finishRc = sqlite3_backup_finish(backup);
    const bool copyCompleted = (rc == SQLITE_DONE) && (finishRc == SQLITE_OK);
    if (!copyCompleted) {
        const wxString backupError = wxString::FromUTF8(sqlite3_errmsg(destDb));
        sqlite3_close_v2(destDb);
        wxRemoveFile(tempPath);
        if (outError) {
            *outError = wxString("SQLite online backup did not complete successfully: ") +
                        backupError;
        }
        return false;
    }

    const int closeRc = sqlite3_close_v2(destDb);
    if (closeRc != SQLITE_OK) {
        wxRemoveFile(tempPath);
        if (outError) {
            *outError = wxString::Format(
                "The backup pages were copied, but the temporary destination could not be closed "
                "for verification (SQLite code %d).",
                closeRc);
        }
        return false;
    }

    // Do not report success until the completed temporary file can be reopened
    // and passes both SQLite integrity and application schema checks.
    DbService verifier;
    wxString verificationError;
    if (!verifier.Open(tempPath, &verificationError)) {
        RemoveEphemeralPathLock(tempPath);
        wxRemoveFile(tempPath);
        if (outError) {
            *outError = wxString("The backup copy could not be reopened for verification: ") +
                        verificationError;
        }
        return false;
    }

    if (!verifier.CheckIntegrity(&verificationError)) {
        verifier.Close();
        RemoveEphemeralPathLock(tempPath);
        wxRemoveFile(tempPath);
        if (outError) {
            *outError = wxString("The backup copy failed its integrity check: ") +
                        verificationError;
        }
        return false;
    }

    if (!verifier.CheckCompatibility(&verificationError)) {
        verifier.Close();
        RemoveEphemeralPathLock(tempPath);
        wxRemoveFile(tempPath);
        if (outError) {
            *outError = wxString("The backup copy failed the database schema check: ") +
                        verificationError;
        }
        return false;
    }

    verifier.Close();
    RemoveEphemeralPathLock(tempPath);

    // Replace the chosen destination only after all checks passed. This keeps a
    // pre-existing good backup intact when copying or verification fails.
    if (!wxRenameFile(tempPath, finalPath, true)) {
        wxRemoveFile(tempPath);
        if (outError) {
            *outError = "The verified backup could not be moved to the selected destination.";
        }
        return false;
    }

    return true;
}

bool DbService::RestoreFrom(const wxString& backupPath, wxString* outError) {
    if (!IsOpen()) {
        if (outError) {
            *outError = "Active database is not open.";
        }
        return false;
    }

    if (backupPath.IsEmpty() || !wxFileExists(backupPath)) {
        if (outError) {
            *outError = backupPath.IsEmpty()
                            ? "A backup file must be selected."
                            : wxString::Format("Backup file does not exist: %s", backupPath);
        }
        return false;
    }

    const wxString currentDbPath = m_dbPath;
    const wxString workingPath = NormalizeFullPath(currentDbPath);
    const wxString candidatePath = NormalizeFullPath(backupPath);
    wxFileName workingFile(workingPath);
    wxFileName candidateFile(candidatePath);
    if (workingFile.SameAs(candidateFile)) {
        if (outError) {
            *outError = "The selected backup is the currently open database.";
        }
        return false;
    }

    const auto verifyDatabaseFile = [](const wxString& path, wxString* error) {
        DbService verifier;
        if (!verifier.Open(path, error)) {
            return false;
        }
        if (!verifier.CheckIntegrity(error) || !verifier.CheckCompatibility(error)) {
            verifier.Close();
            return false;
        }
        // Open enables foreign keys and verifies PRAGMA foreign_keys = 1.
        verifier.Close();
        return true;
    };

    // Validate before acquiring the exclusive working-database lock or creating
    // any safety files, so a bad candidate cannot disrupt a live database.
    wxString candidateError;
    if (!verifyDatabaseFile(candidatePath, &candidateError)) {
        if (outError) {
            *outError = wxString("The selected backup is not a valid MSMS database: ") +
                        candidateError;
        }
        return false;
    }

    // Every DbService connection holds a shared OS lock on <db>.msms.lock.
    // Restore must upgrade that lock to exclusive; if another app instance has
    // the database open, the upgrade fails and the file is never replaced.
    wxString lockError;
    if (!ChangePathLockMode(true, &lockError)) {
        if (!m_pathLockHeld) {
            // The OS lock mode transition failed to reacquire the prior shared
            // lock. Drop this SQLite handle before releasing the lock file, then
            // try to reconnect through the normal coordinated Open path.
            CloseConnectionPreservingPathLock();
            ReleasePathLock();
            wxString reopenError;
            if (!Open(workingPath, &reopenError) && outError) {
                *outError = wxString("Restore cancelled because another process has the database "
                                     "open. The connection could not be reopened yet: ") +
                            reopenError;
                return false;
            }
        }
        if (outError) {
            *outError = wxString("Restore cancelled: another MSMS Manager instance has this "
                                 "database open, or another restore is in progress. Close the "
                                 "other instance and retry.") +
                        (lockError.IsEmpty() ? wxString() : wxString("\n\n") + lockError);
        }
        return false;
    }

    const auto releaseExclusiveAndReopenOriginal = [&](const wxString& failure) {
        // No database connection is allowed to remain open during the lock-mode
        // transition. Another restore may win the lock race, but then Open will
        // refuse to attach to a stale path while that restore is in progress.
        CloseConnectionPreservingPathLock();
        ReleasePathLock();
        wxString reopenError;
        if (Open(workingPath, &reopenError)) {
            if (outError) {
                *outError = failure +
                            "\n\nThe original database file was not replaced and the "
                            "connection was reopened.";
            }
        } else if (outError) {
            *outError = failure +
                        "\n\nThe original database file was not replaced, but reopening it "
                        "failed: " + reopenError;
        }
        return false;
    };

    // The exclusive coordination lock is now held, so cooperative MSMS Manager
    // instances cannot start or continue using the working database. Take the
    // safety snapshot only after obtaining that lock to include all earlier writes.
    wxFileName currentFn(workingPath);
    const wxString timestamp = wxDateTime::Now().Format("%Y%m%d_%H%M%S");
    const wxString safetyBase =
        currentFn.GetPathWithSep() + currentFn.GetFullName() + ".backup." + timestamp;
    wxString safetyPath = safetyBase;
    unsigned int safetySuffix = 1;
    while (wxFileExists(safetyPath) || NormalizeFullPath(safetyPath) == candidatePath) {
        safetyPath = safetyBase + wxString::Format("_%02u", safetySuffix++);
    }

    wxString safetyError;
    if (!BackupTo(safetyPath, &safetyError)) {
        return releaseExclusiveAndReopenOriginal(
            wxString("Failed to create the timestamped safety copy: ") + safetyError);
    }

    // Stage in the same directory as the working database, then verify the
    // staged copy before any replacement. The rename below is an OS-level
    // same-filesystem atomic replacement operation.
    const wxString tempPrefix =
        currentFn.GetPathWithSep() + "." + currentFn.GetFullName() + ".restore.tmp.";
    const wxString tempPath = wxFileName::CreateTempFileName(tempPrefix);
    if (tempPath.IsEmpty()) {
        return releaseExclusiveAndReopenOriginal(
            "Could not create a temporary restore file beside the working database.");
    }

    if (!wxCopyFile(candidatePath, tempPath, true)) {
        wxRemoveFile(tempPath);
        return releaseExclusiveAndReopenOriginal(
            "Could not copy the selected backup to the temporary restore file.");
    }

    wxString stagedError;
    if (!verifyDatabaseFile(tempPath, &stagedError)) {
        RemoveEphemeralPathLock(tempPath);
        wxRemoveFile(tempPath);
        return releaseExclusiveAndReopenOriginal(
            wxString("The staged restore file failed integrity or schema verification: ") +
            stagedError);
    }
    RemoveEphemeralPathLock(tempPath);

    // Close only SQLite while retaining the exclusive path lock. Thus there is
    // no window in which another cooperative process can open or write the old file.
    CloseConnectionPreservingPathLock();
    wxString replaceError;
    if (!AtomicReplaceFile(tempPath, workingPath, &replaceError)) {
        wxRemoveFile(tempPath);
        return releaseExclusiveAndReopenOriginal(
            wxString("Could not atomically replace the working database: ") + replaceError);
    }

    // The file is now replaced while the exclusive lock is held. Release the
    // lock only with the SQLite connection closed, then Open acquires a fresh
    // shared lock before it attaches to the path.
    ReleasePathLock();

    wxString activationError;
    bool activated = Open(workingPath, &activationError);
    if (activated &&
        (!CheckIntegrity(&activationError) || !CheckCompatibility(&activationError))) {
        Close();
        activated = false;
    }
    if (activated) {
        // Open already enabled and verified PRAGMA foreign_keys = 1.
        return true;
    }

    // An unexpected post-replacement activation failure requires restoring the
    // verified safety copy. Reacquire the exclusive path lock first; never replace
    // the database if another process has opened it in the meantime.
    if (IsOpen()) {
        Close();
    }
    wxString recoveryLockError;
    if (!AcquirePathLock(workingPath, true, &recoveryLockError)) {
        if (outError) {
            *outError = wxString("The backup was installed but the restored database could not "
                                 "be activated: ") +
                        activationError + "\n\nThe previous database is preserved at: " +
                        safetyPath + "\nCould not safely roll back because another process "
                                     "holds the working database lock: " +
                        recoveryLockError;
        }
        return false;
    }

    const wxString recoveryPrefix =
        currentFn.GetPathWithSep() + "." + currentFn.GetFullName() + ".restore-recovery.";
    const wxString recoveryPath = wxFileName::CreateTempFileName(recoveryPrefix);
    if (recoveryPath.IsEmpty()) {
        ReleasePathLock();
        if (outError) {
            *outError = wxString("The restored database could not be activated: ") +
                        activationError + "\n\nCould not stage the safety copy for recovery. "
                                         "The safety copy remains at: " +
                        safetyPath;
        }
        return false;
    }

    wxString recoveryError;
    bool recovered = wxCopyFile(safetyPath, recoveryPath, true);
    if (!recovered) {
        recoveryError = "Could not copy the verified safety database to a recovery file.";
    }
    if (recovered) {
        recovered = verifyDatabaseFile(recoveryPath, &recoveryError);
        RemoveEphemeralPathLock(recoveryPath);
    }
    if (recovered) {
        recovered = AtomicReplaceFile(recoveryPath, workingPath, &recoveryError);
    }
    if (!recovered) {
        wxRemoveFile(recoveryPath);
        ReleasePathLock();
        if (outError) {
            *outError = wxString("The restored database could not be activated: ") +
                        activationError + "\n\nSafety-copy recovery failed: " + recoveryError +
                        "\nThe safety copy remains at: " + safetyPath;
        }
        return false;
    }

    ReleasePathLock();
    wxString reopenError;
    if (!Open(workingPath, &reopenError) || !CheckIntegrity(&reopenError) ||
        !CheckCompatibility(&reopenError)) {
        Close();
        if (outError) {
            *outError = wxString("The restored database could not be activated: ") +
                        activationError + "\n\nThe previous database was copied back, but "
                        "reopening or verification failed: " + reopenError +
                        "\nThe safety copy remains at: " + safetyPath;
        }
        return false;
    }

    if (outError) {
        *outError = wxString("The restored database could not be activated: ") +
                    activationError + "\n\nThe previous database was restored from the "
                    "verified safety copy.";
    }
    return false;
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
