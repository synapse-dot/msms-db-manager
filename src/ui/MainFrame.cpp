#include "MainFrame.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <wx/accel.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/fileconf.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

#include "db/DbService.h"
#include "forms/RecordForms.h"
#include "forms/RelationshipForm.h"
#include "ui/ScopedPage.h"
#include "ui/TablePage.h"

namespace msms {
namespace {

constexpr const char* kApplicationVersion = "1.0.0";

enum MenuItemId {
    ID_OPEN_DATABASE = wxID_HIGHEST + 1,
    ID_CREATE_NEW_DATABASE,
    ID_SAVE_BACKUP,
    ID_RESTORE_BACKUP
};

enum class NavigationPageType {
    Table,
    ScopedTable,
    Form,
    RelationshipForm
};

class NavigationItemData final : public wxTreeItemData {
public:
    // A missing page type identifies a category node.
    explicit NavigationItemData(wxString title) : m_pageTitle(std::move(title)) {}

    NavigationItemData(wxString pageTitle, NavigationPageType pageType)
        : m_pageTitle(std::move(pageTitle)), m_pageType(pageType) {}

    [[nodiscard]] bool IsPage() const noexcept { return m_pageType.has_value(); }
    [[nodiscard]] const wxString& PageTitle() const noexcept { return m_pageTitle; }
    [[nodiscard]] const std::optional<NavigationPageType>& PageType() const noexcept {
        return m_pageType;
    }

private:
    wxString m_pageTitle;
    std::optional<NavigationPageType> m_pageType;
};

class BlankNavigationForm final : public wxPanel, public INavigationForm {
public:
    BlankNavigationForm(wxWindow* parent, const wxString& pageTitle,
                        const std::optional<wxString>& recordKey)
        : wxPanel(parent, wxID_ANY) {
        auto* layout = new wxBoxSizer(wxVERTICAL);

        auto* title = new wxStaticText(this, wxID_ANY, pageTitle);
        wxFont titleFont = title->GetFont();
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        titleFont.SetPointSize(titleFont.GetPointSize() + 2);
        title->SetFont(titleFont);

        wxString modeText =
            "Create mode — blank form. Fields will be provided by the feature-specific form view.";
        if (recordKey) {
            wxString recordDescription = *recordKey;
            const int separator = recordDescription.Find(static_cast<wxChar>(0x1f));
            if (separator != wxNOT_FOUND) {
                recordDescription = "Start Date: " + recordDescription.Left(separator) +
                                    ", Office: " + recordDescription.Mid(separator + 1);
            }
            modeText = "Edit mode — selected record key: " + recordDescription +
                       ". Editable record fields will be provided by the feature-specific form view.";
        }

        auto* mode = new wxStaticText(this, wxID_ANY, modeText);
        mode->Wrap(480);

        layout->Add(title, 0, wxALL, 16);
        layout->Add(mode, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        SetSizer(layout);
    }

    [[nodiscard]] bool IsDirty() const override { return false; }
    bool Save(wxString* /*outError*/) override { return true; }
    void DiscardChanges() override {}
};

// Return the sizer that directly owns window, searching nested sizers too.
wxSizer* FindContainingSizer(wxSizer* sizer, wxWindow* window) {
    if (!sizer || !window) {
        return nullptr;
    }

    for (wxSizerItemList::compatibility_iterator node = sizer->GetChildren().GetFirst(); node;
         node = node->GetNext()) {
        wxSizerItem* item = node->GetData();
        if (item->GetWindow() == window) {
            return sizer;
        }
        if (item->IsSizer()) {
            if (wxSizer* owner = FindContainingSizer(item->GetSizer(), window)) {
                return owner;
            }
        }
    }

    return nullptr;
}

bool SameFilePath(const wxString& first, const wxString& second) {
    if (first.IsEmpty() || second.IsEmpty()) {
        return false;
    }

    wxFileName firstFile(first);
    wxFileName secondFile(second);
    firstFile.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    secondFile.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    return firstFile.SameAs(secondFile);
}

wxString DatabasePathLockFile(const wxString& databasePath) {
    wxFileName file(databasePath);
    file.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    return file.GetPathWithSep() + "." + file.GetFullName() + ".msms.lock";
}

void ShowDatabaseError(wxWindow* parent, const wxString& title, const wxString& message) {
    wxMessageBox(message, title, wxOK | wxICON_ERROR, parent);
}

std::optional<std::int64_t> ParseRecordId(const wxString& text) {
    try {
        const std::string encoded = text.ToStdString();
        std::size_t consumed = 0;
        const long long value = std::stoll(encoded, &consumed, 10);
        if (consumed != encoded.size() || value <= 0) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(value);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

MainFrame::MainFrame(DbService& dbService)
    : wxFrame(nullptr, wxID_ANY, "MSMS Manager", wxDefaultPosition, wxSize(1000, 700)),
      m_dbService(dbService) {
    SetMinSize(wxSize(800, 500));

    m_contentPanel = new wxPanel(this, wxID_ANY);
    auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    m_navigationPanel = new wxPanel(m_contentPanel, wxID_ANY);
    m_navigationPanel->SetMinSize(wxSize(230, -1));
    auto* navigationSizer = new wxBoxSizer(wxVERTICAL);

    m_navigationTree = new wxTreeCtrl(
        m_navigationPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE | wxTR_HIDE_ROOT);
    navigationSizer->Add(m_navigationTree, 1, wxEXPAND | wxALL, 4);
    m_navigationPanel->SetSizer(navigationSizer);

    m_workspacePanel = new wxPanel(m_contentPanel, wxID_ANY);
    m_workspaceSizer = new wxBoxSizer(wxVERTICAL);
    m_workspacePanel->SetSizer(m_workspaceSizer);

    contentSizer->Add(m_navigationPanel, 0, wxEXPAND | wxTOP | wxLEFT | wxBOTTOM, 4);
    contentSizer->Add(m_workspacePanel, 1, wxEXPAND | wxALL, 4);
    m_contentPanel->SetSizer(contentSizer);

    auto* frameSizer = new wxBoxSizer(wxVERTICAL);
    frameSizer->Add(m_contentPanel, 1, wxEXPAND);
    SetSizer(frameSizer);

    BuildMenuBar();
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    m_navigationTree->Bind(wxEVT_TREE_SEL_CHANGING,
                           &MainFrame::OnNavigationSelectionChanging, this);

    BuildNavigationTree();
    UpdateTitle();
    Centre();
}

void MainFrame::BuildMenuBar() {
    auto* fileMenu = new wxMenu;
    fileMenu->Append(ID_OPEN_DATABASE, "&Open Database...");
    fileMenu->Append(ID_CREATE_NEW_DATABASE, "&Create New Database...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_SAVE_BACKUP, "Save &Backup...");
    fileMenu->Append(ID_RESTORE_BACKUP, "&Restore Backup...");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit\tCtrl+Q");

    auto* helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT, "&About");

    auto* menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    const wxAcceleratorEntry exitShortcut(wxACCEL_CTRL, 'Q', wxID_EXIT);
    SetAcceleratorTable(wxAcceleratorTable(1, &exitShortcut));

    Bind(wxEVT_MENU, &MainFrame::OnOpenDatabase, this, ID_OPEN_DATABASE);
    Bind(wxEVT_MENU, &MainFrame::OnCreateNewDatabase, this, ID_CREATE_NEW_DATABASE);
    Bind(wxEVT_MENU, &MainFrame::OnSaveBackup, this, ID_SAVE_BACKUP);
    Bind(wxEVT_MENU, &MainFrame::OnRestoreBackup, this, ID_RESTORE_BACKUP);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
}

void MainFrame::BuildNavigationTree() {
    const wxTreeItemId root = m_navigationTree->AddRoot("Navigation");

    const auto addCategory = [this, root](const wxString& title) {
        return m_navigationTree->AppendItem(
            root, title, -1, -1, new NavigationItemData(title));
    };
    const auto addPage = [this](const wxTreeItemId& parent, const wxString& title,
                                NavigationPageType pageType) {
        return m_navigationTree->AppendItem(
            parent, title, -1, -1, new NavigationItemData(title, pageType));
    };

    const wxTreeItemId members = addCategory("Members");
    const wxTreeItemId allMembers =
        addPage(members, "All Members", NavigationPageType::Table);
    addPage(members, "New / Edit Member", NavigationPageType::Form);

    const wxTreeItemId meetings = addCategory("Meetings");
    addPage(meetings, "All Meetings", NavigationPageType::Table);
    addPage(meetings, "Attendees", NavigationPageType::ScopedTable);
    addPage(meetings, "New / Edit Meeting", NavigationPageType::Form);
    addPage(meetings, "Attendance Register", NavigationPageType::RelationshipForm);

    const wxTreeItemId projects = addCategory("Projects");
    addPage(projects, "All Projects", NavigationPageType::Table);
    addPage(projects, "Project Teams", NavigationPageType::ScopedTable);
    addPage(projects, "New / Edit Project", NavigationPageType::Form);
    addPage(projects, "New / Edit Project Members", NavigationPageType::RelationshipForm);

    const wxTreeItemId committee = addCategory("Committee");
    addPage(committee, "All Appointments", NavigationPageType::Table);
    addPage(committee, "Current Committee", NavigationPageType::Table);
    addPage(committee, "New / Edit Appointment", NavigationPageType::Form);

    const wxTreeItemId activities = addCategory("Activities");
    addPage(activities, "All Activities", NavigationPageType::Table);
    addPage(activities, "New / Edit Activity", NavigationPageType::Form);

    const wxTreeItemId competitions = addCategory("External Competitions");
    addPage(competitions, "All Competitions", NavigationPageType::Table);
    addPage(competitions, "Competition Teams", NavigationPageType::ScopedTable);
    addPage(competitions, "New / Edit Competitions", NavigationPageType::Form);
    addPage(competitions, "New / Edit Competition Team Assignments",
            NavigationPageType::RelationshipForm);

    m_navigationTree->ExpandAll();
    m_navigationTree->SelectItem(allMembers);
    // Some native tree controls do not emit selection notifications for a
    // programmatic initial selection. Route the selected item through the same
    // controller path in that case.
    if (!m_activeWorkspaceView) {
        (void)NavigateToNavigationItem(allMembers);
    }
}

void MainFrame::OnNavigationSelectionChanging(wxTreeEvent& event) {
    if (!NavigateToNavigationItem(event.GetItem())) {
        event.Veto();
        return;
    }
    event.Skip();
}

bool MainFrame::NavigateToNavigationItem(const wxTreeItemId& item) {
    if (!item.IsOk()) {
        return false;
    }

    const auto* data = dynamic_cast<const NavigationItemData*>(m_navigationTree->GetItemData(item));
    if (!data) {
        return false;
    }

    if (!data->IsPage()) {
        // Categories may expand or collapse, but only page items change the workspace.
        return true;
    }

    // Avoid rebuilding a page when its leaf is still the selected item.
    // If a category was selected in between, selecting the active leaf again
    // intentionally re-enters it (forms therefore return to a clean create view).
    if (item == m_activeNavigationItem && m_activeWorkspaceView &&
        m_navigationTree->GetSelection() == item) {
        return true;
    }

    if (!ConfirmLeaveActiveForm()) {
        return false;
    }

    const auto pageType = data->PageType();
    const bool formPage =
        pageType && (*pageType == NavigationPageType::Form ||
                     *pageType == NavigationPageType::RelationshipForm);
    std::optional<wxString> recordKey;
    if (m_pendingFormOpen && m_pendingFormTitle == data->PageTitle()) {
        recordKey = m_pendingFormRecordKey;
        m_pendingFormOpen = false;
        m_pendingFormTitle.Clear();
        m_pendingFormRecordKey.reset();
    }
    wxWindow* const view = CreateNavigationPage(data->PageTitle(), formPage, recordKey);
    if (!view || !SetWorkspaceView(view)) {
        if (view) {
            view->Destroy();
        }
        ShowDatabaseError(this, "Navigation Error",
                          "The selected page could not be opened. The current page was kept.");
        return false;
    }

    m_activeNavigationItem = item;
    return true;
}

bool MainFrame::ConfirmLeaveActiveForm() {
    auto* form = dynamic_cast<INavigationForm*>(m_activeWorkspaceView);
    if (!form || !form->IsDirty()) {
        return true;
    }

    wxDialog dialog(this, wxID_ANY, "Unsaved Changes", wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* message = new wxStaticText(
        &dialog, wxID_ANY,
        "This form has unsaved changes. Save before continuing, discard the changes, or cancel this action.");
    message->Wrap(420);
    layout->Add(message, 0, wxALL, 14);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* saveButton = new wxButton(&dialog, wxID_SAVE, "Save");
    auto* discardButton = new wxButton(&dialog, wxID_NO, "Discard");
    auto* cancelButton = new wxButton(&dialog, wxID_CANCEL, "Cancel");
    saveButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_SAVE); });
    discardButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_NO); });
    cancelButton->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(wxID_CANCEL); });
    cancelButton->SetDefault();
    buttons->Add(saveButton, 0, wxRIGHT, 8);
    buttons->Add(discardButton, 0, wxRIGHT, 8);
    buttons->Add(cancelButton, 0);
    layout->Add(buttons, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    dialog.SetSizerAndFit(layout);
    dialog.CentreOnParent();

    const int result = dialog.ShowModal();
    if (result == wxID_SAVE) {
        wxString error;
        if (!form->Save(&error)) {
            if (error.IsEmpty()) {
                error = "The form could not be saved.";
            }
            ShowDatabaseError(this, "Save Error", error);
            return false;
        }
        if (form->IsDirty()) {
            ShowDatabaseError(
                this, "Save Error",
                "The form still reports unsaved changes after saving. The action was cancelled.");
            return false;
        }
        return true;
    }

    if (result == wxID_NO) {
        form->DiscardChanges();
        if (form->IsDirty()) {
            ShowDatabaseError(
                this, "Discard Error",
                "The form still reports unsaved changes after discard. The action was cancelled.");
            return false;
        }
        return true;
    }

    return false;
}

