#include "forms/RecordForms.h"

#include <limits>
#include <memory>
#include <utility>

#include <wx/arrstr.h>
#include <wx/msgdlg.h>

#include "util/ErrorReporter.h"
#include "util/Validation.h"

namespace msms {
namespace {

RecordFormOptions MakeOptions(const wxString& entity, const wxString& deleteTable,
                              RecordFormMode mode, DbService& db,
                              std::optional<std::int64_t> recordId,
                              const wxString& compositeKeyDescription,
                              std::function<void()> onReturnToParent) {
    RecordFormOptions options;
    options.title = (mode == RecordFormMode::Edit ? "Edit " : "New ") + entity;
    options.mode = mode;
    options.database = &db;
    options.deleteTable = deleteTable;
    options.recordId = recordId;
    options.compositeKeyDescription = compositeKeyDescription;
    options.onReturnToParent = std::move(onReturnToParent);
    return options;
}

wxArrayString Choices(std::initializer_list<const char*> values) {
    wxArrayString result;
    for (const char* value : values) {
        result.Add(wxString::FromUTF8(value));
    }
    return result;
}

wxString Value(const wxTextCtrl* control) {
    return control ? validation::Trimmed(control->GetValue()) : wxString{};
}

void SetText(wxTextCtrl* control, const wxString& value) {
    if (control) {
        control->ChangeValue(value);
    }
}

bool Fail(wxString* outError, const wxString& message) {
    if (outError) {
        *outError = message;
    }
    return false;
}

wxString DatabaseError(const DbService& db, const wxString& context) {
    return ErrorReporter::Report(db, context).FormatForDisplay();
}

template <typename Binder>
std::unique_ptr<Statement> LoadSingleRow(DbService& db, const wxString& sql, Binder binder,
                                         const wxString& missingMessage,
                                         const wxString& context, wxString* outError) {
    wxString prepareError;
    auto statement = db.Prepare(sql, &prepareError);
    if (!statement) {
        Fail(outError, wxString::Format("Could not %s:\n%s", context, prepareError));
        return nullptr;
    }

    if (!binder(*statement)) {
        Fail(outError, wxString::Format("Could not bind the lookup for %s.", context));
        return nullptr;
    }

    const int rc = sqlite3_step(statement->Get());
    if (rc == SQLITE_ROW) {
        return statement;
    }
    if (rc == SQLITE_DONE) {
        Fail(outError, missingMessage);
    } else {
        Fail(outError, DatabaseError(db, context));
    }
    return nullptr;
}

template <typename Binder>
bool ExecuteWrite(DbService& db, const wxString& sql, Binder binder,
                  const wxString& context, wxString* outError) {
    wxString prepareError;
    auto statement = db.Prepare(sql, &prepareError);
    if (!statement) {
        return Fail(outError, wxString::Format("Could not prepare the database write:\n%s",
                                               prepareError));
    }

    if (!binder(*statement)) {
        return Fail(outError, wxString::Format("Could not bind values while %s.",
                                               context));
    }

    const int rc = sqlite3_step(statement->Get());
    if (rc == SQLITE_DONE) {
        return true;
    }

    return Fail(outError, DatabaseError(db, context));
}

bool BindOptionalText(Statement& statement, int index, const wxString& value) {
    const wxString trimmed = validation::Trimmed(value);
    return trimmed.IsEmpty() ? statement.BindNull(index) : statement.BindText(index, trimmed);
}

bool ValidateChoice(const wxChoice* choice, const wxString& fieldName, wxString* outError) {
    if (!choice || choice->GetSelection() == wxNOT_FOUND) {
        return Fail(outError, "Please choose a value for " + fieldName + ".");
    }
    return true;
}

bool ValidateLoaded(bool loaded, const wxString& loadError, wxString* outError) {
    if (loaded) {
        return true;
    }
    return Fail(outError, loadError.IsEmpty()
                              ? "The selected record could not be loaded. Cancel and reopen it."
                              : loadError);
}

void ShowLoadError(wxWindow* parent, const wxString& entity, const wxString& error) {
    wxMessageBox(wxString("Could not load this ") + entity + " record:\n\n" + error,
                 "Could Not Load Record", wxOK | wxICON_ERROR, parent);
}

wxString AppointmentKeyDescription(const std::optional<wxString>& recordKey) {
    if (!recordKey) {
        return {};
    }
    const int separator = recordKey->Find(static_cast<wxChar>(0x1f));
    if (separator == wxNOT_FOUND) {
        return *recordKey;
    }
    const wxString date = recordKey->Left(separator);
    const wxString office = recordKey->Mid(separator + 1);
    return wxString::Format("start_date='%s', office='%s'", date, office);
}

bool SplitAppointmentKey(const wxString& key, wxString* startDate, wxString* office) {
    const int separator = key.Find(static_cast<wxChar>(0x1f));
    if (separator == wxNOT_FOUND) {
        return false;
    }
    *startDate = key.Left(separator);
    *office = key.Mid(separator + 1);
    return !startDate->IsEmpty() && !office->IsEmpty();
}

bool LoadActivityTypes(DbService& db, wxArrayString* types, wxString* outError) {
    auto statement = db.Prepare(
        "SELECT DISTINCT type FROM activities "
        "WHERE type IS NOT NULL AND trim(type) <> '' "
        "ORDER BY type COLLATE NOCASE ASC;",
        outError);
    if (!statement) {
        return false;
    }

    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(statement->Get())) == SQLITE_ROW) {
        const wxString type = statement->GetText(0);
        if (types->Index(type) == wxNOT_FOUND) {
            types->Add(type);
        }
    }
    if (rc != SQLITE_DONE) {
        return Fail(outError, DatabaseError(db, "loading existing activity types"));
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Member
// ---------------------------------------------------------------------------

MemberForm::MemberForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                       std::optional<std::int64_t> recordId,
                       std::function<void()> onReturnToParent)
    : RecordForm(parent, MakeOptions("Member", "members", mode, db, recordId, {},
                                     std::move(onReturnToParent))),
      m_db(db), m_id(recordId) {
    m_name = AddTextField("Name *");
    m_class = AddTextField("Class");
    m_phone = AddTextField("Phone");
    m_email = AddTextField("Email");
    m_status = AddChoiceField("Status *", Choices({"Active", "Former"}), 0);
    m_joinDate = AddTextField("Join Date *");
    m_joinDate->SetHint("YYYY-MM-DD");

    if (IsEditMode()) {
        if (!m_id) {
            m_recordLoaded = false;
            m_loadError = "The member ID is missing. Cancel and reopen the member.";
        } else if (!LoadRecord(&m_loadError)) {
            m_recordLoaded = false;
        }
    }

    MarkClean();
    if (!m_recordLoaded) {
        ShowLoadError(this, "member", m_loadError);
    }
}

bool MemberForm::LoadRecord(wxString* outError) {
    auto statement = LoadSingleRow(
        m_db,
        "SELECT name, class, phone, email, status, join_date "
        "FROM members WHERE member_id = ?;",
        [this](Statement& value) { return value.BindInt64(1, *m_id); },
        "This member no longer exists. Return to All Members and refresh the table.",
        "loading the member", outError);
    if (!statement) {
        return false;
    }

    SetText(m_name, statement->GetText(0));
    SetText(m_class, statement->GetText(1));
    SetText(m_phone, statement->GetText(2));
    SetText(m_email, statement->GetText(3));
    m_status->SetStringSelection(statement->GetText(4));
    SetText(m_joinDate, statement->GetText(5));
    return true;
}

bool MemberForm::ValidateFields(wxString* outError) const {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError) ||
        !validation::ValidateRequiredField(Value(m_name), "Name", outError) ||
        !validation::ValidateDateField(Value(m_joinDate), "Join Date", true, outError) ||
        !ValidateChoice(m_status, "Status", outError)) {
        return false;
    }

    // Duplicate names are deliberately advisory; they never block a save.
    (void)validation::CheckMemberNameAndWarn(
        const_cast<MemberForm*>(this), m_db, Value(m_name), m_id, nullptr);
    return true;
}

