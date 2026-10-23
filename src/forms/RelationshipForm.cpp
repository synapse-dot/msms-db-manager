#include "forms/RelationshipForm.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>

#include <sqlite3.h>

#include "util/ErrorReporter.h"

namespace msms {
namespace {

struct RelationshipSql {
    wxString assignmentTable;
    wxString parentIdColumn;
    wxString memberIdColumn;
    wxString selectSql;
    wxString deleteSql;
    wxString insertSql;
    wxString updateRoleSql;
};

RelationshipSql SqlFor(RelationshipKind kind) {
    switch (kind) {
    case RelationshipKind::MeetingAttendance:
        return {
            "meeting_attendance", "meeting_id", "member_id",
            "SELECT member_id FROM meeting_attendance WHERE meeting_id = ?;",
            "DELETE FROM meeting_attendance WHERE meeting_id = ? AND member_id = ?;",
            "INSERT INTO meeting_attendance (meeting_id, member_id) VALUES (?, ?);",
            wxString{}};
    case RelationshipKind::ProjectMembers:
        return {
            "project_members", "project_id", "member_id",
            "SELECT member_id, role FROM project_members WHERE project_id = ?;",
            "DELETE FROM project_members WHERE project_id = ? AND member_id = ?;",
            "INSERT INTO project_members (project_id, member_id, role) VALUES (?, ?, ?);",
            "UPDATE project_members SET role = ? WHERE project_id = ? AND member_id = ?;"};
    case RelationshipKind::CompetitionParticipants:
        return {
            "competition_participants", "competition_id", "member_id",
            "SELECT member_id, role FROM competition_participants WHERE competition_id = ?;",
            "DELETE FROM competition_participants WHERE competition_id = ? AND member_id = ?;",
            "INSERT INTO competition_participants (competition_id, member_id, role) "
            "VALUES (?, ?, ?);",
            "UPDATE competition_participants SET role = ? "
            "WHERE competition_id = ? AND member_id = ?;"};
    }
    return {};
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
bool ExecuteWrite(DbService& database, const wxString& sql, Binder binder,
                  const wxString& context, wxString* outError) {
    wxString prepareError;
    auto statement = database.Prepare(sql, &prepareError);
    if (!statement) {
        return Fail(outError, wxString::Format("Could not prepare %s:\n%s", context, prepareError));
    }

    if (!binder(*statement)) {
        return Fail(outError, wxString::Format("Could not bind values while %s.", context));
    }

    const int rc = sqlite3_step(statement->Get());
    if (rc == SQLITE_DONE) {
        return true;
    }
    return Fail(outError, DatabaseError(database, context));
}

wxString LabelWithDate(const wxString& title, const wxString& date) {
    if (date.IsEmpty()) {
        return title;
    }
    return title + wxString::FromUTF8(" \xe2\x80\x94 ") + date;
}

} // namespace

RelationshipForm::RelationshipForm(wxWindow* parent, DbService& database, RelationshipKind kind,
                                   std::function<void()> onReturnToParent)
    : wxPanel(parent, wxID_ANY),
      m_database(database),
      m_kind(kind),
      m_onReturnToParent(std::move(onReturnToParent)) {
    BuildControls();

    wxString error;
    if (!LoadParents(&error)) {
        wxMessageBox("Parent records could not be loaded:\n\n" + error,
                     "Relationship Form", wxOK | wxICON_ERROR, this);
        m_parents.clear();
    }

    m_loading = true;
    m_parentChoice->Clear();
    if (m_parents.empty()) {
        m_parentChoice->Append(ParentEmptyPlaceholder());
        m_parentChoice->SetSelection(0);
        m_parentChoice->Enable(false);
        m_scrolledWindow->Enable(false);
        m_saveButton->Enable(false);
        m_scrolledSizer->Clear(true);
        m_scrolledSizer->Add(
            new wxStaticText(m_scrolledWindow, wxID_ANY,
                             "Create a parent record before managing its member relationships."),
            0, wxALL, 10);
        m_scrolledSizer->Layout();
        m_scrolledWindow->FitInside();
    } else {
        for (const auto& record : m_parents) {
            m_parentChoice->Append(record.label);
        }
        m_parentChoice->SetSelection(0);
        m_parentChoice->Enable(true);
        m_scrolledWindow->Enable(true);
        m_saveButton->Enable(false);
    }
    m_loading = false;

    if (!m_parents.empty()) {
        if (!LoadParentAt(0, &error)) {
            wxMessageBox("The relationship list could not be loaded:\n\n" + error,
                         "Relationship Form", wxOK | wxICON_ERROR, this);
            // Leave no selection so choosing the first item again can retry
            // the load after a transient query failure.
            m_parentChoice->SetSelection(wxNOT_FOUND);
            m_currentParentIndex = -1;
            m_currentParentId.reset();
            m_scrolledWindow->Enable(false);
            m_saveButton->Enable(false);
        }
    }
}

void RelationshipForm::BuildControls() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY,
                                   m_kind == RelationshipKind::MeetingAttendance
                                       ? "Attendance Register"
                                       : (m_kind == RelationshipKind::ProjectMembers
                                              ? "Project Members"
                                              : "Competition Team Assignments"));
    wxFont titleFont = title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleFont.SetPointSize(titleFont.GetPointSize() + 2);
    title->SetFont(titleFont);
    root->Add(title, 0, wxALL, 12);