wxWindow* MainFrame::CreateNavigationPage(
    const wxString& pageTitle, bool formPage, const std::optional<wxString>& recordKey) {
    if (formPage) {
        const bool recordForm =
            pageTitle == "New / Edit Member" || pageTitle == "New / Edit Meeting" ||
            pageTitle == "New / Edit Project" || pageTitle == "New / Edit Appointment" ||
            pageTitle == "New / Edit Activity" || pageTitle == "New / Edit Competitions";

        if (recordForm) {
            if (!m_dbService.IsOpen()) {
                ShowDatabaseError(this, "Database Error",
                                  "Open a database before creating or editing records.");
                return nullptr;
            }

            const RecordFormMode mode =
                recordKey ? RecordFormMode::Edit : RecordFormMode::Create;
            const auto returnToParent = [this, pageTitle]() {
                wxString parentTitle;
                if (pageTitle == "New / Edit Member") {
                    parentTitle = "All Members";
                } else if (pageTitle == "New / Edit Meeting") {
                    parentTitle = "All Meetings";
                } else if (pageTitle == "New / Edit Project") {
                    parentTitle = "All Projects";
                } else if (pageTitle == "New / Edit Appointment") {
                    parentTitle = "All Appointments";
                } else if (pageTitle == "New / Edit Activity") {
                    parentTitle = "All Activities";
                } else {
                    parentTitle = "All Competitions";
                }
                ReturnToTablePage(parentTitle);
            };

            if (pageTitle == "New / Edit Appointment") {
                return new AppointmentForm(m_workspacePanel, m_dbService, mode, recordKey,
                                           returnToParent);
            }

            std::optional<std::int64_t> id;
            if (recordKey) {
                id = ParseRecordId(*recordKey);
                if (!id) {
                    ShowDatabaseError(this, "Invalid Record Key",
                                      "The selected record ID is invalid. Return to its table and "
                                      "select the record again.");
                    return nullptr;
                }
            }

            if (pageTitle == "New / Edit Member") {
                return new MemberForm(m_workspacePanel, m_dbService, mode, id, returnToParent);
            }
            if (pageTitle == "New / Edit Meeting") {
                return new MeetingForm(m_workspacePanel, m_dbService, mode, id, returnToParent);
            }
            if (pageTitle == "New / Edit Project") {
                return new ProjectForm(m_workspacePanel, m_dbService, mode, id, returnToParent);
            }
            if (pageTitle == "New / Edit Activity") {
                return new ActivityForm(m_workspacePanel, m_dbService, mode, id, returnToParent);
            }
            return new CompetitionForm(m_workspacePanel, m_dbService, mode, id, returnToParent);
        }

        const bool relationshipForm =
            pageTitle == "Attendance Register" ||
            pageTitle == "New / Edit Project Members" ||
            pageTitle == "New / Edit Competition Team Assignments";
        if (relationshipForm) {
            if (!m_dbService.IsOpen()) {
                ShowDatabaseError(this, "Database Error",
                                  "Open a database before managing member relationships.");
                return nullptr;
            }

            RelationshipKind kind = RelationshipKind::MeetingAttendance;
            wxString parentTableTitle = "All Meetings";
            if (pageTitle == "New / Edit Project Members") {
                kind = RelationshipKind::ProjectMembers;
                parentTableTitle = "All Projects";
            } else if (pageTitle == "New / Edit Competition Team Assignments") {
                kind = RelationshipKind::CompetitionParticipants;
                parentTableTitle = "All Competitions";
            }

            const auto returnToParent = [this, parentTableTitle]() {
                ReturnToTablePage(parentTableTitle);
            };
            return new RelationshipForm(m_workspacePanel, m_dbService, kind, returnToParent);
        }

        return new BlankNavigationForm(m_workspacePanel, pageTitle, recordKey);
    }

    if (wxWindow* tablePage = CreateRecordTablePage(pageTitle)) {
        return tablePage;
    }

    if (wxWindow* scopedPage = CreateScopedTablePage(pageTitle)) {
        return scopedPage;
    }

    auto* view = new wxPanel(m_workspacePanel, wxID_ANY);
    auto* viewSizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(view, wxID_ANY, pageTitle);
    wxFont titleFont = title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleFont.SetPointSize(titleFont.GetPointSize() + 2);
    title->SetFont(titleFont);

    auto* description =
        new wxStaticText(view, wxID_ANY, "The selected page will be displayed here.");

    viewSizer->Add(title, 0, wxALL, 16);
    viewSizer->Add(description, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    view->SetSizer(viewSizer);
    return view;
}

wxWindow* MainFrame::CreateRecordTablePage(const wxString& pageTitle) {
    TablePageOptions options;
    options.title = pageTitle;
    wxString formTitle;

    const auto loadRows = [this](const wxString& sql, const auto& makeRow) {
        std::vector<TableRow> rows;
        if (!m_dbService.IsOpen()) {
            return rows;
        }

        wxString error;
        auto statement = m_dbService.Prepare(sql, &error);
        if (!statement) {
            ShowDatabaseError(this, "Database Query Error",
                              wxString("Could not load table data:\n\n") + error);
            return rows;
        }

        while (statement->Step()) {
            rows.push_back(makeRow(*statement));
        }
        return rows;
    };

    if (pageTitle == "All Members") {
        formTitle = "New / Edit Member";
        options.columns = {{"Name", 180}, {"Class", 80}, {"Phone", 130},
                           {"Email", 220}, {"Status", 90}, {"Join Date", 110}};
        options.filterOptions = {{"Active", "Active"}, {"Former", "Former"}};
        options.rows = loadRows(
            "SELECT member_id, name, class, phone, email, status, join_date "
            "FROM members ORDER BY name COLLATE NOCASE ASC, member_id ASC;",
            [](Statement& statement) {
                TableRow row;
                row.id = statement.GetInt64(0);
                row.cells = {statement.GetText(1), statement.GetText(2), statement.GetText(3),
                             statement.GetText(4), statement.GetText(5), statement.GetText(6)};
                row.filterValue = statement.GetText(5);
                return row;
            });
    } else if (pageTitle == "All Meetings") {
        formTitle = "New / Edit Meeting";
        options.columns = {{"Title", 180}, {"Type", 70}, {"Date", 110},
                           {"Start", 80}, {"End", 80}, {"Location", 160}};
        options.filterOptions = {{"AGM", "AGM"}, {"EGM", "EGM"}, {"ECM", "ECM"}};
        options.rows = loadRows(
            "SELECT meeting_id, title, type, meeting_date, start_time, end_time, location "
            "FROM meetings "
            "ORDER BY CASE WHEN meeting_date IS NULL OR trim(meeting_date) = '' "
            "THEN 1 ELSE 0 END ASC, meeting_date DESC, meeting_id ASC;",
            [](Statement& statement) {
                TableRow row;
                row.id = statement.GetInt64(0);
                row.cells = {statement.GetText(1), statement.GetText(2), statement.GetText(3),
                             statement.GetText(4), statement.GetText(5), statement.GetText(6)};
                row.filterValue = statement.GetText(2);
                return row;
            });
    } else if (pageTitle == "All Projects") {
        formTitle = "New / Edit Project";
        options.columns = {{"Title", 200}, {"Status", 100}, {"Start", 110},
                           {"Target End", 110}, {"Actual End", 110}};
        options.filterOptions = {{"Planned", "Planned"}, {"Ongoing", "Ongoing"},
                                 {"Completed", "Completed"}, {"Cancelled", "Cancelled"}};
        options.rows = loadRows(
            "SELECT project_id, title, status, start_date, target_end_date, actual_end_date "
            "FROM projects ORDER BY title COLLATE NOCASE ASC, project_id ASC;",
            [](Statement& statement) {
                TableRow row;
                row.id = statement.GetInt64(0);
                row.cells = {statement.GetText(1), statement.GetText(2), statement.GetText(3),
                             statement.GetText(4), statement.GetText(5)};
                row.filterValue = statement.GetText(2);
                return row;
            });
    } else if (pageTitle == "All Appointments" || pageTitle == "Current Committee") {
        formTitle = "New / Edit Appointment";
        options.columns = {{"Office", 180}, {"Member", 180},
                           {"Start Date", 110}, {"End Date", 110}};
        wxString sql =
            "SELECT ca.start_date, ca.office, ca.end_date, m.name "
            "FROM committee_appointments AS ca "
            "JOIN members AS m ON m.member_id = ca.member_id ";
        if (pageTitle == "Current Committee") {
            sql +=
                "WHERE start_date <= date('now', 'localtime') "
                "AND (end_date IS NULL OR end_date >= date('now', 'localtime')) "
                "ORDER BY office COLLATE NOCASE ASC, start_date DESC;";
        } else {
            sql += "ORDER BY ca.start_date DESC, ca.office COLLATE NOCASE ASC;";
        }
        options.rows = loadRows(sql, [](Statement& statement) {
            TableRow row;
            const wxString startDate = statement.GetText(0);
            row.recordKey = startDate;
            row.recordKey.Append(static_cast<wxChar>(0x1f));
            row.recordKey.Append(statement.GetText(1));
            row.cells = {statement.GetText(1), statement.GetText(3), startDate,
                         statement.GetText(2)};
            return row;
        });
    } else if (pageTitle == "All Activities") {
        formTitle = "New / Edit Activity";
        options.columns = {{"Title", 180}, {"Type", 100}, {"Date", 110},
                           {"Start", 80}, {"End", 80}, {"Location", 160}};
        options.showFilter = true;
        if (m_dbService.IsOpen()) {
            wxString error;
            auto typeStatement = m_dbService.Prepare(
                "SELECT DISTINCT type FROM activities "
                "WHERE type IS NOT NULL AND trim(type) <> '' "
                "ORDER BY type COLLATE NOCASE ASC;",
                &error);
            if (!typeStatement) {
                ShowDatabaseError(this, "Database Query Error",
                                  wxString("Could not load activity types:\n\n") + error);
            } else {
                while (typeStatement->Step()) {
                    const wxString type = typeStatement->GetText(0);
                    options.filterOptions.push_back({type, type});
                }
            }
        }
        options.rows = loadRows(
            "SELECT activity_id, title, type, activity_date, start_time, end_time, location "
            "FROM activities "
            "ORDER BY CASE WHEN activity_date IS NULL OR trim(activity_date) = '' "
            "THEN 1 ELSE 0 END ASC, activity_date DESC, activity_id ASC;",
            [](Statement& statement) {
                TableRow row;
                row.id = statement.GetInt64(0);
                row.cells = {statement.GetText(1), statement.GetText(2), statement.GetText(3),
                             statement.GetText(4), statement.GetText(5), statement.GetText(6)};
                row.filterValue = statement.GetText(2);
                return row;
            });
    } else if (pageTitle == "All Competitions") {
        formTitle = "New / Edit Competitions";
        options.columns = {{"Title", 240}, {"Year", 80}};
        options.rows = loadRows(
            "SELECT competition_id, title, year FROM external_competitions "
            "ORDER BY title COLLATE NOCASE ASC, year ASC;",
            [](Statement& statement) {
                TableRow row;
                row.id = statement.GetInt64(0);
                row.cells = {statement.GetText(1), statement.GetText(2)};
                return row;
            });
    } else {
        return nullptr;
    }

    // Defer workspace replacement until the button event has returned; this
    // page owns the button handler and will be destroyed by the navigation.
    options.onNew = [this, formTitle]() {
        CallAfter([this, formTitle]() {
            (void)OpenFormFromTable(formTitle, std::nullopt);
        });
    };
    options.onEdit = [this, formTitle](long long id) {
        CallAfter([this, formTitle, id]() {
            (void)OpenFormFromTable(formTitle, wxString::Format("%lld", id));
        });
    };
    options.onEditKey = [this, formTitle](const wxString& key) {
        CallAfter([this, formTitle, key]() {
            (void)OpenFormFromTable(formTitle, key);
        });
    };

    return new TablePage(m_workspacePanel, std::move(options));
}

wxWindow* MainFrame::CreateScopedTablePage(const wxString& pageTitle) {
    if (!m_dbService.IsOpen()) {
        return nullptr;
    }

    const auto loadParents = [this](const wxString& sql, const auto& makeParent) {
        std::vector<ScopedParent> parents;
        wxString error;
        auto statement = m_dbService.Prepare(sql, &error);
        if (!statement) {
            ShowDatabaseError(this, "Database Query Error",
                              wxString("Could not load parent records:\n\n") + error);
            return parents;
        }
        while (statement->Step()) {
            parents.push_back(makeParent(*statement));
        }
        return parents;
    };

    const auto makeRowLoader = [this](const wxString& sql) -> ScopedRowLoader {
        return [this, sql](long long parentId) -> std::vector<ScopedRow> {
            std::vector<ScopedRow> rows;
            wxString error;
            auto statement = m_dbService.Prepare(sql, &error);
            if (!statement) {
                ShowDatabaseError(this, "Database Query Error",
                                  wxString("Could not load scoped rows:\n\n") + error);
                return rows;
            }
            statement->BindInt64(1, parentId);
            while (statement->Step()) {
                ScopedRow row;
                const int colCount = statement->GetColumnCount();
                for (int col = 0; col < colCount; ++col) {
                    row.cells.push_back(statement->GetText(col));
                }
                rows.push_back(std::move(row));
            }
            return rows;
        };
    };

    ScopedPageOptions opts;

    if (pageTitle == "Attendees") {
        opts.title = "Meeting";
        opts.emptyMessage =
            "No meetings yet \xe2\x80\x94 create one to see attendees.";
        opts.columns = {{"Member ID", 80}, {"Name", 180}, {"Phone", 130}, {"Email", 220}};
        opts.parents = loadParents(
            "SELECT meeting_id, title, meeting_date FROM meetings "
            "ORDER BY CASE WHEN meeting_date IS NULL OR trim(meeting_date) = '' "
            "THEN 1 ELSE 0 END ASC, meeting_date DESC, meeting_id ASC;",
            [](Statement& s) {
                ScopedParent p;
                p.id = s.GetInt64(0);
                const wxString date = s.GetText(2);
                p.label = s.GetText(1);
                if (!date.IsEmpty()) {
                    p.label += wxString::FromUTF8(" \xe2\x80\x94 ") + date;
                }
                return p;
            });
        opts.rowLoader = makeRowLoader(
            "SELECT m.member_id, m.name, m.phone, m.email "
            "FROM meeting_attendance AS ma "
            "JOIN members AS m ON m.member_id = ma.member_id "
            "WHERE ma.meeting_id = ? "
            "ORDER BY m.name COLLATE NOCASE ASC;");
    } else if (pageTitle == "Project Teams") {
        opts.title = "Project";
        opts.emptyMessage =
            "No projects yet \xe2\x80\x94 create one to see project teams.";
        opts.columns = {
            {"Member ID", 80}, {"Name", 180}, {"Phone", 130}, {"Email", 220}, {"Role", 100}};
        opts.parents = loadParents(
            "SELECT project_id, title FROM projects "
            "ORDER BY title COLLATE NOCASE ASC, project_id ASC;",
            [](Statement& s) {
                ScopedParent p;
                p.id = s.GetInt64(0);
                p.label = s.GetText(1);
                return p;
            });
        opts.rowLoader = makeRowLoader(
            "SELECT m.member_id, m.name, m.phone, m.email, pm.role "
            "FROM project_members AS pm "
            "JOIN members AS m ON m.member_id = pm.member_id "
            "WHERE pm.project_id = ? "
            "ORDER BY m.name COLLATE NOCASE ASC;");
    } else if (pageTitle == "Competition Teams") {
        opts.title = "Competition";
        opts.emptyMessage =
            "No competitions yet \xe2\x80\x94 create one to see competition teams.";
        opts.columns = {{"Member ID", 80}, {"Name", 180}, {"Role", 140}};
        opts.parents = loadParents(
            "SELECT competition_id, title, year FROM external_competitions "
            "ORDER BY title COLLATE NOCASE ASC, year ASC;",
            [](Statement& s) {
                ScopedParent p;
                p.id = s.GetInt64(0);
                p.label = s.GetText(1) + wxString::FromUTF8(" \xe2\x80\x94 ") + s.GetText(2);
                return p;
            });
        opts.rowLoader = makeRowLoader(
            "SELECT m.member_id, m.name, cp.role "
            "FROM competition_participants AS cp "
            "JOIN members AS m ON m.member_id = cp.member_id "
            "WHERE cp.competition_id = ? "
            "ORDER BY m.name COLLATE NOCASE ASC;");
    } else {
        return nullptr;
    }

    return new ScopedPage(m_workspacePanel, std::move(opts));
}

bool MainFrame::OpenFormFromTable(
    const wxString& formTitle, const std::optional<wxString>& recordKey) {
    const auto root = m_navigationTree->GetRootItem();
    std::function<wxTreeItemId(const wxTreeItemId&)> findPage =
        [this, &findPage, &formTitle](const wxTreeItemId& parent) -> wxTreeItemId {
        wxTreeItemIdValue cookie;
        for (wxTreeItemId child = m_navigationTree->GetFirstChild(parent, cookie);
             child.IsOk(); child = m_navigationTree->GetNextChild(parent, cookie)) {
            const auto* data = dynamic_cast<const NavigationItemData*>(
                m_navigationTree->GetItemData(child));
            if (data && data->IsPage() && data->PageTitle() == formTitle) {
                return child;
            }
            const wxTreeItemId nested = findPage(child);
            if (nested.IsOk()) {
                return nested;
            }
        }
        return wxTreeItemId();
    };

    const wxTreeItemId target = findPage(root);
    if (!target.IsOk()) {
        ShowDatabaseError(this, "Navigation Error",
                          wxString("Could not find form page: ") + formTitle);
        return false;
    }

    m_pendingFormOpen = true;
    m_pendingFormTitle = formTitle;
    m_pendingFormRecordKey = recordKey;
    m_navigationTree->SelectItem(target);

    // Some native tree controls do not emit a changing event for programmatic
    // selection. Call the controller directly if the event did not navigate.
    if (m_activeNavigationItem != target && !NavigateToNavigationItem(target)) {
        m_pendingFormOpen = false;
        m_pendingFormTitle.Clear();
        m_pendingFormRecordKey.reset();
        return false;
    }

    m_pendingFormOpen = false;
    m_pendingFormTitle.Clear();
    m_pendingFormRecordKey.reset();
    return m_activeNavigationItem == target;
}

bool MainFrame::SetWorkspaceView(wxWindow* view) {
    if (view == m_activeWorkspaceView) {
        return true;
    }

    wxWindow* const previousParent = view ? view->GetParent() : nullptr;
    wxSizer* const previousSizer =
        (view && previousParent) ? FindContainingSizer(previousParent->GetSizer(), view) : nullptr;

    if (view && previousParent != m_workspacePanel && !view->Reparent(m_workspacePanel)) {
        return false;
    }

    // Detach the incoming view before Clear(true), which destroys managed windows.
    if (previousSizer && !previousSizer->Detach(view)) {
        if (previousParent && view->GetParent() != previousParent) {
            view->Reparent(previousParent);
        }
        return false;
    }

    if (previousParent && previousParent != m_workspacePanel) {
        previousParent->Layout();
    }

    m_workspaceSizer->Clear(true);
    m_activeWorkspaceView = view;

    if (m_activeWorkspaceView) {
        m_workspaceSizer->Add(m_activeWorkspaceView, 1, wxEXPAND);
    }

    m_workspacePanel->Layout();
    return true;
}

void MainFrame::OnOpenDatabase(wxCommandEvent& /*event*/) {
    if (!ConfirmLeaveActiveForm()) {
        return;
    }

    wxFileDialog dialog(
        this, "Open Database", wxEmptyString, wxEmptyString,
        "SQLite Database (*.db;*.sqlite;*.sqlite3)|*.db;*.sqlite;*.sqlite3|All Files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString path = dialog.GetPath();

    // Validate with a separate connection first. A failed selection must not close
    // the database already being used by the main frame.
    wxString error;
    {
        DbService candidate;
        if (!candidate.Open(path, &error) || !candidate.CheckCompatibility(&error)) {
            ShowDatabaseError(this, "Database Open Error",
                              wxString("Could not open a compatible MSMS database:\n\n") + error);
            return;
        }
    }

    if (!ReopenDatabase(path, &error)) {
        ShowDatabaseError(this, "Database Open Error",
                          wxString("Could not switch to the selected database:\n\n") + error);
    } else {
        // Do not leave a form bound to records from the previous database open.
        ReturnToTablePage("All Members");
    }
}

void MainFrame::OnCreateNewDatabase(wxCommandEvent& /*event*/) {
    if (!ConfirmLeaveActiveForm()) {
        return;
    }

    wxFileDialog dialog(this, "Create New Database", wxEmptyString, "members.db",
                        "SQLite Database (*.db)|*.db|All Files (*.*)|*.*", wxFD_SAVE);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString path = dialog.GetPath();
    if (wxDirExists(path)) {
        ShowDatabaseError(this, "Invalid Database Path",
                          "The selected path is a directory. Choose a database filename instead.");
        return;
    }

    if (wxFileExists(path)) {
        wxMessageDialog confirmation(
            this,
            wxString("The file '") + path +
                "' already exists. Creating a new database will replace this file and all records "
                "it contains. This action cannot be undone.\n\nDo you want to continue?",
            "Confirm Database Replacement", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
        if (confirmation.ShowModal() != wxID_YES) {
            return;
        }
    }

    wxFileName destination(path);
    destination.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    const wxString tempPrefix =
        destination.GetPathWithSep() + "." + destination.GetFullName() + ".tmp.";
    const wxString tempPath = wxFileName::CreateTempFileName(tempPrefix);
    if (tempPath.IsEmpty()) {
        ShowDatabaseError(this, "Database Creation Error",
                          "Could not create a temporary database file in the destination directory.");
        return;
    }

    wxString error;
    {
        DbService candidate;
        if (!candidate.CreateNew(tempPath, &error) || !candidate.CheckCompatibility(&error)) {
            candidate.Close();
            wxRemoveFile(DatabasePathLockFile(tempPath));
            wxRemoveFile(tempPath);
            ShowDatabaseError(this, "Database Creation Error",
                              wxString("Could not initialise a new database:\n\n") + error);
            return;
        }
    }

    // The temporary database is no longer an openable path after it is renamed;
    // remove its now-unused coordination file after the candidate connection closed.
    wxRemoveFile(DatabasePathLockFile(tempPath));

    const wxString previousPath = m_dbService.GetDbPath();
    const bool replacingOpenDatabase =
        m_dbService.IsOpen() && SameFilePath(previousPath, path);
    if (replacingOpenDatabase) {
        m_dbService.Close();
    }

    if (!wxRenameFile(tempPath, path, true)) {
        if (wxFileExists(tempPath)) {
            wxRemoveFile(tempPath);
        }
        if (replacingOpenDatabase) {
            wxString reopenError;
            ReopenDatabase(previousPath, &reopenError);
        }
        ShowDatabaseError(this, "Database Creation Error",
                          "The initialised database could not be moved to the selected location. "
                          "The existing file has not been intentionally deleted by this operation.");
        return;
    }

    if (!ReopenDatabase(path, &error)) {
        ShowDatabaseError(this, "Database Creation Error",
                          wxString("The new database was created, but could not be opened:\n\n") +
                              error);
    } else {
        // The current form belongs to the previous database, if one was open.
        ReturnToTablePage("All Members");
    }
}

void MainFrame::OnSaveBackup(wxCommandEvent& /*event*/) {
    if (!ConfirmLeaveActiveForm()) {
        return;
    }

    if (!m_dbService.IsOpen()) {
        ShowDatabaseError(this, "Backup Error", "There is no open database to back up.");
        return;
    }

    wxFileDialog dialog(this, "Save Database Backup", wxEmptyString, "members-backup.db",
                        "SQLite Database (*.db)|*.db|All Files (*.*)|*.*", wxFD_SAVE);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString path = dialog.GetPath();
    if (SameFilePath(path, m_dbService.GetDbPath())) {
        ShowDatabaseError(this, "Backup Error",
                          "The backup destination must not be the currently open database file.");
        return;
    }

    if (wxFileExists(path)) {
        const int answer = wxMessageBox(
            wxString("The file '") + path +
                "' already exists. Replacing it will overwrite that backup file.\n\nContinue?",
            "Confirm Backup Replacement", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this);
        if (answer != wxYES) {
            return;
        }
    }

    wxFileName destination(path);
    destination.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    const wxString tempPrefix =
        destination.GetPathWithSep() + "." + destination.GetFullName() + ".tmp.";
    const wxString tempPath = wxFileName::CreateTempFileName(tempPrefix);
    if (tempPath.IsEmpty()) {
        ShowDatabaseError(this, "Backup Error",
                          "Could not create a temporary backup file in the destination directory.");
        return;
    }

    wxString error;
    if (!m_dbService.BackupTo(tempPath, &error)) {
        wxRemoveFile(tempPath);
        ShowDatabaseError(this, "Backup Error",
                          wxString("The database backup failed:\n\n") + error);
        return;
    }

    if (!wxRenameFile(tempPath, path, true)) {
        if (wxFileExists(tempPath)) {
            wxRemoveFile(tempPath);
        }
        ShowDatabaseError(this, "Backup Error",
                          "The backup was created but could not be moved to the selected path.");
        return;
    }

    wxMessageBox("Database backup saved successfully.", "Backup Complete",
                 wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnRestoreBackup(wxCommandEvent& /*event*/) {
    if (!m_dbService.IsOpen()) {
        ShowDatabaseError(this, "Restore Error", "There is no open database to restore.");
        return;
    }

    // Resolve any pending form edits before a restore can replace the database
    // those forms were editing.
    if (!ConfirmLeaveActiveForm()) {
        return;
    }

    const wxString activePath = m_dbService.GetDbPath();
    wxMessageDialog confirmation(
        this,
        wxString("Restoring a backup will replace all records in '") +
            wxFileName(activePath).GetFullName() +
            "'. A timestamped safety copy of the current database will be saved beside it first. "
            "Only a backup that passes integrity and schema validation will be installed.\n\n"
            "Continue to choose a backup file?",
        "Confirm Database Restore", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    if (confirmation.ShowModal() != wxID_YES) {
        return;
    }

    wxFileDialog dialog(
        this, "Select Database Backup", wxEmptyString, wxEmptyString,
        "SQLite Database (*.db;*.sqlite;*.sqlite3)|*.db;*.sqlite;*.sqlite3|All Files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString backupPath = dialog.GetPath();
    if (SameFilePath(backupPath, activePath)) {
        ShowDatabaseError(this, "Restore Error",
                          "The selected backup is the currently open database.");
        return;
    }

    wxString error;
    if (!m_dbService.RestoreFrom(backupPath, &error)) {
        const wxString restoreError = error;
        if (!m_dbService.IsOpen()) {
            wxString reopenError;
            if (!ReopenDatabase(activePath, &reopenError) ||
                !m_dbService.CheckIntegrity(&reopenError)) {
                m_dbService.Close();
                UpdateTitle();
                error = restoreError + "\n\nCould not reopen a verified working database: " +
                        reopenError;
            } else {
                error = restoreError;
            }
        } else {
            error = restoreError;
        }
        ShowDatabaseError(this, "Restore Error",
                          wxString("Could not restore the database:\n\n") + error);
        return;
    }

    UpdateTitle();
    ReturnToTablePage("All Members");
    // ReturnToTablePage can be a no-op when All Members was already selected;
    // force a refresh in that case too.
    RefreshActiveTablePage();
    wxMessageBox("Database restored successfully.", "Restore Complete",
                 wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnExit(wxCommandEvent& /*event*/) {
    // Route menu exit through the close event so it uses the same dirty-form guard
    // as the window manager's close button.
    Close(false);
}

void MainFrame::OnClose(wxCloseEvent& event) {
    if (event.CanVeto() && !ConfirmLeaveActiveForm()) {
        event.Veto();
        return;
    }
    event.Skip();
}

void MainFrame::OnAbout(wxCommandEvent& /*event*/) {
    wxDialog dialog(this, wxID_ANY, "About MSMS Manager", wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE);
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(&dialog, wxID_ANY, "MSMS Manager");
    wxFont titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 3);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(titleFont);

    auto* version = new wxStaticText(
        &dialog, wxID_ANY, wxString("Version ") + kApplicationVersion);
    auto* purpose = new wxStaticText(
        &dialog, wxID_ANY,
        "An offline desktop application for managing MSMS membership and society records.");
    purpose->Wrap(440);

    auto* licenceHeading = new wxStaticText(&dialog, wxID_ANY, "Licence");
    wxFont licenceFont = licenceHeading->GetFont();
    licenceFont.SetWeight(wxFONTWEIGHT_BOLD);
    licenceHeading->SetFont(licenceFont);

    auto* licence = new wxStaticText(
        &dialog, wxID_ANY,
        "GNU General Public License v3.0 or later (GPL-3.0-or-later).\n"
        "The full licence text is distributed with this application in the LICENSE file.");
    licence->Wrap(440);

    rootSizer->Add(title, 0, wxALL, 12);
    rootSizer->Add(version, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    rootSizer->Add(purpose, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    rootSizer->Add(licenceHeading, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    rootSizer->Add(licence, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* okButton = new wxButton(&dialog, wxID_OK, "OK");
    okButton->SetDefault();
    rootSizer->Add(okButton, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    dialog.SetSizerAndFit(rootSizer);
    dialog.SetMinSize(wxSize(480, dialog.GetSize().GetHeight()));
    dialog.CentreOnParent();
    dialog.ShowModal();
}

void MainFrame::RememberDatabasePath(const wxString& path) {
    wxFileConfig config("MSMSManager");
    config.Write("LastDatabasePath", path);
    if (!config.Flush()) {
        wxMessageBox("The database is open, but its path could not be saved for the next launch.",
                     "Configuration Warning", wxOK | wxICON_WARNING, this);
    }
}

bool MainFrame::ReopenDatabase(const wxString& path, wxString* outError) {
    const wxString previousPath = m_dbService.GetDbPath();
    wxString error;
    if (m_dbService.Open(path, &error) && m_dbService.CheckCompatibility(&error)) {
        UpdateTitle();
        RememberDatabasePath(path);
        RefreshActiveTablePage();
        return true;
    }

    const wxString switchError = error;
    m_dbService.Close();

    // Restore the prior connection when a switch failed after pre-validation.
    if (!previousPath.IsEmpty() && wxFileExists(previousPath)) {
        wxString restoreError;
        if (m_dbService.Open(previousPath, &restoreError) &&
            m_dbService.CheckCompatibility(&restoreError)) {
            UpdateTitle();
        } else {
            m_dbService.Close();
            UpdateTitle();
        }
    } else {
        UpdateTitle();
    }

    if (outError) {
        *outError = switchError;
    }
    return false;
}

void MainFrame::RefreshActiveTablePage() {
    if ((!dynamic_cast<TablePage*>(m_activeWorkspaceView) &&
         !dynamic_cast<ScopedPage*>(m_activeWorkspaceView)) ||
        !m_activeNavigationItem.IsOk()) {
        return;
    }

    const auto* data =
        dynamic_cast<const NavigationItemData*>(m_navigationTree->GetItemData(m_activeNavigationItem));
    if (!data || !data->IsPage()) {
        return;
    }

    wxWindow* const refreshedView = CreateNavigationPage(data->PageTitle(), false);
    if (!refreshedView || !SetWorkspaceView(refreshedView)) {
        if (refreshedView) {
            refreshedView->Destroy();
        }
        ShowDatabaseError(this, "Refresh Error",
                          "The active table could not be refreshed. Its existing view was kept.");
    }
}

void MainFrame::ReturnToTablePage(const wxString& pageTitle) {
    if (!m_navigationTree) {
        return;
    }

    const wxTreeItemId root = m_navigationTree->GetRootItem();
    std::function<wxTreeItemId(const wxTreeItemId&)> findTable =
        [this, &findTable, &pageTitle](const wxTreeItemId& parent) -> wxTreeItemId {
        wxTreeItemIdValue cookie;
        for (wxTreeItemId child = m_navigationTree->GetFirstChild(parent, cookie);
             child.IsOk(); child = m_navigationTree->GetNextChild(parent, cookie)) {
            const auto* data = dynamic_cast<const NavigationItemData*>(
                m_navigationTree->GetItemData(child));
            if (data && data->IsPage() && data->PageTitle() == pageTitle &&
                data->PageType() && *data->PageType() == NavigationPageType::Table) {
                return child;
            }
            const wxTreeItemId nested = findTable(child);
            if (nested.IsOk()) {
                return nested;
            }
        }
        return wxTreeItemId();
    };

    const wxTreeItemId target = findTable(root);
    if (!target.IsOk()) {
        ShowDatabaseError(this, "Navigation Error",
                          "Could not find the parent table page: " + pageTitle);
        return;
    }

    m_navigationTree->SelectItem(target);
    // As with opening a form, call the controller explicitly if the native tree
    // did not emit a selection-changing event for the programmatic selection.
    if (m_activeNavigationItem != target && !NavigateToNavigationItem(target)) {
        if (m_activeNavigationItem.IsOk()) {
            m_navigationTree->SelectItem(m_activeNavigationItem);
        }
    }
}

void MainFrame::UpdateTitle() {
    if (m_dbService.IsOpen()) {
        const wxFileName databaseFile(m_dbService.GetDbPath());
        SetTitle(wxString::FromUTF8("MSMS Manager \xE2\x80\x94 ") + databaseFile.GetFullName());
    } else {
        SetTitle("MSMS Manager");
    }
}

} // namespace msms
