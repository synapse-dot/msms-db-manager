#include "MainFrame.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <wx/accel.h>
#include <wx/aboutdlg.h>
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
#include "util/FileIdentity.h"

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

bool ConfirmDestinationIdentityIsStable(wxWindow* parent, const wxString& path,
                                        msms::FileIdentity& approvedIdentity) {
    if (!approvedIdentity.valid) {
        ShowDatabaseError(parent, "Database Creation Error",
                          "The destination file identity could not be determined safely. "
                          "No replacement was performed.");
        return false;
    }

    while (true) {
        const msms::FileIdentity currentIdentity = msms::CaptureFileIdentity(path);
        if (!currentIdentity.valid) {
            ShowDatabaseError(parent, "Database Creation Error",
                              "The destination file identity can no longer be determined. "
                              "No replacement was performed.");
            return false;
        }
        if (currentIdentity == approvedIdentity) {
            return true;
        }

        // The user previously consented to a different destination state. Capture
        // the new identity before prompting, then loop and check again after the
        // dialog closes so a change while the prompt is open causes another prompt.
        wxMessageDialog changedDialog(
            parent,
            wxString("The destination file '") + path +
                "' changed while the new database was being created. Another process may have "
                "created, replaced, or removed it.\n\nDo you still want to replace the destination "
                "in its current state?",
            "Destination Changed", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
        if (changedDialog.ShowModal() != wxID_YES) {
            return false;
        }
        approvedIdentity = currentIdentity;
    }
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

    // Capture what the user is being asked to replace. Unknown identity is not
    // treated as absence, because that would bypass the overwrite confirmation.
    msms::FileIdentity approvedIdentity = msms::CaptureFileIdentity(path);
    if (!approvedIdentity.valid) {
        ShowDatabaseError(this, "Database Creation Error",
                          "The destination file identity could not be determined safely.");
        return;
    }

    if (approvedIdentity.exists) {
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

    // Recheck after the temporary database is ready and the active connection (if
    // this path is open here) has closed. This narrows the check/rename race; it is
    // not an atomic compare-and-swap against arbitrary external filesystem writers.
    if (!ConfirmDestinationIdentityIsStable(this, path, approvedIdentity)) {
        wxRemoveFile(tempPath);
        if (replacingOpenDatabase) {
            wxString reopenError;
            if (!ReopenDatabase(previousPath, &reopenError)) {
                ShowDatabaseError(this, "Database Reopen Error",
                                  wxString("The create operation was cancelled, but the previous "
                                           "database could not be reopened:\n\n") + reopenError);
            }
        }
        return;
    }

    if (!wxRenameFile(tempPath, path, true)) {
        if (wxFileExists(tempPath)) {
            wxRemoveFile(tempPath);
        }
        if (replacingOpenDatabase) {
            wxString reopenError;
            if (!ReopenDatabase(previousPath, &reopenError)) {
                ShowDatabaseError(this, "Database Reopen Error",
                                  wxString("The previous database could not be reopened:\n\n") +
                                      reopenError);
            }
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
    wxAboutDialogInfo info;
    info.SetName("MSMS Manager");
    info.SetVersion(kApplicationVersion);
    info.SetLicence(wxString::FromUTF8(R"GPL3LICENSE(
                    GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007

 Copyright (C) 2007 Free Software Foundation, Inc. <http://fsf.org/>
 Everyone is permitted to copy and distribute verbatim copies
 of this license document, but changing it is not allowed.

                            Preamble

  The GNU General Public License is a free, copyleft license for
software and other kinds of works.

  The licenses for most software and other practical works are designed
to take away your freedom to share and change the works.  By contrast,
the GNU General Public License is intended to guarantee your freedom to
share and change all versions of a program--to make sure it remains free
software for all its users.  We, the Free Software Foundation, use the
GNU General Public License for most of our software; it applies also to
any other work released this way by its authors.  You can apply it to
your programs, too.

  When we speak of free software, we are referring to freedom, not
price.  Our General Public Licenses are designed to make sure that you
have the freedom to distribute copies of free software (and charge for
them if you wish), that you receive source code or can get it if you
want it, that you can change the software or use pieces of it in new
free programs, and that you know you can do these things.

  To protect your rights, we need to prevent others from denying you
these rights or asking you to surrender the rights.  Therefore, you have
certain responsibilities if you distribute copies of the software, or if
you modify it: responsibilities to respect the freedom of others.

  For example, if you distribute copies of such a program, whether
gratis or for a fee, you must pass on to the recipients the same
freedoms that you received.  You must make sure that they, too, receive
or can get the source code.  And you must show them these terms so they
know their rights.

  Developers that use the GNU GPL protect your rights with two steps:
(1) assert copyright on the software, and (2) offer you this License
giving you legal permission to copy, distribute and/or modify it.

  For the developers' and authors' protection, the GPL clearly explains
that there is no warranty for this free software.  For both users' and
authors' sake, the GPL requires that modified versions be marked as
changed, so that their problems will not be attributed erroneously to
authors of previous versions.

  Some devices are designed to deny users access to install or run
modified versions of the software inside them, although the manufacturer
can do so.  This is fundamentally incompatible with the aim of
protecting users' freedom to change the software.  The systematic
pattern of such abuse occurs in the area of products for individuals to
use, which is precisely where it is most unacceptable.  Therefore, we
have designed this version of the GPL to prohibit the practice for those
products.  If such problems arise substantially in other domains, we
stand ready to extend this provision to those domains in future versions
of the GPL, as needed to protect the freedom of users.

  Finally, every program is threatened constantly by software patents.
States should not allow patents to restrict development and use of
software on general-purpose computers, but in those that do, we wish to
avoid the special danger that patents applied to a free program could
make it effectively proprietary.  To prevent this, the GPL assures that
patents cannot be used to render the program non-free.

  The precise terms and conditions for copying, distribution and
modification follow.

                       TERMS AND CONDITIONS

  0. Definitions.

  "This License" refers to version 3 of the GNU General Public License.

  "Copyright" also means copyright-like laws that apply to other kinds of
works, such as semiconductor masks.

  "The Program" refers to any copyrightable work licensed under this
License.  Each licensee is addressed as "you".  "Licensees" and
"recipients" may be individuals or organizations.

  To "modify" a work means to copy from or adapt all or part of the work
in a fashion requiring copyright permission, other than the making of an
exact copy.  The resulting work is called a "modified version" of the
earlier work or a work "based on" the earlier work.

  A "covered work" means either the unmodified Program or a work based
on the Program.

  To "propagate" a work means to do anything with it that, without
permission, would make you directly or secondarily liable for
infringement under applicable copyright law, except executing it on a
computer or modifying a private copy.  Propagation includes copying,
distribution (with or without modification), making available to the
public, and in some countries other activities as well.

  To "convey" a work means any kind of propagation that enables other
parties to make or receive copies.  Mere interaction with a user through
a computer network, with no transfer of a copy, is not conveying.

  An interactive user interface displays "Appropriate Legal Notices"
to the extent that it includes a convenient and prominently visible
feature that (1) displays an appropriate copyright notice, and (2)
tells the user that there is no warranty for the work (except to the
extent that warranties are provided), that licensees may convey the
work under this License, and how to view a copy of this License.  If
the interface presents a list of user commands or options, such as a
menu, a prominent item in the list meets this criterion.

  1. Source Code.

  The "source code" for a work means the preferred form of the work
for making modifications to it.  "Object code" means any non-source
form of a work.

  A "Standard Interface" means an interface that either is an official
standard defined by a recognized standards body, or, in the case of
interfaces specified for a particular programming language, one that
is widely used among developers working in that language.

  The "System Libraries" of an executable work include anything, other
than the work as a whole, that (a) is included in the normal form of
packaging a Major Component, but which is not part of that Major
Component, and (b) serves only to enable use of the work with that
Major Component, or to implement a Standard Interface for which an
implementation is available to the public in source code form.  A
"Major Component", in this context, means a major essential component
(kernel, window system, and so on) of the specific operating system
(if any) on which the executable work runs, or a compiler used to
produce the work, or an object code interpreter used to run it.

  The "Corresponding Source" for a work in object code form means all
the source code needed to generate, install, and (for an executable
work) run the object code and to modify the work, including scripts to
control those activities.  However, it does not include the work's
System Libraries, or general-purpose tools or generally available free
programs which are used unmodified in performing those activities but
which are not part of the work.  For example, Corresponding Source
includes interface definition files associated with source files for
the work, and the source code for shared libraries and dynamically
linked subprograms that the work is specifically designed to require,
such as by intimate data communication or control flow between those
subprograms and other parts of the work.

  The Corresponding Source need not include anything that users
can regenerate automatically from other parts of the Corresponding
Source.

  The Corresponding Source for a work in source code form is that
same work.

  2. Basic Permissions.

  All rights granted under this License are granted for the term of
copyright on the Program, and are irrevocable provided the stated
conditions are met.  This License explicitly affirms your unlimited
permission to run the unmodified Program.  The output from running a
covered work is covered by this License only if the output, given its
content, constitutes a covered work.  This License acknowledges your
rights of fair use or other equivalent, as provided by copyright law.

  You may make, run and propagate covered works that you do not
convey, without conditions so long as your license otherwise remains
in force.  You may convey covered works to others for the sole purpose
of having them make modifications exclusively for you, or provide you
with facilities for running those works, provided that you comply with
the terms of this License in conveying all material for which you do
not control copyright.  Those thus making or running the covered works
for you must do so exclusively on your behalf, under your direction
and control, on terms that prohibit them from making any copies of
your copyrighted material outside their relationship with you.

  Conveying under any other circumstances is permitted solely under
the conditions stated below.  Sublicensing is not allowed; section 10
makes it unnecessary.

  3. Protecting Users' Legal Rights From Anti-Circumvention Law.

  No covered work shall be deemed part of an effective technological
measure under any applicable law fulfilling obligations under article
11 of the WIPO copyright treaty adopted on 20 December 1996, or
similar laws prohibiting or restricting circumvention of such
measures.

  When you convey a covered work, you waive any legal power to forbid
circumvention of technological measures to the extent such circumvention
is effected by exercising rights under this License with respect to
the covered work, and you disclaim any intention to limit operation or
modification of the work as a means of enforcing, against the work's
users, your or third parties' legal rights to forbid circumvention of
technological measures.

  4. Conveying Verbatim Copies.

  You may convey verbatim copies of the Program's source code as you
receive it, in any medium, provided that you conspicuously and
appropriately publish on each copy an appropriate copyright notice;
keep intact all notices stating that this License and any
non-permissive terms added in accord with section 7 apply to the code;
keep intact all notices of the absence of any warranty; and give all
recipients a copy of this License along with the Program.

  You may charge any price or no price for each copy that you convey,
and you may offer support or warranty protection for a fee.

  5. Conveying Modified Source Versions.

  You may convey a work based on the Program, or the modifications to
produce it from the Program, in the form of source code under the
terms of section 4, provided that you also meet all of these conditions:

    a) The work must carry prominent notices stating that you modified
    it, and giving a relevant date.

    b) The work must carry prominent notices stating that it is
    released under this License and any conditions added under section
    7.  This requirement modifies the requirement in section 4 to
    "keep intact all notices".

    c) You must license the entire work, as a whole, under this
    License to anyone who comes into possession of a copy.  This
    License will therefore apply, along with any applicable section 7
    additional terms, to the whole of the work, and all its parts,
    regardless of how they are packaged.  This License gives no
    permission to license the work in any other way, but it does not
    invalidate such permission if you have separately received it.

    d) If the work has interactive user interfaces, each must display
    Appropriate Legal Notices; however, if the Program has interactive
    interfaces that do not display Appropriate Legal Notices, your
    work need not make them do so.

  A compilation of a covered work with other separate and independent
works, which are not by their nature extensions of the covered work,
and which are not combined with it such as to form a larger program,
in or on a volume of a storage or distribution medium, is called an
"aggregate" if the compilation and its resulting copyright are not
used to limit the access or legal rights of the compilation's users
beyond what the individual works permit.  Inclusion of a covered work
in an aggregate does not cause this License to apply to the other
parts of the aggregate.

  6. Conveying Non-Source Forms.

  You may convey a covered work in object code form under the terms
of sections 4 and 5, provided that you also convey the
machine-readable Corresponding Source under the terms of this License,
in one of these ways:

    a) Convey the object code in, or embodied in, a physical product
    (including a physical distribution medium), accompanied by the
    Corresponding Source fixed on a durable physical medium
    customarily used for software interchange.

    b) Convey the object code in, or embodied in, a physical product
    (including a physical distribution medium), accompanied by a
    written offer, valid for at least three years and valid for as
    long as you offer spare parts or customer support for that product
    model, to give anyone who possesses the object code either (1) a
    copy of the Corresponding Source for all the software in the
    product that is covered by this License, on a durable physical
    medium customarily used for software interchange, for a price no
    more than your reasonable cost of physically performing this
    conveying of source, or (2) access to copy the
    Corresponding Source from a network server at no charge.

    c) Convey individual copies of the object code with a copy of the
    written offer to provide the Corresponding Source.  This
    alternative is allowed only occasionally and noncommercially, and
    only if you received the object code with such an offer, in accord
    with subsection 6b.

    d) Convey the object code by offering access from a designated
    place (gratis or for a charge), and offer equivalent access to the
    Corresponding Source in the same way through the same place at no
    further charge.  You need not require recipients to copy the
    Corresponding Source along with the object code.  If the place to
    copy the object code is a network server, the Corresponding Source
    may be on a different server (operated by you or a third party)
    that supports equivalent copying facilities, provided you maintain
    clear directions next to the object code saying where to find the
    Corresponding Source.  Regardless of what server hosts the
    Corresponding Source, you remain obligated to ensure that it is
    available for as long as needed to satisfy these requirements.

    e) Convey the object code using peer-to-peer transmission, provided
    you inform other peers where the object code and Corresponding
    Source of the work are being offered to the general public at no
    charge under subsection 6d.

  A separable portion of the object code, whose source code is excluded