bool MemberForm::SaveRecord(wxString* outError) {
    const wxString name = Value(m_name);
    const wxString className = Value(m_class);
    const wxString phone = Value(m_phone);
    const wxString email = Value(m_email);
    const wxString status = m_status->GetStringSelection();
    const wxString joinDate = Value(m_joinDate);

    if (IsEditMode()) {
        return ExecuteWrite(
            m_db,
            "UPDATE members SET name = ?, class = ?, phone = ?, email = ?, status = ?, "
            "join_date = ? WHERE member_id = ?;",
            [&](Statement& value) {
                return value.BindText(1, name) &&
                       BindOptionalText(value, 2, className) &&
                       BindOptionalText(value, 3, phone) &&
                       BindOptionalText(value, 4, email) &&
                       value.BindText(5, status) &&
                       value.BindText(6, joinDate) &&
                       value.BindInt64(7, *m_id);
            },
            "saving the member", outError);
    }

    return ExecuteWrite(
        m_db,
        "INSERT INTO members (name, class, phone, email, status, join_date) "
        "VALUES (?, ?, ?, ?, ?, ?);",
        [&](Statement& value) {
            return value.BindText(1, name) &&
                   BindOptionalText(value, 2, className) &&
                   BindOptionalText(value, 3, phone) &&
                   BindOptionalText(value, 4, email) &&
                   value.BindText(5, status) &&
                   value.BindText(6, joinDate);
        },
        "saving the member", outError);
}