    auto* parentRow = new wxBoxSizer(wxHORIZONTAL);
    m_parentLabel = new wxStaticText(this, wxID_ANY, ParentNoun() + ":");
    parentRow->Add(m_parentLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    m_parentChoice = new wxChoice(this, wxID_ANY);
    parentRow->Add(m_parentChoice, 1, wxALIGN_CENTER_VERTICAL);
    root->Add(parentRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    m_scrolledWindow = new wxScrolledWindow(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_SIMPLE);
    m_scrolledWindow->SetScrollRate(0, 12);
    m_scrolledSizer = new wxBoxSizer(wxVERTICAL);
    m_scrolledWindow->SetSizer(m_scrolledSizer);
    root->Add(m_scrolledWindow, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    m_cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
    m_saveButton = new wxButton(this, wxID_SAVE, "Save");
    m_saveButton->SetDefault();
    actions->AddStretchSpacer(1);
    actions->Add(m_cancelButton, 0, wxRIGHT, 8);
    actions->Add(m_saveButton, 0);
    root->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    SetSizer(root);

    m_parentChoice->Bind(wxEVT_CHOICE, &RelationshipForm::OnParentChanged, this);
    m_saveButton->Bind(wxEVT_BUTTON, &RelationshipForm::OnSave, this);
    m_cancelButton->Bind(wxEVT_BUTTON, &RelationshipForm::OnCancel, this);
}

bool RelationshipForm::LoadParents(wxString* outError) {
    wxString sql;
    switch (m_kind) {
    case RelationshipKind::MeetingAttendance:
        sql = "SELECT meeting_id, title, meeting_date FROM meetings "
              "ORDER BY CASE WHEN meeting_date IS NULL OR trim(meeting_date) = '' "
              "THEN 1 ELSE 0 END ASC, meeting_date DESC, meeting_id ASC;";
        break;
    case RelationshipKind::ProjectMembers:
        sql = "SELECT project_id, title FROM projects "
              "ORDER BY title COLLATE NOCASE ASC, project_id ASC;";
        break;
    case RelationshipKind::CompetitionParticipants:
        sql = "SELECT competition_id, title, year FROM external_competitions "
              "ORDER BY title COLLATE NOCASE ASC, year ASC;";
        break;
    }

    auto statement = m_database.Prepare(sql, outError);
    if (!statement) {
        return false;
    }

    std::vector<ParentRecord> loaded;
    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(statement->Get())) == SQLITE_ROW) {
        ParentRecord record;
        record.id = statement->GetInt64(0);
        const wxString title = statement->GetText(1);
        if (m_kind == RelationshipKind::MeetingAttendance) {
            record.label = LabelWithDate(title, statement->GetText(2));
        } else if (m_kind == RelationshipKind::ProjectMembers) {
            record.label = title;
        } else {
            record.label = LabelWithDate(title, statement->GetText(2));
        }
        loaded.push_back(std::move(record));
    }

    if (rc != SQLITE_DONE) {
        return Fail(outError, DatabaseError(m_database, "loading parent records"));
    }
    m_parents = std::move(loaded);
    return true;
}

bool RelationshipForm::LoadMemberRows(std::int64_t parentId, std::vector<MemberRow>* rows,
                                      wxString* outError) {
    wxString sql;
    switch (m_kind) {
    case RelationshipKind::MeetingAttendance:
        sql = "SELECT m.member_id, m.name, m.class, m.status, "
              "CASE WHEN ma.member_id IS NULL THEN 0 ELSE 1 END, '' "
              "FROM members AS m "
              "LEFT JOIN meeting_attendance AS ma "
              "ON ma.member_id = m.member_id AND ma.meeting_id = ? "
              "WHERE m.status = 'Active' OR ma.member_id IS NOT NULL "
              "ORDER BY m.name COLLATE NOCASE ASC, m.member_id ASC;";
        break;
    case RelationshipKind::ProjectMembers:
        sql = "SELECT m.member_id, m.name, m.class, m.status, "
              "CASE WHEN pm.member_id IS NULL THEN 0 ELSE 1 END, COALESCE(pm.role, 'Member') "
              "FROM members AS m "
              "LEFT JOIN project_members AS pm "
              "ON pm.member_id = m.member_id AND pm.project_id = ? "
              "WHERE m.status = 'Active' OR pm.member_id IS NOT NULL "
              "ORDER BY m.name COLLATE NOCASE ASC, m.member_id ASC;";
        break;
    case RelationshipKind::CompetitionParticipants:
        sql = "SELECT m.member_id, m.name, m.class, m.status, "
              "CASE WHEN cp.member_id IS NULL THEN 0 ELSE 1 END, COALESCE(cp.role, '') "
              "FROM members AS m "
              "LEFT JOIN competition_participants AS cp "
              "ON cp.member_id = m.member_id AND cp.competition_id = ? "
              "WHERE m.status = 'Active' OR cp.member_id IS NOT NULL "
              "ORDER BY m.name COLLATE NOCASE ASC, m.member_id ASC;";
        break;
    }

    wxString prepareError;
    auto statement = m_database.Prepare(sql, &prepareError);
    if (!statement) {
        return Fail(outError, wxString::Format("Could not load members:\n%s", prepareError));
    }
    if (!statement->BindInt64(1, parentId)) {
        return Fail(outError, "Could not bind the selected parent record.");
    }

    std::vector<MemberRow> loaded;
    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(statement->Get())) == SQLITE_ROW) {
        MemberRow row;
        row.memberId = statement->GetInt64(0);
        const wxString name = statement->GetText(1);
        const wxString className = statement->GetText(2);
        const wxString status = statement->GetText(3);
        row.isFormer = status == "Former";
        row.baselineChecked = statement->GetInt(4) != 0;
        row.baselineRole = statement->GetText(5);
        row.label = name;
        if (!className.IsEmpty()) {
            row.label += " (" + className + ")";
        }
        if (row.isFormer) {
            row.label += " (Former)";
        }
        loaded.push_back(std::move(row));
    }
    if (rc != SQLITE_DONE) {
        return Fail(outError, DatabaseError(m_database, "loading relationship members"));
    }
    *rows = std::move(loaded);
    return true;
}

