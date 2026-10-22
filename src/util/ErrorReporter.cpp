#include "ErrorReporter.h"

#include <wx/log.h>

#include "db/DbService.h"

namespace msms {

namespace {

int64_t QueryCount(sqlite3* db, const char* sql, int64_t param) {
    if (!db)
        return 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
        return 0;
    }
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(param));
    int64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<int64_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return count;
}

} // namespace

bool ErrorReporter::IsForeignKeyViolation(int errorCode, int extendedErrorCode) {
    return extendedErrorCode == SQLITE_CONSTRAINT_FOREIGNKEY ||
           (errorCode == SQLITE_CONSTRAINT && extendedErrorCode == SQLITE_CONSTRAINT);
}

bool ErrorReporter::IsUniqueViolation(int errorCode, int extendedErrorCode) {
    return extendedErrorCode == SQLITE_CONSTRAINT_UNIQUE ||
           extendedErrorCode == SQLITE_CONSTRAINT_PRIMARYKEY;
}

bool ErrorReporter::IsCheckViolation(int errorCode, int extendedErrorCode) {
    return extendedErrorCode == SQLITE_CONSTRAINT_CHECK;
}

ErrorInfo ErrorReporter::Report(sqlite3* db, const wxString& context) {
    ErrorInfo info;
    if (!db) {
        info.rawMessage = "Database handle is null.";
        info.plainExplanation = "An unexpected internal database error occurred.";
        return info;
    }

    info.errorCode = sqlite3_errcode(db);
    info.extendedErrorCode = sqlite3_extended_errcode(db);
    const char* raw = sqlite3_errmsg(db);
    info.rawMessage = raw ? wxString::FromUTF8(raw) : wxString("Unknown SQLite error");

    info.plainExplanation = ExplainConstraint(info.extendedErrorCode, info.rawMessage, context);

    if (info.errorCode != SQLITE_OK) {
        wxLogError("Database error (%d / %d): %s. Context: %s. Explanation: %s", info.errorCode,
                   info.extendedErrorCode, info.rawMessage, context.IsEmpty() ? "None" : context,
                   info.plainExplanation);
    }

    return info;
}

ErrorInfo ErrorReporter::Report(const DbService& db, const wxString& context) {
    return Report(db.GetRawHandle(), context);
}

ErrorInfo ErrorReporter::ReportDeleteError(sqlite3* db, const wxString& table, int64_t primaryId) {
    ErrorInfo info = Report(db, wxString::Format("deleting from %s", table));

    // If this is a foreign key violation, provide the specific relationship diagnosis
    if (IsForeignKeyViolation(info.errorCode, info.extendedErrorCode) ||
        info.rawMessage.Contains("FOREIGN KEY")) {
        wxString diagnosis = DiagnoseForeignKeyBlock(db, table, primaryId);
        if (!diagnosis.IsEmpty()) {
            info.plainExplanation = diagnosis;
        }
    }

    return info;
}

ErrorInfo ErrorReporter::ReportDeleteError(const DbService& db, const wxString& table,
                                           int64_t primaryId) {
    return ReportDeleteError(db.GetRawHandle(), table, primaryId);
}

ErrorInfo ErrorReporter::ReportDeleteError(sqlite3* db, const wxString& table,
                                           const wxString& compositeKeyDesc) {
    ErrorInfo info = Report(db, wxString::Format("deleting from %s (%s)", table, compositeKeyDesc));
    return info;
}

wxString ErrorReporter::DiagnoseForeignKeyBlock(sqlite3* db, const wxString& table,
                                                int64_t primaryId) {
    if (!db)
        return wxEmptyString;

    // 1. members deletion blocked by child relationships
    if (table.CmpNoCase("members") == 0) {
        if (QueryCount(db, "SELECT COUNT(*) FROM meeting_attendance WHERE member_id = ?;",
                       primaryId) > 0) {
            return "meeting_attendance blocks member deletion: This member cannot be deleted "
                   "because they are recorded in meeting attendance.";
        }
        if (QueryCount(db, "SELECT COUNT(*) FROM project_members WHERE member_id = ?;", primaryId) >
            0) {
            return "project_members blocks member deletion: This member cannot be deleted "
                   "because they are assigned to a project team.";
        }
        if (QueryCount(db, "SELECT COUNT(*) FROM committee_appointments WHERE member_id = ?;",
                       primaryId) > 0) {
            return "committee_appointments blocks member deletion: This member cannot be deleted "
                   "because they hold a committee appointment.";
        }
        if (QueryCount(db, "SELECT COUNT(*) FROM competition_participants WHERE member_id = ?;",
                       primaryId) > 0) {
            return "competition_participants blocks member deletion: This member cannot be "
                   "deleted because they are registered as a competition participant.";
        }
        return "A foreign key constraint in a related table prevents deleting this member.";
    }

    // 2. meetings deletion blocked by meeting_attendance
    if (table.CmpNoCase("meetings") == 0) {
        if (QueryCount(db, "SELECT COUNT(*) FROM meeting_attendance WHERE meeting_id = ?;",
                       primaryId) > 0) {
            return "meeting_attendance blocks meeting deletion: This meeting cannot be deleted "
                   "because members are recorded in its attendance register.";
        }
        return "A foreign key constraint prevents deleting this meeting.";
    }

    // 3. projects deletion blocked by project_members
    if (table.CmpNoCase("projects") == 0) {
        if (QueryCount(db, "SELECT COUNT(*) FROM project_members WHERE project_id = ?;",
                       primaryId) > 0) {
            return "project_members blocks project deletion: This project cannot be deleted "
                   "because members are assigned to its project team.";
        }
        return "A foreign key constraint prevents deleting this project.";
    }

    // 4. external_competitions deletion blocked by competition_participants
    if (table.CmpNoCase("external_competitions") == 0) {
        if (QueryCount(db,
                       "SELECT COUNT(*) FROM competition_participants WHERE competition_id = ?;",
                       primaryId) > 0) {
            return "competition_participants blocks competition deletion: This competition cannot "
                   "be deleted because participants are registered on its team.";
        }
        return "A foreign key constraint prevents deleting this competition.";
    }

    return wxEmptyString;
}

wxString ErrorReporter::ExplainConstraint(int extendedCode, const wxString& rawMsg,
                                          const wxString& context) {
    if (extendedCode == SQLITE_CONSTRAINT_FOREIGNKEY || rawMsg.Contains("FOREIGN KEY")) {
        return "A foreign-key constraint prevents this action because related records exist.";
    }

    if (extendedCode == SQLITE_CONSTRAINT_UNIQUE || extendedCode == SQLITE_CONSTRAINT_PRIMARYKEY ||
        rawMsg.Contains("UNIQUE constraint failed")) {
        if (rawMsg.Contains("external_competitions.title") ||
            rawMsg.Contains("external_competitions.year")) {
            return "A competition with this title and year already exists.";
        }
        if (rawMsg.Contains("committee_appointments.start_date") ||
            rawMsg.Contains("committee_appointments.office")) {
            return "An appointment for this office and start date already exists.";
        }
        return "A record with these unique details already exists.";
    }

    if (extendedCode == SQLITE_CONSTRAINT_CHECK || rawMsg.Contains("CHECK constraint failed")) {
        return "One or more values violate the required constraints (e.g. end date/time before "
               "start date/time, or an invalid status value).";
    }

    if (!context.IsEmpty()) {
        return wxString::Format("The database operation failed while %s.", context);
    }

    return wxEmptyString;
}

} // namespace msms