from the Corresponding Source as a System Library, need not be
included in conveying the object code work.

  A "User Product" is either (1) a "consumer product", which means any
tangible personal property which is normally used for personal, family,
or household purposes, or (2) anything designed or sold for incorporation
into a dwelling.  In determining whether a product is a consumer product,
doubtful cases shall be resolved in favor of coverage.  For a particular
product received by a particular user, "normally used" refers to a
typical or common use of that class of product, regardless of the status
of the particular user or of the way in which the particular user
actually uses, or expects or is expected to use, the product.  A product
is a consumer product regardless of whether the product has substantial
commercial, industrial or non-consumer uses, unless such uses represent
the only significant mode of use of the product.

  "Installation Information" for a User Product means any methods,
procedures, authorization keys, or other information required to install
and execute modified versions of a covered work in that User Product from
a modified version of its Corresponding Source.  The information must
suffice to ensure that the continued functioning of the modified object
code is in no case prevented or interfered with solely because
modification has been made.

  If you convey an object code work under this section in, or with, or
specifically for use in, a User Product, and the conveying occurs as
part of a transaction in which the right of possession and use of the
User Product is transferred to the recipient in perpetuity or for a
fixed term (regardless of how the transaction is characterized), the
Corresponding Source conveyed under this section must be accompanied
by the Installation Information.  But this requirement does not apply
if neither you nor any third party retains the ability to install
modified object code on the User Product (for example, the work has
been installed in ROM).

  The requirement to provide Installation Information does not include a