bool RelationshipForm::LoadParentAt(int index, wxString* outError) {
    if (index < 0 || static_cast<std::size_t>(index) >= m_parents.size()) {
        return Fail(outError, "Choose a valid parent record.");
    }

    std::vector<MemberRow> loadedRows;
    const std::int64_t parentId = m_parents[static_cast<std::size_t>(index)].id;
    if (!LoadMemberRows(parentId, &loadedRows, outError)) {
        return false;
    }

    m_loading = true;
    RebuildChecklist(std::move(loadedRows));
    m_currentParentIndex = index;
    m_currentParentId = parentId;
    m_parentChoice->SetSelection(index);
    m_loading = false;

    m_scrolledWindow->Enable(true);
    m_saveButton->Enable(true);
    MarkClean();
    Layout();
    return true;
}

void RelationshipForm::RebuildChecklist(std::vector<MemberRow> rows) {
    m_rows.clear();
    m_scrolledSizer->Clear(true);

    auto* grid = new wxFlexGridSizer(0, HasRoleControls() ? 2 : 1, 6, 12);
    grid->AddGrowableCol(0, 1);
    if (HasRoleControls()) {
        grid->AddGrowableCol(1, 1);
    }

    auto* memberHeading = new wxStaticText(m_scrolledWindow, wxID_ANY, "Member");
    wxFont headingFont = memberHeading->GetFont();
    headingFont.SetWeight(wxFONTWEIGHT_BOLD);
    memberHeading->SetFont(headingFont);
    grid->Add(memberHeading, 0, wxBOTTOM, 4);

    if (HasRoleControls()) {
        auto* roleHeading = new wxStaticText(m_scrolledWindow, wxID_ANY, "Role");
        roleHeading->SetFont(headingFont);
        grid->Add(roleHeading, 0, wxBOTTOM, 4);
    }

    for (auto& row : rows) {
        row.checkBox = new wxCheckBox(m_scrolledWindow, wxID_ANY, row.label);
        row.checkBox->SetValue(row.baselineChecked);
        row.checkBox->Bind(wxEVT_CHECKBOX, &RelationshipForm::OnMemberToggled, this);
        grid->Add(row.checkBox, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND);

        if (m_kind == RelationshipKind::ProjectMembers) {
            wxArrayString roles;
            roles.Add("Member");
            roles.Add("Lead");
            row.projectRole = new wxChoice(m_scrolledWindow, wxID_ANY,
                                           wxDefaultPosition, wxDefaultSize, roles);
            row.projectRole->SetStringSelection(
                row.baselineRole.IsEmpty() ? "Member" : row.baselineRole);
            if (row.projectRole->GetSelection() == wxNOT_FOUND) {
                row.projectRole->SetSelection(0);
            }
            row.projectRole->Bind(wxEVT_CHOICE, &RelationshipForm::OnRoleChanged, this);
            grid->Add(row.projectRole, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
        } else if (m_kind == RelationshipKind::CompetitionParticipants) {
            row.competitionRole = new wxTextCtrl(m_scrolledWindow, wxID_ANY,
                                                 row.baselineRole, wxDefaultPosition,
                                                 wxSize(180, -1));
            row.competitionRole->Bind(wxEVT_TEXT, &RelationshipForm::OnRoleChanged, this);
            grid->Add(row.competitionRole, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
        }

        UpdateRowEnabledState(row);
        m_rows.push_back(std::move(row));
    }

    if (m_rows.empty()) {
        auto* empty = new wxStaticText(
            m_scrolledWindow, wxID_ANY,
            "No members are currently available for this parent. Active members appear here; "
            "former members appear only when already assigned.");
        empty->Wrap(640);
        m_scrolledSizer->Add(empty, 0, wxALL, 10);
    } else {
        m_scrolledSizer->Add(grid, 0, wxEXPAND | wxALL, 10);
    }

    m_scrolledSizer->Layout();
    m_scrolledWindow->FitInside();
    m_scrolledWindow->Layout();
}

bool RelationshipForm::HasRoleControls() const noexcept {
    return m_kind != RelationshipKind::MeetingAttendance;
}

wxString RelationshipForm::CurrentRole(const MemberRow& row) const {
    if (row.projectRole) {
        return row.projectRole->GetStringSelection();
    }
    if (row.competitionRole) {
        return row.competitionRole->GetValue();
    }
    return {};
}

bool RelationshipForm::IsDirty() const {
    if (!m_currentParentId) {
        return false;
    }

    for (const auto& row : m_rows) {
        const bool checked = row.checkBox && row.checkBox->GetValue();
        if (checked != row.baselineChecked) {
            return true;
        }
        if (checked && CurrentRole(row) != row.baselineRole) {
            return true;
        }
    }
    return false;
}

void RelationshipForm::MarkClean() {
    for (auto& row : m_rows) {
        row.baselineChecked = row.checkBox && row.checkBox->GetValue();
        row.baselineRole = CurrentRole(row);
        UpdateRowEnabledState(row);
    }
}

void RelationshipForm::DiscardChanges() {
    for (auto& row : m_rows) {
        if (row.checkBox) {
            row.checkBox->SetValue(row.baselineChecked);
        }
        if (row.projectRole) {
            row.projectRole->SetStringSelection(row.baselineRole.IsEmpty()
                                                    ? "Member"
                                                    : row.baselineRole);
            if (row.projectRole->GetSelection() == wxNOT_FOUND) {
                row.projectRole->SetSelection(0);
            }
        }
        if (row.competitionRole) {
            row.competitionRole->ChangeValue(row.baselineRole);
        }
        UpdateRowEnabledState(row);
    }
    Layout();
}

void RelationshipForm::UpdateRowEnabledState(MemberRow& row) {
    const bool enabled = row.checkBox && row.checkBox->GetValue();
    if (row.projectRole) {
        row.projectRole->Enable(enabled);
    }
    if (row.competitionRole) {
        row.competitionRole->Enable(enabled);
    }
}

wxString RelationshipForm::ParentNoun() const {
    switch (m_kind) {
    case RelationshipKind::MeetingAttendance:
        return "Meeting";
    case RelationshipKind::ProjectMembers:
        return "Project";
    case RelationshipKind::CompetitionParticipants:
        return "Competition";
    }
    return "Parent";
}

wxString RelationshipForm::ParentEmptyPlaceholder() const {
    switch (m_kind) {
    case RelationshipKind::MeetingAttendance:
        return "No meetings available";
    case RelationshipKind::ProjectMembers:
        return "No projects available";
    case RelationshipKind::CompetitionParticipants:
        return "No competitions available";
    }
    return "No parent records available";
}

wxString RelationshipForm::ParentTableTitle() const {
    switch (m_kind) {
    case RelationshipKind::MeetingAttendance:
        return "All Meetings";
    case RelationshipKind::ProjectMembers:
        return "All Projects";
    case RelationshipKind::CompetitionParticipants:
        return "All Competitions";
    }
    return {};
}

RelationshipForm::UnsavedDecision RelationshipForm::PromptUnsavedChanges() {
    wxDialog dialog(this, wxID_ANY, "Unsaved Changes", wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* message = new wxStaticText(
        &dialog, wxID_ANY,
        "This relationship form has unsaved changes. Save them, discard them, or cancel to stay.");
    message->Wrap(420);
    layout->Add(message, 0, wxALL, 14);

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* save = new wxButton(&dialog, wxID_SAVE, "Save");
    auto* discard = new wxButton(&dialog, wxID_NO, "Discard");
    auto* cancel = new wxButton(&dialog, wxID_CANCEL, "Cancel");
    save->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_SAVE); });
    discard->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_NO); });
    cancel->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_CANCEL); });
    cancel->SetDefault();
    actions->Add(save, 0, wxRIGHT, 8);
    actions->Add(discard, 0, wxRIGHT, 8);
    actions->Add(cancel, 0);
    layout->Add(actions, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    dialog.SetSizerAndFit(layout);
    dialog.CentreOnParent();

    switch (dialog.ShowModal()) {
    case wxID_SAVE:
        return UnsavedDecision::Save;
    case wxID_NO:
        return UnsavedDecision::Discard;
    default:
        return UnsavedDecision::Cancel;
    }
}