bool MemberForm::DeleteRecord(wxString* outError) {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError)) {
        return false;
    }
    return ExecuteWrite(m_db, "DELETE FROM members WHERE member_id = ?;",
                        [this](Statement& value) { return value.BindInt64(1, *m_id); },
                        "deleting the member", outError);
}

// ---------------------------------------------------------------------------
// Meeting
// ---------------------------------------------------------------------------

MeetingForm::MeetingForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                         std::optional<std::int64_t> recordId,
                         std::function<void()> onReturnToParent)
    : RecordForm(parent, MakeOptions("Meeting", "meetings", mode, db, recordId, {},
                                     std::move(onReturnToParent))),
      m_db(db), m_id(recordId) {
    m_title = AddTextField("Title *");
    m_type = AddChoiceField("Type *", Choices({"AGM", "EGM", "ECM"}), 0);
    m_date = AddTextField("Date");
    m_date->SetHint("YYYY-MM-DD");
    m_startTime = AddTextField("Start Time");
    m_startTime->SetHint("HH:MM");
    m_endTime = AddTextField("End Time");
    m_endTime->SetHint("HH:MM");
    m_location = AddTextField("Location");

    if (IsEditMode()) {
        if (!m_id) {
            m_recordLoaded = false;
            m_loadError = "The meeting ID is missing. Cancel and reopen the meeting.";
        } else if (!LoadRecord(&m_loadError)) {
            m_recordLoaded = false;
        }
    }

    MarkClean();
    if (!m_recordLoaded) {
        ShowLoadError(this, "meeting", m_loadError);
    }
}

bool MeetingForm::LoadRecord(wxString* outError) {
    auto statement = LoadSingleRow(
        m_db,
        "SELECT title, type, meeting_date, start_time, end_time, location "
        "FROM meetings WHERE meeting_id = ?;",
        [this](Statement& value) { return value.BindInt64(1, *m_id); },
        "This meeting no longer exists. Return to All Meetings and refresh the table.",
        "loading the meeting", outError);
    if (!statement) {
        return false;
    }
    SetText(m_title, statement->GetText(0));
    m_type->SetStringSelection(statement->GetText(1));
    SetText(m_date, statement->GetText(2));
    SetText(m_startTime, statement->GetText(3));
    SetText(m_endTime, statement->GetText(4));
    SetText(m_location, statement->GetText(5));
    return true;
}

bool MeetingForm::ValidateFields(wxString* outError) const {
    return ValidateLoaded(m_recordLoaded, m_loadError, outError) &&
           validation::ValidateRequiredField(Value(m_title), "Title", outError) &&
           ValidateChoice(m_type, "Type", outError) &&
           validation::ValidateDateField(Value(m_date), "Date", false, outError) &&
           validation::ValidateTimeField(Value(m_startTime), "Start Time", false, outError) &&
           validation::ValidateTimeField(Value(m_endTime), "End Time", false, outError) &&
           validation::ValidateTimeRange(Value(m_startTime), Value(m_endTime), outError);
}

