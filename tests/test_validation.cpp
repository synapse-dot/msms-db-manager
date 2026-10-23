#include <catch2/catch_test_macros.hpp>

#include <wx/filefn.h>
#include <wx/filename.h>

#include "db/DbService.h"
#include "util/Validation.h"

namespace {

struct TemporaryDatabase {
    wxString path{wxFileName::CreateTempFileName("msms_validation_test_")};
    msms::DbService db;

    TemporaryDatabase() {
        if (wxFileExists(path)) {
            wxRemoveFile(path);
        }
    }

    ~TemporaryDatabase() {
        db.Close();
        if (wxFileExists(path)) {
            wxRemoveFile(path);
        }
    }
};

} // namespace

TEST_CASE("Date validation checks exact ISO format and real calendar dates",
          "[validation][date]") {
    using msms::validation::IsValidDate;
    using msms::validation::ValidateDateField;

    REQUIRE(IsValidDate("2024-02-29"));
    REQUIRE(IsValidDate("2000-02-29"));
    REQUIRE(IsValidDate("2026-10-23"));
    REQUIRE_FALSE(IsValidDate("2023-02-29"));
    REQUIRE_FALSE(IsValidDate("1900-02-29"));
    REQUIRE_FALSE(IsValidDate("2026-13-01"));
    REQUIRE_FALSE(IsValidDate("2026-04-31"));
    REQUIRE_FALSE(IsValidDate("2026-1-01"));
    REQUIRE_FALSE(IsValidDate("0000-01-01"));
    REQUIRE_FALSE(IsValidDate(""));

    wxString error;
    REQUIRE(ValidateDateField("", "Meeting Date", false, &error));
    REQUIRE_FALSE(ValidateDateField("", "Join Date", true, &error));
    REQUIRE(error.Contains("Join Date"));
    REQUIRE_FALSE(ValidateDateField("not-a-date", "Join Date", true, &error));
    REQUIRE(error.Contains("YYYY-MM-DD"));
}

TEST_CASE("Time validation accepts only 24-hour HH:MM values", "[validation][time]") {
    using msms::validation::IsValidTime;
    using msms::validation::ValidateTimeField;

    REQUIRE(IsValidTime("00:00"));
    REQUIRE(IsValidTime("09:05"));
    REQUIRE(IsValidTime("23:59"));
    REQUIRE_FALSE(IsValidTime("24:00"));
    REQUIRE_FALSE(IsValidTime("12:60"));
    REQUIRE_FALSE(IsValidTime("9:05"));
    REQUIRE_FALSE(IsValidTime("12:5"));
    REQUIRE_FALSE(IsValidTime("12.30"));
    REQUIRE_FALSE(IsValidTime(""));

    wxString error;
    REQUIRE(ValidateTimeField("", "Start Time", false, &error));
    REQUIRE_FALSE(ValidateTimeField("", "Start Time", true, &error));
    REQUIRE_FALSE(ValidateTimeField("25:00", "Start Time", false, &error));
    REQUIRE(error.Contains("HH:MM"));
}

TEST_CASE("Meeting and activity time range validation", "[validation][time][range]") {
    using msms::validation::ValidateTimeRange;

    wxString error;
    REQUIRE(ValidateTimeRange("09:00", "09:00", &error));
    REQUIRE(ValidateTimeRange("09:00", "10:30", &error));
    REQUIRE(ValidateTimeRange("", "10:30", &error));
    REQUIRE(ValidateTimeRange("09:00", "", &error));
    REQUIRE_FALSE(ValidateTimeRange("10:00", "09:59", &error));
    REQUIRE(error.Contains("End Time"));
    REQUIRE_FALSE(ValidateTimeRange("not-time", "11:00", &error));
}

TEST_CASE("Project dates validate each date and both date-order constraints",
          "[validation][date][project]") {
    using msms::validation::ValidateProjectDateRange;

    wxString error;
    REQUIRE(ValidateProjectDateRange("2026-01-01", "2026-01-01", "2026-01-01", &error));
    REQUIRE(ValidateProjectDateRange("", "2026-01-01", "", &error));
    REQUIRE(ValidateProjectDateRange("2026-01-01", "", "2026-01-02", &error));

    REQUIRE_FALSE(ValidateProjectDateRange("2026-02-01", "2026-01-31", "", &error));
    REQUIRE(error.Contains("Target End Date"));
    REQUIRE_FALSE(ValidateProjectDateRange("2026-02-01", "", "2026-01-31", &error));
    REQUIRE(error.Contains("Actual End Date"));
    REQUIRE_FALSE(ValidateProjectDateRange("2026-02-30", "", "", &error));
}

