#include "MainFrame.h"

#include <functional>
#include <optional>
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

void ShowDatabaseError(wxWindow* parent, const wxString& title, const wxString& message) {
    wxMessageBox(message, title, wxOK | wxICON_ERROR, parent);
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
        "This form has unsaved changes. Save them before leaving, discard them, or cancel navigation.");
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
                "The form still reports unsaved changes after saving. Navigation was cancelled.");
            return false;
        }
        return true;
    }

    if (result == wxID_NO) {
        form->DiscardChanges();
        if (form->IsDirty()) {
            ShowDatabaseError(
                this, "Discard Error",
                "The form still reports unsaved changes after discard. Navigation was cancelled.");
            return false;
        }
        return true;
    }

    return false;
}

wxWindow* MainFrame::CreateNavigationPage(
    const wxString& pageTitle, bool formPage, const std::optional<wxString>& recordKey) {
    if (formPage) {
        // The form receives the selected row key now; feature-specific fields
        // and persistence will be implemented by the individual form views.
        return new BlankNavigationForm(m_workspacePanel, pageTitle, recordKey);
    }

    if (wxWindow* tablePage = CreateRecordTablePage(pageTitle)) {
        return tablePage;
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
    }
}

void MainFrame::OnCreateNewDatabase(wxCommandEvent& /*event*/) {
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
        const int answer = wxMessageBox(
            wxString("The file '") + path +
                "' already exists. Creating a new database will replace this file and all records "
                "it contains. This action cannot be undone.\n\nDo you want to continue?",
            "Confirm Database Replacement", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this);
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
        ShowDatabaseError(this, "Database Creation Error",
                          "Could not create a temporary database file in the destination directory.");
        return;
    }

    wxString error;
    {
        DbService candidate;
        if (!candidate.CreateNew(tempPath, &error) || !candidate.CheckCompatibility(&error)) {
            candidate.Close();
            wxRemoveFile(tempPath);
            ShowDatabaseError(this, "Database Creation Error",
                              wxString("Could not initialise a new database:\n\n") + error);
            return;
        }
    }

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
    }
}

void MainFrame::OnSaveBackup(wxCommandEvent& /*event*/) {
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

    wxFileDialog dialog(
        this, "Select Database Backup", wxEmptyString, wxEmptyString,
        "SQLite Database (*.db;*.sqlite;*.sqlite3)|*.db;*.sqlite;*.sqlite3|All Files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString backupPath = dialog.GetPath();
    const wxString activePath = m_dbService.GetDbPath();
    if (SameFilePath(backupPath, activePath)) {
        ShowDatabaseError(this, "Restore Error",
                          "The selected backup is the currently open database.");
        return;
    }

    const int answer = wxMessageBox(
        wxString("Restore '") + wxFileName(backupPath).GetFullName() + "' over '" +
            wxFileName(activePath).GetFullName() +
            "'?\n\nThis replaces the current database contents. A safety copy of the current "
            "database will be created before replacement.",
        "Confirm Database Restore", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this);
    if (answer != wxYES) {
        return;
    }

    wxString error;
    if (!m_dbService.RestoreFrom(backupPath, &error)) {
        const wxString restoreError = error;
        if (!m_dbService.IsOpen()) {
            wxString reopenError;
            ReopenDatabase(activePath, &reopenError);
        }
        ShowDatabaseError(this, "Restore Error",
                          wxString("Could not restore the database:\n\n") + restoreError);
        return;
    }

    UpdateTitle();
    RefreshActiveTablePage();
    wxMessageBox("Database restored successfully.", "Restore Complete",
                 wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnExit(wxCommandEvent& /*event*/) {
    Close(true);
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
    if (!dynamic_cast<TablePage*>(m_activeWorkspaceView) || !m_activeNavigationItem.IsOk()) {
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

void MainFrame::UpdateTitle() {
    if (m_dbService.IsOpen()) {
        const wxFileName databaseFile(m_dbService.GetDbPath());
        SetTitle(wxString::FromUTF8("MSMS Manager \xE2\x80\x94 ") + databaseFile.GetFullName());
    } else {
        SetTitle("MSMS Manager");
    }
}

} // namespace msms