bool MeetingForm::SaveRecord(wxString* outError) {
    const wxString title = Value(m_title);
    const wxString type = m_type->GetStringSelection();
    const wxString date = Value(m_date);
    const wxString startTime = Value(m_startTime);
    const wxString endTime = Value(m_endTime);
    const wxString location = Value(m_location);

    if (IsEditMode()) {
        return ExecuteWrite(
            m_db,
            "UPDATE meetings SET title = ?, type = ?, meeting_date = ?, start_time = ?, "
            "end_time = ?, location = ? WHERE meeting_id = ?;",
            [&](Statement& value) {
                return value.BindText(1, title) && value.BindText(2, type) &&
                       BindOptionalText(value, 3, date) &&
                       BindOptionalText(value, 4, startTime) &&
                       BindOptionalText(value, 5, endTime) &&
                       BindOptionalText(value, 6, location) &&
                       value.BindInt64(7, *m_id);
            },
            "saving the meeting", outError);
    }

    return ExecuteWrite(
        m_db,
        "INSERT INTO meetings (title, type, meeting_date, start_time, end_time, location) "
        "VALUES (?, ?, ?, ?, ?, ?);",
        [&](Statement& value) {
            return value.BindText(1, title) && value.BindText(2, type) &&
                   BindOptionalText(value, 3, date) &&
                   BindOptionalText(value, 4, startTime) &&
                   BindOptionalText(value, 5, endTime) &&
                   BindOptionalText(value, 6, location);
        },
        "saving the meeting", outError);
}

bool MeetingForm::DeleteRecord(wxString* outError) {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError)) {
        return false;
    }
    return ExecuteWrite(m_db, "DELETE FROM meetings WHERE meeting_id = ?;",
                        [this](Statement& value) { return value.BindInt64(1, *m_id); },
                        "deleting the meeting", outError);
}

// ---------------------------------------------------------------------------
// Project
// ---------------------------------------------------------------------------

ProjectForm::ProjectForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                         std::optional<std::int64_t> recordId,
                         std::function<void()> onReturnToParent)
    : RecordForm(parent, MakeOptions("Project", "projects", mode, db, recordId, {},
                                     std::move(onReturnToParent))),
      m_db(db), m_id(recordId) {
    m_title = AddTextField("Title *");
    m_status = AddChoiceField("Status *",
                              Choices({"Planned", "Ongoing", "Completed", "Cancelled"}), 0);
    m_startDate = AddTextField("Start Date");
    m_startDate->SetHint("YYYY-MM-DD");
    m_targetEndDate = AddTextField("Target End Date");
    m_targetEndDate->SetHint("YYYY-MM-DD");
    m_actualEndDate = AddTextField("Actual End Date");
    m_actualEndDate->SetHint("YYYY-MM-DD");

    if (IsEditMode()) {
        if (!m_id) {
            m_recordLoaded = false;
            m_loadError = "The project ID is missing. Cancel and reopen the project.";
        } else if (!LoadRecord(&m_loadError)) {
            m_recordLoaded = false;
        }
    }

    MarkClean();
    if (!m_recordLoaded) {
        ShowLoadError(this, "project", m_loadError);
    }
}

bool ProjectForm::LoadRecord(wxString* outError) {
    auto statement = LoadSingleRow(
        m_db,
        "SELECT title, status, start_date, target_end_date, actual_end_date "
        "FROM projects WHERE project_id = ?;",
        [this](Statement& value) { return value.BindInt64(1, *m_id); },
        "This project no longer exists. Return to All Projects and refresh the table.",
        "loading the project", outError);
    if (!statement) {
        return false;
    }
    SetText(m_title, statement->GetText(0));
    m_status->SetStringSelection(statement->GetText(1));
    SetText(m_startDate, statement->GetText(2));
    SetText(m_targetEndDate, statement->GetText(3));
    SetText(m_actualEndDate, statement->GetText(4));
    return true;
}

bool ProjectForm::ValidateFields(wxString* outError) const {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError) ||
        !validation::ValidateRequiredField(Value(m_title), "Title", outError) ||
        !ValidateChoice(m_status, "Status", outError) ||
        !validation::ValidateDateField(Value(m_startDate), "Start Date", false, outError) ||
        !validation::ValidateDateField(Value(m_targetEndDate), "Target End Date", false, outError) ||
        !validation::ValidateDateField(Value(m_actualEndDate), "Actual End Date", false, outError)) {
        return false;
    }
    return validation::ValidateProjectDateRange(
        Value(m_startDate), Value(m_targetEndDate), Value(m_actualEndDate), outError);
}

