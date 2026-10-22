#include "MainFrame.h"

#include <utility>

#include <wx/accel.h>
#include <wx/aboutdlg.h>
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

namespace msms {
namespace {

constexpr const char* kApplicationVersion = "1.0.0";

enum MenuItemId {
    ID_OPEN_DATABASE = wxID_HIGHEST + 1,
    ID_CREATE_NEW_DATABASE,
    ID_SAVE_BACKUP,
    ID_RESTORE_BACKUP
};

class NavigationItemData final : public wxTreeItemData {
public:
    NavigationItemData(wxString pageTitle, bool isPage)
        : m_pageTitle(std::move(pageTitle)), m_isPage(isPage) {}

    [[nodiscard]] bool IsPage() const noexcept { return m_isPage; }
    [[nodiscard]] const wxString& PageTitle() const noexcept { return m_pageTitle; }

private:
    wxString m_pageTitle;
    bool m_isPage;
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
    m_navigationTree->Bind(wxEVT_TREE_SEL_CHANGED, &MainFrame::OnNavigationSelection, this);

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
        return m_navigationTree->AppendItem(root, title, -1, -1,
                                            new NavigationItemData(title, false));
    };
    const auto addPage = [this](const wxTreeItemId& parent, const wxString& title) {
        return m_navigationTree->AppendItem(parent, title, -1, -1,
                                            new NavigationItemData(title, true));
    };

    const wxTreeItemId members = addCategory("Members");
    const wxTreeItemId allMembers = addPage(members, "All Members");
    addPage(members, "New / Edit Member");

    const wxTreeItemId meetings = addCategory("Meetings");
    addPage(meetings, "All Meetings");
    addPage(meetings, "Attendees");
    addPage(meetings, "New / Edit Meeting");
    addPage(meetings, "Attendance Register");

    const wxTreeItemId projects = addCategory("Projects");
    addPage(projects, "All Projects");
    addPage(projects, "Project Teams");
    addPage(projects, "New / Edit Project");
    addPage(projects, "New / Edit Project Members");

    const wxTreeItemId committee = addCategory("Committee");
    addPage(committee, "All Appointments");
    addPage(committee, "Current Committee");
    addPage(committee, "New / Edit Appointment");

    const wxTreeItemId activities = addCategory("Activities");
    addPage(activities, "All Activities");
    addPage(activities, "New / Edit Activity");

    const wxTreeItemId competitions = addCategory("External Competitions");
    addPage(competitions, "All Competitions");
    addPage(competitions, "Competition Teams");
    addPage(competitions, "New / Edit Competitions");
    addPage(competitions, "New / Edit Competition Team Assignments");

    m_navigationTree->ExpandAll();
    m_navigationTree->SelectItem(allMembers);
    ShowPlaceholderPage("All Members");
}

void MainFrame::OnNavigationSelection(wxTreeEvent& event) {
    const wxTreeItemId item = event.GetItem();
    if (!item.IsOk()) {
        event.Skip();
        return;
    }

    const auto* data = dynamic_cast<const NavigationItemData*>(m_navigationTree->GetItemData(item));
    if (data && data->IsPage()) {
        ShowPlaceholderPage(data->PageTitle());
    }

    event.Skip();
}

void MainFrame::ShowPlaceholderPage(const wxString& pageTitle) {
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

    if (!SetWorkspaceView(view)) {
        view->Destroy();
    }
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
    wxFileDialog dialog(this, "Open Database", wxEmptyString, wxEmptyString,
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
        return;
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

    wxFileDialog dialog(this, "Select Database Backup", wxEmptyString, wxEmptyString,
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
    wxMessageBox("Database restored successfully.", "Restore Complete",
                 wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnExit(wxCommandEvent& /*event*/) {
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& /*event*/) {
    wxAboutDialogInfo info;
    info.SetName("MSMS Manager");
    info.SetVersion(kApplicationVersion);
    info.SetDescription(
        "An offline desktop application for managing MSMS membership and society records.");
    info.SetLicence(
        "Licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later). "
        "The full licence text is distributed with this application in the LICENSE file.");
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

void MainFrame::UpdateTitle() {
    if (m_dbService.IsOpen()) {
        const wxFileName databaseFile(m_dbService.GetDbPath());
        SetTitle(wxString::FromUTF8("MSMS Manager \xE2\x80\x94 ") + databaseFile.GetFullName());
    } else {
        SetTitle("MSMS Manager");
    }
}

} // namespace msms
