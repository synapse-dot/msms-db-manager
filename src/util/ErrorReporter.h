#ifndef MSMS_ERROR_REPORTER_H
#define MSMS_ERROR_REPORTER_H

#include <cstdint>

#include <wx/string.h>

#include <sqlite3.h>

namespace msms {

class DbService;

/**
 * @brief Structured representation of an intercepted database or application error.
 */
struct ErrorInfo {
    int errorCode{SQLITE_OK};
    int extendedErrorCode{SQLITE_OK};
    wxString rawMessage;
    wxString plainExplanation;

    /**
     * @brief Formats the error for modal dialog or user display.
     * Contains the raw SQLite message and a plain-language explanation if available.
     */
    [[nodiscard]] wxString FormatForDisplay() const {
        if (plainExplanation.IsEmpty()) {
            return rawMessage;
        }
        return wxString::Format("%s\n\nExplanation: %s", rawMessage, plainExplanation);
    }
};

/**
 * @brief Captures and explains SQLite errors with human-friendly context.
 *
 * This utility does not log errors or display UI. Callers decide how to present
 * the returned ErrorInfo, so this code remains safe for tests and headless use.
 */
class ErrorReporter {
public:
    /**
     * @brief Capture and format an error from raw sqlite3 handle.
     * @param db SQLite connection handle.
     * @param context Optional action description (e.g. "saving member").
     */
    static ErrorInfo Report(sqlite3* db, const wxString& context = wxEmptyString);

    /**
     * @brief Capture and format an error from a DbService instance.
     */
    static ErrorInfo Report(const DbService& db, const wxString& context = wxEmptyString);

    /**
     * @brief Capture and format an error specifically for DELETE operations.
     * Queries foreign-key referencing tables to identify the exact blocking relationship.
     * @param db SQLite connection handle.
     * @param table Table where deletion was attempted (e.g. "members", "meetings").
     * @param primaryId Primary key ID of the entity being deleted.
     */
    static ErrorInfo ReportDeleteError(sqlite3* db, const wxString& table, int64_t primaryId);

    /**
     * @brief Same as above, taking DbService.
     */
    static ErrorInfo ReportDeleteError(const DbService& db, const wxString& table,
                                       int64_t primaryId);

    /**
     * @brief Overload for composite primary keys (e.g. committee_appointments).
     */
    static ErrorInfo ReportDeleteError(sqlite3* db, const wxString& table,
                                       const wxString& compositeKeyDesc);

    /**
     * @brief Helper to check if an error code represents a foreign key constraint violation.
     */
    static bool IsForeignKeyViolation(int errorCode, int extendedErrorCode);

    /**
     * @brief Helper to check if an error code represents a unique constraint violation.
     */
    static bool IsUniqueViolation(int errorCode, int extendedErrorCode);

    /**
     * @brief Helper to check if an error code represents a check constraint violation.
     */
    static bool IsCheckViolation(int errorCode, int extendedErrorCode);

private:
    static wxString DiagnoseForeignKeyBlock(sqlite3* db, const wxString& table, int64_t primaryId);
    static wxString ExplainConstraint(int extendedCode, const wxString& rawMsg,
                                      const wxString& context);
};

} // namespace msms

#endif // MSMS_ERROR_REPORTER_H