bool ProjectForm::SaveRecord(wxString* outError) {
    const wxString title = Value(m_title);
    const wxString status = m_status->GetStringSelection();
    const wxString startDate = Value(m_startDate);
    const wxString targetEndDate = Value(m_targetEndDate);
    const wxString actualEndDate = Value(m_actualEndDate);

    if (IsEditMode()) {
        return ExecuteWrite(
            m_db,
            "UPDATE projects SET title = ?, status = ?, start_date = ?, target_end_date = ?, "
            "actual_end_date = ? WHERE project_id = ?;",
            [&](Statement& value) {
                return value.BindText(1, title) && value.BindText(2, status) &&
                       BindOptionalText(value, 3, startDate) &&
                       BindOptionalText(value, 4, targetEndDate) &&
                       BindOptionalText(value, 5, actualEndDate) &&
                       value.BindInt64(6, *m_id);
            },
            "saving the project", outError);
    }

    return ExecuteWrite(
        m_db,
        "INSERT INTO projects (title, status, start_date, target_end_date, actual_end_date) "
        "VALUES (?, ?, ?, ?, ?);",
        [&](Statement& value) {
            return value.BindText(1, title) && value.BindText(2, status) &&
                   BindOptionalText(value, 3, startDate) &&
                   BindOptionalText(value, 4, targetEndDate) &&
                   BindOptionalText(value, 5, actualEndDate);
        },
        "saving the project", outError);
}

bool ProjectForm::DeleteRecord(wxString* outError) {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError)) {
        return false;
    }
    return ExecuteWrite(m_db, "DELETE FROM projects WHERE project_id = ?;",
                        [this](Statement& value) { return value.BindInt64(1, *m_id); },
                        "deleting the project", outError);
}

// ---------------------------------------------------------------------------
// Committee appointment
// ---------------------------------------------------------------------------

AppointmentForm::AppointmentForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                                 const std::optional<wxString>& recordKey,
                                 std::function<void()> onReturnToParent)
    : RecordForm(parent,
                 MakeOptions("Appointment", "committee_appointments", mode, db, std::nullopt,
                             AppointmentKeyDescription(recordKey), std::move(onReturnToParent))),
      m_db(db) {
    if (IsEditMode()) {
        if (!recordKey || !SplitAppointmentKey(*recordKey, &m_startDateKey, &m_officeKey)) {
            m_recordLoaded = false;
            m_loadError =
                "The appointment key is invalid. Return to All Appointments and reopen the record.";
        } else if (!LoadRecord(&m_loadError)) {
            m_recordLoaded = false;
        }
    }

    m_office = AddTextField("Office *", m_officeKey);
    auto* memberPanelChoices = new wxArrayString();
    wxString memberError;
    if (!PopulateMemberChoices(m_assignedMemberId, memberPanelChoices, &memberError)) {
        m_recordLoaded = false;
        if (m_loadError.IsEmpty()) {
            m_loadError = memberError;
        }
    }
    m_member = AddChoiceField("Member *", *memberPanelChoices, wxNOT_FOUND);
    delete memberPanelChoices;

    if (m_assignedMemberId) {
        const auto found = std::find(m_memberIds.begin(), m_memberIds.end(), *m_assignedMemberId);
        if (found != m_memberIds.end()) {
            m_member->SetSelection(static_cast<int>(std::distance(m_memberIds.begin(), found)));
        }
    }

    m_startDate = AddTextField("Start Date *", m_startDateKey);
    m_startDate->SetHint("YYYY-MM-DD");
    m_endDate = AddTextField("End Date", m_initialEndDate);
    m_endDate->SetHint("YYYY-MM-DD");

    if (IsEditMode()) {
        // The composite primary key is immutable during edit; Delete + New is
        // the intentional route to a different office/start-date identity.
        m_office->Enable(false);
        m_startDate->Enable(false);
    }

    MarkClean();
    if (!m_recordLoaded) {
        ShowLoadError(this, "appointment", m_loadError);
    }
}