bool RelationshipForm::ResolveDirtyStateBeforeSwitch(int targetIndex) {
    const int currentIndex = m_currentParentIndex;
    const UnsavedDecision decision = PromptUnsavedChanges();

    if (decision == UnsavedDecision::Cancel) {
        m_loading = true;
        m_parentChoice->SetSelection(currentIndex);
        m_loading = false;
        return false;
    }

    if (decision == UnsavedDecision::Discard) {
        DiscardChanges();
    } else {
        wxString error;
        if (!Save(&error)) {
            wxMessageBox(error, "Could Not Save Relationships",
                         wxOK | wxICON_ERROR, this);
            m_loading = true;
            m_parentChoice->SetSelection(currentIndex);
            m_loading = false;
            return false;
        }
    }

    wxString loadError;
    if (!LoadParentAt(targetIndex, &loadError)) {
        wxMessageBox("The selected parent could not be loaded:\n\n" + loadError,
                     "Could Not Switch Parent", wxOK | wxICON_ERROR, this);
        m_loading = true;
        m_parentChoice->SetSelection(currentIndex);
        m_loading = false;
        return false;
    }
    return true;
}

bool RelationshipForm::Save(wxString* outError) {
    if (outError) {
        outError->Clear();
    }
    if (!m_currentParentId) {
        return Fail(outError, ParentEmptyPlaceholder() +
                                  ". Create a parent record before saving relationships.");
    }

    // Validate required per-row role values before opening a transaction.
    if (m_kind == RelationshipKind::ProjectMembers) {
        for (const auto& row : m_rows) {
            if (row.checkBox && row.checkBox->GetValue() &&
                (!row.projectRole || row.projectRole->GetSelection() == wxNOT_FOUND)) {
                return Fail(outError, "Choose either Member or Lead for each selected project member.");
            }
        }
    }

    if (!SaveCurrentParent(outError)) {
        return false;
    }
    MarkClean();
    return true;
}