TEST_CASE("Committee appointment requires a start date and checks end-date ordering",
          "[validation][date][appointment]") {
    using msms::validation::ValidateAppointmentDateRange;

    wxString error;
    REQUIRE(ValidateAppointmentDateRange("2026-01-01", "", &error));
    REQUIRE(ValidateAppointmentDateRange("2026-01-01", "2026-01-01", &error));
    REQUIRE(ValidateAppointmentDateRange("2026-01-01", "2026-01-02", &error));
    REQUIRE_FALSE(ValidateAppointmentDateRange("", "", &error));
    REQUIRE(error.Contains("Start Date"));
    REQUIRE_FALSE(ValidateAppointmentDateRange("2026-02-01", "2026-01-31", &error));
    REQUIRE(error.Contains("End Date"));
}

TEST_CASE("Required fields reject empty and whitespace-only values", "[validation][required]") {
    using msms::validation::ValidateRequiredField;

    wxString error;
    REQUIRE(ValidateRequiredField("Felix", "Name", &error));
    REQUIRE_FALSE(ValidateRequiredField("", "Name", &error));
    REQUIRE(error.Contains("Name"));
    REQUIRE_FALSE(ValidateRequiredField("   \t ", "Title", &error));
    REQUIRE(error.Contains("Title"));
}

TEST_CASE("Member-name duplicate check is advisory and ignores case and surrounding spaces",
          "[validation][duplicates][member]") {
    using msms::validation::HasDuplicateMemberName;

    TemporaryDatabase temporary;
    wxString error;
    REQUIRE(temporary.db.CreateNew(temporary.path, &error));
    REQUIRE(temporary.db.Execute(
        "INSERT INTO members (name, join_date) VALUES ('Ada Lovelace', '2026-01-01');", &error));
    const auto memberId = temporary.db.GetLastInsertRowId();

    const auto duplicate = HasDuplicateMemberName(temporary.db, "  aDA LOVELACE  ",
                                                   std::nullopt, &error);
    REQUIRE(duplicate.has_value());
    REQUIRE(*duplicate);

    const auto excludingSelf =
        HasDuplicateMemberName(temporary.db, "Ada Lovelace", memberId, &error);
    REQUIRE(excludingSelf.has_value());
    REQUIRE_FALSE(*excludingSelf);

    const auto newName = HasDuplicateMemberName(temporary.db, "Grace Hopper", std::nullopt, &error);
    REQUIRE(newName.has_value());
    REQUIRE_FALSE(*newName);

    const auto blankName = HasDuplicateMemberName(temporary.db, "   ", std::nullopt, &error);
    REQUIRE(blankName.has_value());
    REQUIRE_FALSE(*blankName);
    REQUIRE(msms::validation::MemberDuplicateNameWarningMessage().Contains("not have to be unique"));
}

TEST_CASE("Competition duplicate check mirrors UNIQUE title/year and can exclude the edited row",
          "[validation][duplicates][competition]") {
    using msms::validation::HasDuplicateCompetition;

    TemporaryDatabase temporary;
    wxString error;
    REQUIRE(temporary.db.CreateNew(temporary.path, &error));
    REQUIRE(temporary.db.Execute(
        "INSERT INTO external_competitions (title, year) VALUES ('Modelling Challenge', 2026);",
        &error));
    const auto competitionId = temporary.db.GetLastInsertRowId();

    const auto duplicate =
        HasDuplicateCompetition(temporary.db, "Modelling Challenge", 2026, std::nullopt, &error);
    REQUIRE(duplicate.has_value());
    REQUIRE(*duplicate);

    const auto excludingSelf = HasDuplicateCompetition(
        temporary.db, "Modelling Challenge", 2026, competitionId, &error);
    REQUIRE(excludingSelf.has_value());
    REQUIRE_FALSE(*excludingSelf);

    const auto otherYear =
        HasDuplicateCompetition(temporary.db, "Modelling Challenge", 2027, std::nullopt, &error);
    REQUIRE(otherYear.has_value());
    REQUIRE_FALSE(*otherYear);

    REQUIRE(msms::validation::IsCompetitionDuplicateConstraint(SQLITE_CONSTRAINT_UNIQUE));
    REQUIRE_FALSE(msms::validation::IsCompetitionDuplicateConstraint(SQLITE_CONSTRAINT_CHECK));
    REQUIRE(msms::validation::CompetitionDuplicateMessage().Contains("title and year"));
}