bool AppointmentForm::LoadRecord(wxString* outError) {
    auto statement = LoadSingleRow(
        m_db,
        "SELECT member_id, end_date FROM committee_appointments "
        "WHERE start_date = ? AND office = ?;",
        [this](Statement& value) {
            return value.BindText(1, m_startDateKey) && value.BindText(2, m_officeKey);
        },
        "This appointment no longer exists. Return to All Appointments and refresh the table.",
        "loading the appointment", outError);
    if (!statement) {
        return false;
    }
    m_assignedMemberId = statement->GetInt64(0);
    m_initialEndDate = statement->GetText(1);
    return true;
}

bool AppointmentForm::PopulateMemberChoices(std::optional<std::int64_t> assignedMemberId,
                                            wxArrayString* labels, wxString* outError) {
    auto statement = m_db.Prepare(
        "SELECT member_id, name, class, status FROM members "
        "WHERE status = 'Active' OR member_id = ? "
        "ORDER BY name COLLATE NOCASE ASC, member_id ASC;",
        outError);
    if (!statement) {
        return false;
    }
    if (assignedMemberId) {
        if (!statement->BindInt64(1, *assignedMemberId)) {
            return Fail(outError, "Could not bind the current appointment member.");
        }
    } else if (!statement->BindNull(1)) {
        return Fail(outError, "Could not prepare the active-member list.");
    }

    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(statement->Get())) == SQLITE_ROW) {
        const std::int64_t id = statement->GetInt64(0);
        const wxString name = statement->GetText(1);
        const wxString className = validation::Trimmed(statement->GetText(2));
        const wxString status = statement->GetText(3);

        wxString label = name;
        if (!className.IsEmpty()) {
            label += " (" + className + ")";
        }
        if (status == "Former") {
            label += " (Former)";
        }
        labels->Add(label);
        m_memberIds.push_back(id);
    }
    if (rc != SQLITE_DONE) {
        return Fail(outError, DatabaseError(m_db, "loading active members"));
    }
    return true;
}

bool AppointmentForm::ValidateFields(wxString* outError) const {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError) ||
        !validation::ValidateRequiredField(Value(m_office), "Office", outError) ||
        !validation::ValidateDateField(Value(m_startDate), "Start Date", true, outError) ||
        !validation::ValidateDateField(Value(m_endDate), "End Date", false, outError) ||
        !validation::ValidateAppointmentDateRange(Value(m_startDate), Value(m_endDate), outError)) {
        return false;
    }

    const int selection = m_member ? m_member->GetSelection() : wxNOT_FOUND;
    if (selection == wxNOT_FOUND || selection < 0 ||
        static_cast<std::size_t>(selection) >= m_memberIds.size()) {
        return Fail(outError, "Please choose a member for this appointment.");
    }
    return true;
}

bool AppointmentForm::SaveRecord(wxString* outError) {
    const int selection = m_member->GetSelection();
    if (selection < 0 || static_cast<std::size_t>(selection) >= m_memberIds.size()) {
        return Fail(outError, "Please choose a member for this appointment.");
    }
    const std::int64_t memberId = m_memberIds[static_cast<std::size_t>(selection)];
    const wxString office = IsEditMode() ? m_officeKey : Value(m_office);
    const wxString startDate = IsEditMode() ? m_startDateKey : Value(m_startDate);
    const wxString endDate = Value(m_endDate);

    if (IsEditMode()) {
        return ExecuteWrite(
            m_db,
            "UPDATE committee_appointments SET member_id = ?, end_date = ? "
            "WHERE start_date = ? AND office = ?;",
            [&](Statement& value) {
                return value.BindInt64(1, memberId) && BindOptionalText(value, 2, endDate) &&
                       value.BindText(3, m_startDateKey) && value.BindText(4, m_officeKey);
            },
            "saving the appointment", outError);
    }

    return ExecuteWrite(
        m_db,
        "INSERT INTO committee_appointments (start_date, office, member_id, end_date) "
        "VALUES (?, ?, ?, ?);",
        [&](Statement& value) {
            return value.BindText(1, startDate) && value.BindText(2, office) &&
                   value.BindInt64(3, memberId) && BindOptionalText(value, 4, endDate);
        },
        "saving the appointment", outError);
}

