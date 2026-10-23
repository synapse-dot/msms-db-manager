#ifndef MSMS_DB_SERVICE_H
#define MSMS_DB_SERVICE_H

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <wx/string.h>

#include <sqlite3.h>

namespace msms {

/**
 * @brief RAII wrapper for prepared SQLite statement (sqlite3_stmt*).
 */
class Statement {
public:
    explicit Statement(sqlite3_stmt* stmt = nullptr);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    [[nodiscard]] bool IsValid() const noexcept { return m_stmt != nullptr; }
    [[nodiscard]] sqlite3_stmt* Get() const noexcept { return m_stmt; }

    // Parameter binding (1-based index)
    bool BindInt(int index, int val);
    bool BindInt64(int index, int64_t val);
    bool BindDouble(int index, double val);
    bool BindText(int index, const wxString& val);
    bool BindNull(int index);

    // Execution
    // Returns true if a row is available (SQLITE_ROW), false if done (SQLITE_DONE).
    // Throws or returns false on error.
    bool Step();
    bool Reset();

    // Column extraction (0-based index)
    [[nodiscard]] bool IsNull(int col) const;
    [[nodiscard]] int GetInt(int col) const;
    [[nodiscard]] int64_t GetInt64(int col) const;
    [[nodiscard]] double GetDouble(int col) const;
    [[nodiscard]] wxString GetText(int col) const;

    [[nodiscard]] int GetColumnCount() const;
    [[nodiscard]] wxString GetColumnName(int col) const;

private:
    sqlite3_stmt* m_stmt{nullptr};
};

/**
 * @brief RAII helper for database transactions.
 */
class Transaction;

/**
 * @brief Database service encapsulating SQLite connection and queries.
 */
class DbService {
public:
    DbService();
    ~DbService();

    DbService(const DbService&) = delete;
    DbService& operator=(const DbService&) = delete;

    // Connection lifecycle
    bool Open(const wxString& dbPath, wxString* outError = nullptr);
    bool CreateNew(const wxString& dbPath, wxString* outError = nullptr);
    void Close();

    [[nodiscard]] bool IsOpen() const noexcept { return m_db != nullptr; }
    [[nodiscard]] const wxString& GetDbPath() const noexcept { return m_dbPath; }
    [[nodiscard]] sqlite3* GetRawHandle() const noexcept { return m_db; }

    // Schema and checks
    bool InitializeSchema(wxString* outError = nullptr);
    [[nodiscard]] bool CheckCompatibility(wxString* outError = nullptr) const;
    [[nodiscard]] bool CheckIntegrity(wxString* outError = nullptr) const;

    // Statements and execution
    [[nodiscard]] std::unique_ptr<Statement> Prepare(const wxString& sql,
                                                     wxString* outError = nullptr) const;
    bool Execute(const wxString& sql, wxString* outError = nullptr);

    // Transactions
    bool Begin(wxString* outError = nullptr);
    bool Commit(wxString* outError = nullptr);
    bool Rollback(wxString* outError = nullptr);

    // Diagnostics & metadata
    [[nodiscard]] int GetLastErrorCode() const;
    [[nodiscard]] wxString GetLastErrorMessage() const;
    [[nodiscard]] int64_t GetLastInsertRowId() const;

    // Online backup and restore APIs
    bool BackupTo(const wxString& destPath, wxString* outError = nullptr);
    bool RestoreFrom(const wxString& backupPath, wxString* outError = nullptr);

    // Schema definition constant
    static const char* GetEmbeddedSchemaSql();

    // Required tables and expected columns
    static const std::map<std::string, std::set<std::string>>& GetRequiredSchema();

private:
    bool EnableForeignKeys(wxString* outError);
    bool VerifyForeignKeys(wxString* outError);

    // A sidecar OS lock coordinates database access between application processes.
    bool AcquirePathLock(const wxString& dbPath, bool exclusive, wxString* outError);
    bool ChangePathLockMode(bool exclusive, wxString* outError);
    void ReleasePathLock();
    void CloseConnectionPreservingPathLock();

    sqlite3* m_db{nullptr};
    wxString m_dbPath;
    bool m_inTransaction{false};
    void* m_nativePathLockHandle{nullptr};
    int m_nativePathLockFd{-1};
    wxString m_pathLockPath;
    bool m_pathLockHeld{false};
    bool m_pathLockExclusive{false};
};

/**
 * @brief RAII Transaction guard. Automatically rolls back on destruction if not committed.
 */
class Transaction {
public:
    explicit Transaction(DbService& db) : m_db(db) { m_active = m_db.Begin(&m_error); }

    ~Transaction() {
        if (m_active) {
            m_db.Rollback(nullptr);
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    [[nodiscard]] bool IsActive() const noexcept { return m_active; }
    [[nodiscard]] const wxString& GetError() const noexcept { return m_error; }

    bool Commit(wxString* outError = nullptr) {
        if (!m_active)
            return false;
        bool ok = m_db.Commit(outError);
        if (ok) {
            m_active = false;
        }
        return ok;
    }

    bool Rollback(wxString* outError = nullptr) {
        if (!m_active)
            return false;
        bool ok = m_db.Rollback(outError);
        m_active = false;
        return ok;
    }

private:
    DbService& m_db;
    bool m_active{false};
    wxString m_error;
};

} // namespace msms

#endif // MSMS_DB_SERVICE_H