requirement to continue to provide support service, warranty, or updates
for a work that has been modified or installed by the recipient, or for
the User Product in which it has been modified or installed.  Access to a
network may be denied when the modification itself materially and
adversely affects the operation of the network or violates the rules and
protocols for communication across the network.

  Corresponding Source conveyed, and Installation Information provided,
in accord with this section must be in a format that is publicly
documented (and with an implementation available to the public in
source code form), and must require no special password or key for
unpacking, reading or copying.

  7. Additional Terms.

  "Additional permissions" are terms that supplement the terms of this
License by making exceptions from one or more of its conditions.
Additional permissions that are applicable to the entire Program shall
be treated as though they were included in this License, to the extent
that they are valid under applicable law.  If additional permissions
apply only to part of the Program, that part may be used separately
under those permissions, but the entire Program remains governed by
this License without regard to the additional permissions.

  When you convey a copy of a covered work, you may at your option
remove any additional permissions from that copy, or from any part of
it.  (Additional permissions may be written to require their own
removal in certain cases when you modify the work.)  You may place
additional permissions on material, added by you to a covered work,
for which you have or can give appropriate copyright permission.

  Notwithstanding any other provision of this License, for material you
add to a covered work, you may (if authorized by the copyright holders of
that material) supplement the terms of this License with terms:

    a) Disclaiming warranty or limiting liability differently from the
    terms of sections 15 and 16 of this License; or

    b) Requiring preservation of specified reasonable legal notices or
    author attributions in that material or in the Appropriate Legal
    Notices displayed by works containing it; or

    c) Prohibiting misrepresentation of the origin of that material, or
    requiring that modified versions of such material be marked in
    reasonable ways as different from the original version; or

    d) Limiting the use for publicity purposes of names of licensors or
    authors of the material; or

    e) Declining to grant rights under trademark law for use of some
    trade names, trademarks, or service marks; or

    f) Requiring indemnification of licensors and authors of that
    material by anyone who conveys the material (or modified versions of
    it) with contractual assumptions of liability to the recipient, for
    any liability that these contractual assumptions directly impose on
    those licensors and authors.

  All other non-permissive additional terms are considered "further