bool RelationshipForm::SaveCurrentParent(wxString* outError) {
    if (!m_currentParentId) {
        return Fail(outError, "No parent record is selected.");
    }

    const RelationshipSql sql = SqlFor(m_kind);
    Transaction transaction(m_database);
    if (!transaction.IsActive()) {
        return Fail(outError, transaction.GetError().IsEmpty()
                                  ? "Could not begin the relationship update transaction."
                                  : transaction.GetError());
    }

    std::unordered_map<std::int64_t, wxString> existing;
    wxString prepareError;
    auto query = m_database.Prepare(sql.selectSql, &prepareError);
    if (!query) {
        const wxString error = "Could not load existing assignments:\n" + prepareError;
        transaction.Rollback(nullptr);
        return Fail(outError, error);
    }
    if (!query->BindInt64(1, *m_currentParentId)) {
        query.reset();
        transaction.Rollback(nullptr);
        return Fail(outError, "Could not bind the current parent ID.");
    }

    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(query->Get())) == SQLITE_ROW) {
        const std::int64_t memberId = query->GetInt64(0);
        const wxString role = query->GetColumnCount() > 1 ? query->GetText(1) : wxString{};
        existing.emplace(memberId, role);
    }
    if (rc != SQLITE_DONE) {
        const wxString error = DatabaseError(m_database, "reading existing relationship assignments");
        query.reset();
        transaction.Rollback(nullptr);
        return Fail(outError, error);
    }
    query.reset();

    std::unordered_map<std::int64_t, const MemberRow*> desired;
    for (const auto& row : m_rows) {
        if (row.checkBox && row.checkBox->GetValue()) {
            desired.emplace(row.memberId, &row);
        }
    }

    // Unchecked assignments are removed first. The enclosing transaction ensures
    // a later failure restores every deletion performed in this save attempt.
    for (const auto& assignment : existing) {
        if (desired.find(assignment.first) == desired.end()) {
            wxString error;
            const bool ok = ExecuteWrite(
                m_database, sql.deleteSql,
                [&](Statement& statement) {
                    return statement.BindInt64(1, *m_currentParentId) &&
                           statement.BindInt64(2, assignment.first);
                },
                "removing a relationship assignment", &error);
            if (!ok) {
                transaction.Rollback(nullptr);
                return Fail(outError, error);
            }
        }
    }

    // Insert new checked members and update the roles of continuing members.
    for (const auto& assignment : desired) {
        const std::int64_t memberId = assignment.first;
        const MemberRow& row = *assignment.second;
        const auto prior = existing.find(memberId);
        const wxString role = CurrentRole(row);

        if (prior == existing.end()) {
            wxString error;
            const bool ok = ExecuteWrite(
                m_database, sql.insertSql,
                [&](Statement& statement) {
                    if (!statement.BindInt64(1, *m_currentParentId) ||
                        !statement.BindInt64(2, memberId)) {
                        return false;
                    }
                    return !HasRoleControls() || statement.BindText(3, role);
                },
                "adding a relationship assignment", &error);
            if (!ok) {
                transaction.Rollback(nullptr);
                return Fail(outError, error);
            }
        } else if (HasRoleControls() && role != prior->second) {
            wxString error;
            const bool ok = ExecuteWrite(
                m_database, sql.updateRoleSql,
                [&](Statement& statement) {
                    return statement.BindText(1, role) &&
                           statement.BindInt64(2, *m_currentParentId) &&
                           statement.BindInt64(3, memberId);
                },
                "updating a relationship role", &error);
            if (!ok) {
                transaction.Rollback(nullptr);
                return Fail(outError, error);
            }
        }
    }

    wxString commitError;
    if (!transaction.Commit(&commitError)) {
        const wxString reported = DatabaseError(m_database, "committing relationship changes");
        transaction.Rollback(nullptr);
        return Fail(outError, reported.IsEmpty() ? commitError : reported);
    }
    return true;
}

