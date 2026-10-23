#ifndef MSMS_VALIDATION_H
#define MSMS_VALIDATION_H

#include <cstddef>
#include <cstdint>
#include <optional>

#include <wx/string.h>

#include <sqlite3.h>

#include "db/DbService.h"

namespace msms::validation {

inline wxString Trimmed(wxString value) {
    value.Trim(true);
    value.Trim(false);
    return value;
}

inline bool IsBlank(const wxString& value) {
    return Trimmed(value).IsEmpty();
}

inline bool ValidateRequiredField(const wxString& value, const wxString& fieldName,
                                  wxString* outError = nullptr) {
    if (!IsBlank(value)) {
        return true;
    }
    if (outError) {
        *outError = fieldName + " is required.";
    }
    return false;
}

inline bool HasDigitsAt(const wxString& value, std::size_t offset, std::size_t count) {
    if (offset + count > value.length()) {
        return false;
    }
    for (std::size_t i = offset; i < offset + count; ++i) {
        const wxChar ch = value[i];
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

inline int TwoDigitNumber(const wxString& value, std::size_t offset) {
    return (value[offset] - '0') * 10 + (value[offset + 1] - '0');
}

inline bool IsValidDate(const wxString& value) {
    const wxString date = Trimmed(value);
    if (date.length() != 10 || date[4] != '-' || date[7] != '-' ||
        !HasDigitsAt(date, 0, 4) || !HasDigitsAt(date, 5, 2) ||
        !HasDigitsAt(date, 8, 2)) {
        return false;
    }

    long year = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        year = year * 10 + (date[i] - '0');
    }
    const int month = TwoDigitNumber(date, 5);
    const int day = TwoDigitNumber(date, 8);
    if (year < 1 || month < 1 || month > 12 || day < 1) {
        return false;
    }

    static constexpr int daysInMonth[] = {31, 28, 31, 30, 31, 30,
                                           31, 31, 30, 31, 30, 31};
    int maxDay = daysInMonth[month - 1];
    const bool leapYear = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
    if (month == 2 && leapYear) {
        maxDay = 29;
    }
    return day <= maxDay;
}

inline bool IsValidTime(const wxString& value) {
    const wxString time = Trimmed(value);
    if (time.length() != 5 || time[2] != ':' ||
        !HasDigitsAt(time, 0, 2) || !HasDigitsAt(time, 3, 2)) {
        return false;
    }
    const int hour = TwoDigitNumber(time, 0);
    const int minute = TwoDigitNumber(time, 3);
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

// Empty input is accepted for optional fields; set required=true for mandatory fields.
inline bool ValidateDateField(const wxString& value, const wxString& fieldName,
                              bool required = false, wxString* outError = nullptr) {
    if (IsBlank(value)) {
        return required ? ValidateRequiredField(value, fieldName, outError) : true;
    }
    if (IsValidDate(value)) {
        return true;
    }
    if (outError) {
        *outError = fieldName + " must be a real date in YYYY-MM-DD format.";
    }
    return false;
}

// Empty input is accepted for optional fields; set required=true for mandatory fields.
inline bool ValidateTimeField(const wxString& value, const wxString& fieldName,
                              bool required = false, wxString* outError = nullptr) {
    if (IsBlank(value)) {
        return required ? ValidateRequiredField(value, fieldName, outError) : true;
    }
    if (IsValidTime(value)) {
        return true;
    }
    if (outError) {
        *outError = fieldName + " must be a valid 24-hour time in HH:MM format.";
    }
    return false;
}

// Meeting and activity start/end times are optional, but end cannot precede start.
inline bool ValidateTimeRange(const wxString& startTime, const wxString& endTime,
                              wxString* outError = nullptr,
                              const wxString& startField = "Start Time",
                              const wxString& endField = "End Time") {
    if (!ValidateTimeField(startTime, startField, false, outError) ||
        !ValidateTimeField(endTime, endField, false, outError)) {
        return false;
    }
    const wxString start = Trimmed(startTime);
    const wxString end = Trimmed(endTime);
    if (!start.IsEmpty() && !end.IsEmpty() && end.Cmp(start) < 0) {
        if (outError) {
            *outError = endField + " must be the same as or later than " + startField + ".";
        }
        return false;
    }
    return true;
}

// Project dates are optional. Each supplied date must be valid, and neither end
// date may precede the start date.
inline bool ValidateProjectDateRange(const wxString& startDate,
                                     const wxString& targetEndDate,
                                     const wxString& actualEndDate,
                                     wxString* outError = nullptr) {
    if (!ValidateDateField(startDate, "Start Date", false, outError) ||
        !ValidateDateField(targetEndDate, "Target End Date", false, outError) ||
        !ValidateDateField(actualEndDate, "Actual End Date", false, outError)) {
        return false;
    }

    const wxString start = Trimmed(startDate);
    const wxString target = Trimmed(targetEndDate);
    const wxString actual = Trimmed(actualEndDate);
    if (!start.IsEmpty() && !target.IsEmpty() && target.Cmp(start) < 0) {
        if (outError) {
            *outError = "Target End Date cannot be earlier than Start Date.";
        }
        return false;
    }
    if (!start.IsEmpty() && !actual.IsEmpty() && actual.Cmp(start) < 0) {
        if (outError) {
            *outError = "Actual End Date cannot be earlier than Start Date.";
        }
        return false;
    }
    return true;
}

// Appointment start date is required; end date is optional.
inline bool ValidateAppointmentDateRange(const wxString& startDate,
                                         const wxString& endDate,
                                         wxString* outError = nullptr) {
    if (!ValidateDateField(startDate, "Start Date", true, outError) ||
        !ValidateDateField(endDate, "End Date", false, outError)) {
        return false;
    }
    const wxString start = Trimmed(startDate);
    const wxString end = Trimmed(endDate);
    if (!end.IsEmpty() && end.Cmp(start) < 0) {
        if (outError) {
            *outError = "End Date cannot be earlier than Start Date.";
        }
        return false;
    }
    return true;
}

// Returns nullopt on query error, true when a duplicate is found, and false
// otherwise. Member-name duplicates are advisory, not a uniqueness constraint.
// Matching ignores surrounding spaces and ASCII case.
inline std::optional<bool> HasDuplicateMemberName(
    const DbService& db, const wxString& name,
    std::optional<std::int64_t> excludedMemberId = std::nullopt,
    wxString* outError = nullptr) {
    if (!db.IsOpen()) {
        if (outError) {
            *outError = "Cannot check duplicate member names because the database is not open.";
        }
        return std::nullopt;
    }
    if (IsBlank(name)) {
        return false;
    }

    auto statement = db.Prepare(
        "SELECT 1 FROM members "
        "WHERE lower(trim(name)) = lower(trim(?)) "
        "AND (? IS NULL OR member_id <> ?) LIMIT 1;",
        outError);
    if (!statement) {
        return std::nullopt;
    }
    if (!statement->BindText(1, Trimmed(name))) {
        if (outError) {
            *outError = "Could not bind the member name for duplicate checking.";
        }
        return std::nullopt;
    }
    const bool exclusionBound =
        excludedMemberId
            ? (statement->BindInt64(2, *excludedMemberId) &&
               statement->BindInt64(3, *excludedMemberId))
            : (statement->BindNull(2) && statement->BindNull(3));
    if (!exclusionBound) {
        if (outError) {
            *outError = "Could not bind the member ID for duplicate checking.";
        }
        return std::nullopt;
    }

    const int rc = sqlite3_step(statement->Get());
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    if (outError) {
        const char* message = sqlite3_errmsg(db.GetRawHandle());
        *outError = message ? wxString::FromUTF8(message)
                            : wxString("Could not check for duplicate member names.");
    }
    return std::nullopt;
}

// Returns nullopt on query error. This mirrors the database's UNIQUE(title, year)
// constraint; the caller must still handle the constraint when saving to cover races.
inline std::optional<bool> HasDuplicateCompetition(
    const DbService& db, const wxString& title, int year,
    std::optional<std::int64_t> excludedCompetitionId = std::nullopt,
    wxString* outError = nullptr) {
    if (!db.IsOpen()) {
        if (outError) {
            *outError = "Cannot check duplicate competitions because the database is not open.";
        }
        return std::nullopt;
    }
    if (IsBlank(title)) {
        return false;
    }

    auto statement = db.Prepare(
        "SELECT 1 FROM external_competitions "
        "WHERE title = ? AND year = ? "
        "AND (? IS NULL OR competition_id <> ?) LIMIT 1;",
        outError);
    if (!statement) {
        return std::nullopt;
    }
    if (!statement->BindText(1, title) || !statement->BindInt(2, year)) {
        if (outError) {
            *outError = "Could not bind competition details for duplicate checking.";
        }
        return std::nullopt;
    }
    const bool exclusionBound =
        excludedCompetitionId
            ? (statement->BindInt64(3, *excludedCompetitionId) &&
               statement->BindInt64(4, *excludedCompetitionId))
            : (statement->BindNull(3) && statement->BindNull(4));
    if (!exclusionBound) {
        if (outError) {
            *outError = "Could not bind the competition ID for duplicate checking.";
        }
        return std::nullopt;
    }

    const int rc = sqlite3_step(statement->Get());
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    if (outError) {
        const char* message = sqlite3_errmsg(db.GetRawHandle());
        *outError = message ? wxString::FromUTF8(message)
                            : wxString("Could not check for duplicate competitions.");
    }
    return std::nullopt;
}

// Use when a save to external_competitions fails with the unique constraint.
// The schema remains authoritative; a pre-check alone cannot eliminate races.
inline bool IsCompetitionDuplicateConstraint(int extendedErrorCode) {
    return extendedErrorCode == SQLITE_CONSTRAINT_UNIQUE;
}

inline wxString MemberDuplicateNameWarningMessage() {
    return "A member with this name already exists. Member names do not have to be unique.";
}

inline wxString CompetitionDuplicateMessage() {
    return "A competition with this title and year already exists. Choose a different title or year.";
}

} // namespace msms::validation

#endif // MSMS_VALIDATION_H