restrictions" within the meaning of section 10.  If the Program as you
received it, or any part of it, contains a notice stating that it is
governed by this License along with a term that is a further
restriction, you may remove that term.  If a license document contains
a further restriction but permits relicensing or conveying under this
License, you may add to a covered work material governed by the terms
of that license document, provided that the further restriction does
not survive such relicensing or conveying.

  If you add terms to a covered work in accord with this section, you
must place, in the relevant source files, a statement of the
additional terms that apply to those files, or a notice indicating
where to find the applicable terms.

  Additional terms, permissive or non-permissive, may be stated in the
form of a separately written license, or stated as exceptions;
the above requirements apply either way.

  8. Termination.

  You may not propagate or modify a covered work except as expressly
provided under this License.  Any attempt otherwise to propagate or
modify it is void, and will automatically terminate your rights under
this License (including any patent licenses granted under the third
paragraph of section 11).

  However, if you cease all violation of this License, then your
license from a particular copyright holder is reinstated (a)
provisionally, unless and until the copyright holder explicitly and
finally terminates your license, and (b) permanently, if the copyright
holder fails to notify you of the violation by some reasonable means
prior to 60 days after the cessation.

  Moreover, your license from a particular copyright holder is
reinstated permanently if the copyright holder notifies you of the
violation by some reasonable means, this is the first time you have
received notice of violation of this License (for any work) from that
copyright holder, and you cure the violation prior to 30 days after
your receipt of the notice.

  Termination of your rights under this section does not terminate the