void RelationshipForm::OnParentChanged(wxCommandEvent& event) {
    if (m_loading) {
        event.Skip();
        return;
    }
    const int targetIndex = m_parentChoice->GetSelection();
    if (targetIndex == wxNOT_FOUND || targetIndex == m_currentParentIndex) {
        event.Skip();
        return;
    }

    if (IsDirty()) {
        (void)ResolveDirtyStateBeforeSwitch(targetIndex);
        event.Skip();
        return;
    }

    wxString error;
    if (!LoadParentAt(targetIndex, &error)) {
        wxMessageBox("The selected parent could not be loaded:\n\n" + error,
                     "Could Not Switch Parent", wxOK | wxICON_ERROR, this);
        m_loading = true;
        if (m_currentParentIndex >= 0) {
            m_parentChoice->SetSelection(m_currentParentIndex);
        }
        m_loading = false;
    }
    event.Skip();
}

void RelationshipForm::OnMemberToggled(wxCommandEvent& event) {
    auto* changed = dynamic_cast<wxCheckBox*>(event.GetEventObject());
    for (auto& row : m_rows) {
        if (row.checkBox == changed) {
            UpdateRowEnabledState(row);
            break;
        }
    }
    event.Skip();
    Layout();
}

void RelationshipForm::OnRoleChanged(wxCommandEvent& event) {
    event.Skip();
}

void RelationshipForm::OnSave(wxCommandEvent& /*event*/) {
    wxString error;
    if (!Save(&error)) {
        wxMessageBox(error, "Could Not Save Relationships", wxOK | wxICON_ERROR, this);
        return;
    }
    ReturnToParent();
}

void RelationshipForm::OnCancel(wxCommandEvent& /*event*/) {
    if (!IsDirty()) {
        ReturnToParent();
        return;
    }

    const UnsavedDecision decision = PromptUnsavedChanges();
    if (decision == UnsavedDecision::Cancel) {
        return;
    }
    if (decision == UnsavedDecision::Save) {
        wxString error;
        if (!Save(&error)) {
            wxMessageBox(error, "Could Not Save Relationships", wxOK | wxICON_ERROR, this);
            return;
        }
    } else {
        DiscardChanges();
    }
    ReturnToParent();
}

void RelationshipForm::ReturnToParent() {
    if (!m_onReturnToParent) {
        return;
    }
    wxWindow* const stableParent = GetParent();
    const auto callback = m_onReturnToParent;
    if (stableParent) {
        stableParent->CallAfter([callback]() { callback(); });
    } else {
        callback();
    }
}

} // namespace msms
