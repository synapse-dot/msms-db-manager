#ifndef MSMS_MAIN_FRAME_H
#define MSMS_MAIN_FRAME_H

#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/treectrl.h>
#include <wx/window.h>

namespace msms {

class DbService;

class MainFrame : public wxFrame {
public:
    explicit MainFrame(DbService& dbService);

    // Call after DbService opens, creates, or switches the active database.
    void UpdateTitle();

    [[nodiscard]] wxPanel* GetWorkspacePanel() const noexcept { return m_workspacePanel; }

    // The view must be a wxWindow. It is reparented into the workspace when possible;
    // replacing a view destroys the previous workspace child.
    bool SetWorkspaceView(wxWindow* view);

private:
    void BuildMenuBar();
    void BuildNavigationTree();
    void OnNavigationSelection(wxTreeEvent& event);
    void ShowPlaceholderPage(const wxString& pageTitle);

    void OnOpenDatabase(wxCommandEvent& event);
    void OnCreateNewDatabase(wxCommandEvent& event);
    void OnSaveBackup(wxCommandEvent& event);
    void OnRestoreBackup(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    void RememberDatabasePath(const wxString& path);
    bool ReopenDatabase(const wxString& path, wxString* outError = nullptr);

    DbService& m_dbService;
    wxPanel* m_contentPanel{nullptr};
    wxPanel* m_navigationPanel{nullptr};
    wxPanel* m_workspacePanel{nullptr};
    wxTreeCtrl* m_navigationTree{nullptr};
    wxBoxSizer* m_workspaceSizer{nullptr};
    wxWindow* m_activeWorkspaceView{nullptr};
};

} // namespace msms

#endif // MSMS_MAIN_FRAME_H