licenses of parties who have received copies or rights from you under
this License.  If your rights have been terminated and not permanently
reinstated, you do not qualify to receive new licenses for the same
material under section 10.

  9. Acceptance Not Required for Having Copies.

  You are not required to accept this License in order to receive or
run a copy of the Program.  Ancillary propagation of a covered work
occurring solely as a consequence of using peer-to-peer transmission
to receive a copy likewise does not require acceptance.  However,
nothing other than this License grants you permission to propagate or
modify any covered work.  These actions infringe copyright if you do
not accept this License.  Therefore, by modifying or propagating a
covered work, you indicate your acceptance of this License to do so.

  10. Automatic Licensing of Downstream Recipients.

  Each time you convey a covered work, the recipient automatically
receives a license from the original licensors, to run, modify and
propagate that work, subject to this License.  You are not responsible
for enforcing compliance by third parties with this License.

  An "entity transaction" is a transaction transferring control of an
organization, or substantially all assets of one, or subdividing an
organization, or merging organizations.  If propagation of a covered
work results from an entity transaction, each party to that
transaction who receives a copy of the work also receives whatever
licenses to the work the party's predecessor in interest had or could
give under the previous paragraph, plus a right to possession of the
Corresponding Source of the work from the predecessor in interest, if
the predecessor has it or can get it with reasonable efforts.

  You may not impose any further restrictions on the exercise of the