bool AppointmentForm::DeleteRecord(wxString* outError) {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError)) {
        return false;
    }
    return ExecuteWrite(
        m_db,
        "DELETE FROM committee_appointments WHERE start_date = ? AND office = ?;",
        [this](Statement& value) {
            return value.BindText(1, m_startDateKey) && value.BindText(2, m_officeKey);
        },
        "deleting the appointment", outError);
}

// ---------------------------------------------------------------------------
// Activity
// ---------------------------------------------------------------------------

ActivityForm::ActivityForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                           std::optional<std::int64_t> recordId,
                           std::function<void()> onReturnToParent)
    : RecordForm(parent, MakeOptions("Activity", "activities", mode, db, recordId, {},
                                     std::move(onReturnToParent))),
      m_db(db), m_id(recordId) {
    m_title = AddTextField("Title *");

    wxArrayString activityTypes;
    wxString typeError;
    if (!LoadActivityTypes(m_db, &activityTypes, &typeError)) {
        // The type is free text; a lookup failure need not prevent entry of a
        // new type, but make the issue visible to the user.
        wxMessageBox("Existing activity types could not be loaded:\n\n" + typeError,
                     "Activity Types", wxOK | wxICON_WARNING, this);
    }
    m_type = AddComboField("Type *", activityTypes);
    m_date = AddTextField("Date");
    m_date->SetHint("YYYY-MM-DD");
    m_startTime = AddTextField("Start Time");
    m_startTime->SetHint("HH:MM");
    m_endTime = AddTextField("End Time");
    m_endTime->SetHint("HH:MM");
    m_location = AddTextField("Location");

    if (IsEditMode()) {
        if (!m_id) {
            m_recordLoaded = false;
            m_loadError = "The activity ID is missing. Cancel and reopen the activity.";
        } else if (!LoadRecord(&m_loadError)) {
            m_recordLoaded = false;
        }
    }

    MarkClean();
    if (!m_recordLoaded) {
        ShowLoadError(this, "activity", m_loadError);
    }
}

bool ActivityForm::LoadRecord(wxString* outError) {
    auto statement = LoadSingleRow(
        m_db,
        "SELECT title, type, activity_date, start_time, end_time, location "
        "FROM activities WHERE activity_id = ?;",
        [this](Statement& value) { return value.BindInt64(1, *m_id); },
        "This activity no longer exists. Return to All Activities and refresh the table.",
        "loading the activity", outError);
    if (!statement) {
        return false;
    }
    SetText(m_title, statement->GetText(0));
    m_type->ChangeValue(statement->GetText(1));
    SetText(m_date, statement->GetText(2));
    SetText(m_startTime, statement->GetText(3));
    SetText(m_endTime, statement->GetText(4));
    SetText(m_location, statement->GetText(5));
    return true;
}

bool ActivityForm::ValidateFields(wxString* outError) const {
    return ValidateLoaded(m_recordLoaded, m_loadError, outError) &&
           validation::ValidateRequiredField(Value(m_title), "Title", outError) &&
           validation::ValidateRequiredField(validation::Trimmed(m_type->GetValue()), "Type",
                                              outError) &&
           validation::ValidateDateField(Value(m_date), "Date", false, outError) &&
           validation::ValidateTimeField(Value(m_startTime), "Start Time", false, outError) &&
           validation::ValidateTimeField(Value(m_endTime), "End Time", false, outError) &&
           validation::ValidateTimeRange(Value(m_startTime), Value(m_endTime), outError);
}

bool ActivityForm::SaveRecord(wxString* outError) {
    const wxString title = Value(m_title);
    const wxString type = validation::Trimmed(m_type->GetValue());
    const wxString date = Value(m_date);
    const wxString startTime = Value(m_startTime);
    const wxString endTime = Value(m_endTime);
    const wxString location = Value(m_location);

    if (IsEditMode()) {
        return ExecuteWrite(
            m_db,
            "UPDATE activities SET title = ?, type = ?, activity_date = ?, start_time = ?, "
            "end_time = ?, location = ? WHERE activity_id = ?;",
            [&](Statement& value) {
                return value.BindText(1, title) && value.BindText(2, type) &&
                       BindOptionalText(value, 3, date) &&
                       BindOptionalText(value, 4, startTime) &&
                       BindOptionalText(value, 5, endTime) &&
                       BindOptionalText(value, 6, location) &&
                       value.BindInt64(7, *m_id);
            },
            "saving the activity", outError);
    }

    return ExecuteWrite(
        m_db,
        "INSERT INTO activities (title, type, activity_date, start_time, end_time, location) "
        "VALUES (?, ?, ?, ?, ?, ?);",
        [&](Statement& value) {
            return value.BindText(1, title) && value.BindText(2, type) &&
                   BindOptionalText(value, 3, date) &&
                   BindOptionalText(value, 4, startTime) &&
                   BindOptionalText(value, 5, endTime) &&
                   BindOptionalText(value, 6, location);
        },
        "saving the activity", outError);
}