rights granted or affirmed under this License.  For example, you may
not impose a license fee, royalty, or other charge for exercise of
rights granted under this License, and you may not initiate litigation
(including a cross-claim or counterclaim in a lawsuit) alleging that
any patent claim is infringed by making, using, selling, offering for
sale, or importing the Program or any portion of it.

  11. Patents.

  A "contributor" is a copyright holder who authorizes use under this
License of the Program or a work on which the Program is based.  The
work thus licensed is called the contributor's "contributor version".

  A contributor's "essential patent claims" are all patent claims
owned or controlled by the contributor, whether already acquired or
hereafter acquired, that would be infringed by some manner, permitted
by this License, of making, using, or selling its contributor version,
but do not include claims that would be infringed only as a
consequence of further modification of the contributor version.  For
purposes of this definition, "control" includes the right to grant
patent sublicenses in a manner consistent with the requirements of
this License.

  Each contributor grants you a non-exclusive, worldwide, royalty-free
patent license under the contributor's essential patent claims, to
make, use, sell, offer for sale, import and otherwise run, modify and
propagate the contents of its contributor version.

  In the following three paragraphs, a "patent license" is any express
agreement or commitment, however denominated, not to enforce a patent
(such as an express permission to practice a patent or covenant not to
sue for patent infringement).  To "grant" such a patent license to a
party means to make such an agreement or commitment not to enforce a
patent against the party.

  If you convey a covered work, knowingly relying on a patent license,
and the Corresponding Source of the work is not available for anyone
to copy, free of charge and under the terms of this License, through a
publicly available network server or other readily accessible means,
then you must either (1) cause the Corresponding Source to be so
available, or (2) arrange to deprive yourself of the benefit of the
patent license for this particular work, or (3) arrange, in a manner
consistent with the requirements of this License, to extend the patent
license to downstream recipients.  "Knowingly relying" means you have
actual knowledge that, but for the patent license, your conveying the
covered work in a country, or your recipient's use of the covered work
in a country, would infringe one or more identifiable patents in that
country that you have reason to believe are valid.

  If, pursuant to or in connection with a single transaction or
arrangement, you convey, or propagate by procuring conveyance of, a
covered work, and grant a patent license to some of the parties
receiving the covered work authorizing them to use, propagate, modify
or convey a specific copy of the covered work, then the patent license
you grant is automatically extended to all recipients of the covered
work and works based on it.

  A patent license is "discriminatory" if it does not include within
the scope of its coverage, prohibits the exercise of, or is
conditioned on the non-exercise of one or more of the rights that are
specifically granted under this License.  You may not convey a covered
work if you are a party to an arrangement with a third party that is
in the business of distributing software, under which you make payment
to the third party based on the extent of your activity of conveying
the work, and under which the third party grants, to any of the
parties who would receive the covered work from you, a discriminatory
patent license (a) in connection with copies of the covered work
conveyed by you (or copies made from those copies), or (b) primarily
for and in connection with specific products or compilations that
contain the covered work, unless you entered into that arrangement,
or that patent license was granted, prior to 28 March 2007.

  Nothing in this License shall be construed as excluding or limiting
any implied license or other defenses to infringement that may
otherwise be available to you under applicable patent law.

  12. No Surrender of Others' Freedom.

  If conditions are imposed on you (whether by court order, agreement or
otherwise) that contradict the conditions of this License, they do not
excuse you from the conditions of this License.  If you cannot convey a
covered work so as to satisfy simultaneously your obligations under this
License and any other pertinent obligations, then as a consequence you may
not convey it at all.  For example, if you agree to terms that obligate you
to collect a royalty for further conveying from those to whom you convey
the Program, the only way you could satisfy both those terms and this
License would be to refrain entirely from conveying the Program.

  13. Use with the GNU Affero General Public License.

  Notwithstanding any other provision of this License, you have
permission to link or combine any covered work with a work licensed
under version 3 of the GNU Affero General Public License into a single
combined work, and to convey the resulting work.  The terms of this
License will continue to apply to the part which is the covered work,
but the special requirements of the GNU Affero General Public License,
section 13, concerning interaction through a network will apply to the
combination as such.

  14. Revised Versions of this License.

  The Free Software Foundation may publish revised and/or new versions of
the GNU General Public License from time to time.  Such new versions will
be similar in spirit to the present version, but may differ in detail to
address new problems or concerns.

  Each version is given a distinguishing version number.  If the
Program specifies that a certain numbered version of the GNU General
Public License "or any later version" applies to it, you have the
option of following the terms and conditions either of that numbered
version or of any later version published by the Free Software
Foundation.  If the Program does not specify a version number of the
GNU General Public License, you may choose any version ever published
by the Free Software Foundation.

  If the Program specifies that a proxy can decide which future
versions of the GNU General Public License can be used, that proxy's
public statement of acceptance of a version permanently authorizes you
to choose that version for the Program.

  Later license versions may give you additional or different
permissions.  However, no additional obligations are imposed on any
author or copyright holder as a result of your choosing to follow a
later version.

  15. Disclaimer of Warranty.

  THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY
APPLICABLE LAW.  EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT
HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY
OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM
IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF
ALL NECESSARY SERVICING, REPAIR OR CORRECTION.

  16. Limitation of Liability.

  IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MODIFIES AND/OR CONVEYS
THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY
GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE
USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED TO LOSS OF
DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD
PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS),
EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGES.

  17. Interpretation of Sections 15 and 16.

  If the disclaimer of warranty and limitation of liability provided
above cannot be given local legal effect according to their terms,
reviewing courts shall apply local law that most closely approximates
an absolute waiver of all civil liability in connection with the
Program, unless a warranty or assumption of liability accompanies a
copy of the Program in return for a fee.

                     END OF TERMS AND CONDITIONS

            How to Apply These Terms to Your New Programs

  If you develop a new program, and you want it to be of the greatest
possible use to the public, the best way to achieve this is to make it
free software which everyone can redistribute and change under these terms.

  To do so, attach the following notices to the program.  It is safest
to attach them to the start of each source file to most effectively
state the exclusion of warranty; and each file should have at least
the "copyright" line and a pointer to where the full notice is found.

    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

Also add information on how to contact you by electronic and paper mail.

  If the program does terminal interaction, make it output a short
notice like this when it starts in an interactive mode:

    <program>  Copyright (C) <year>  <name of author>
    This program comes with ABSOLUTELY NO WARRANTY; for details type `show w'.
    This is free software, and you are welcome to redistribute it
    under certain conditions; type `show c' for details.

The hypothetical commands `show w' and `show c' should show the appropriate
parts of the General Public License.  Of course, your program's commands
might be different; for a GUI interface, you would use an "about box".

  You should also get your employer (if you work as a programmer) or school,
if any, to sign a "copyright disclaimer" for the program, if necessary.
For more information on this, and how to apply and follow the GNU GPL, see
<http://www.gnu.org/licenses/>.

  The GNU General Public License does not permit incorporating your program
into proprietary programs.  If your program is a subroutine library, you
may consider it more useful to permit linking proprietary applications with
the library.  If this is what you want to do, use the GNU Lesser General
Public License instead of this License.  But first, please read
<http://www.gnu.org/philosophy/why-not-lgpl.html>.
)GPL3LICENSE"));
    info.SetDescription(
        "Offline desktop application for managing MSMS membership and society records.");

    wxAboutBox(info, this);
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