bool ActivityForm::DeleteRecord(wxString* outError) {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError)) {
        return false;
    }
    return ExecuteWrite(m_db, "DELETE FROM activities WHERE activity_id = ?;",
                        [this](Statement& value) { return value.BindInt64(1, *m_id); },
                        "deleting the activity", outError);
}

// ---------------------------------------------------------------------------
// External competition
// ---------------------------------------------------------------------------

CompetitionForm::CompetitionForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                                 std::optional<std::int64_t> recordId,
                                 std::function<void()> onReturnToParent)
    : RecordForm(parent, MakeOptions("Competition", "external_competitions", mode, db, recordId, {},
                                     std::move(onReturnToParent))),
      m_db(db), m_id(recordId) {
    m_title = AddTextField("Title *");
    m_year = AddTextField("Year *");

    if (IsEditMode()) {
        if (!m_id) {
            m_recordLoaded = false;
            m_loadError = "The competition ID is missing. Cancel and reopen the competition.";
        } else if (!LoadRecord(&m_loadError)) {
            m_recordLoaded = false;
        }
    }

    MarkClean();
    if (!m_recordLoaded) {
        ShowLoadError(this, "competition", m_loadError);
    }
}

bool CompetitionForm::LoadRecord(wxString* outError) {
    auto statement = LoadSingleRow(
        m_db,
        "SELECT title, year FROM external_competitions WHERE competition_id = ?;",
        [this](Statement& value) { return value.BindInt64(1, *m_id); },
        "This competition no longer exists. Return to All Competitions and refresh the table.",
        "loading the competition", outError);
    if (!statement) {
        return false;
    }
    SetText(m_title, statement->GetText(0));
    SetText(m_year, statement->GetText(1));
    return true;
}

bool CompetitionForm::ValidateFields(wxString* outError) const {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError) ||
        !validation::ValidateRequiredField(Value(m_title), "Title", outError) ||
        !validation::ValidateRequiredField(Value(m_year), "Year", outError)) {
        return false;
    }

    long year = 0;
    const wxString yearText = Value(m_year);
    if (!yearText.ToLong(&year) || year <= 0 ||
        year > static_cast<long>(std::numeric_limits<int>::max())) {
        return Fail(outError, "Year must be a positive whole number.");
    }
    return true;
}

bool CompetitionForm::SaveRecord(wxString* outError) {
    const wxString title = Value(m_title);
    long yearValue = 0;
    if (!Value(m_year).ToLong(&yearValue) || yearValue <= 0 ||
        yearValue > static_cast<long>(std::numeric_limits<int>::max())) {
        return Fail(outError, "Year must be a positive whole number.");
    }
    const int year = static_cast<int>(yearValue);

    if (IsEditMode()) {
        return ExecuteWrite(
            m_db,
            "UPDATE external_competitions SET title = ?, year = ? WHERE competition_id = ?;",
            [&](Statement& value) {
                return value.BindText(1, title) && value.BindInt(2, year) &&
                       value.BindInt64(3, *m_id);
            },
            "saving the competition", outError);
    }

    return ExecuteWrite(
        m_db,
        "INSERT INTO external_competitions (title, year) VALUES (?, ?);",
        [&](Statement& value) { return value.BindText(1, title) && value.BindInt(2, year); },
        "saving the competition", outError);
}

bool CompetitionForm::DeleteRecord(wxString* outError) {
    if (!ValidateLoaded(m_recordLoaded, m_loadError, outError)) {
        return false;
    }
    return ExecuteWrite(
        m_db, "DELETE FROM external_competitions WHERE competition_id = ?;",
        [this](Statement& value) { return value.BindInt64(1, *m_id); },
        "deleting the competition", outError);
}

} // namespace msms
