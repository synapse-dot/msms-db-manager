#ifndef MSMS_MAIN_FRAME_H
#define MSMS_MAIN_FRAME_H

#include <optional>

#include <wx/event.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/treectrl.h>
#include <wx/window.h>

#include "ui/INavigationForm.h"

namespace msms {

class DbService;

class MainFrame : public wxFrame {
public:
    explicit MainFrame(DbService& dbService);

    // Call after DbService opens, creates, or switches the active database.
    void UpdateTitle();

    [[nodiscard]] wxPanel* GetWorkspacePanel() const noexcept { return m_workspacePanel; }

private:
    void BuildMenuBar();
    void BuildNavigationTree();
    void OnNavigationSelectionChanging(wxTreeEvent& event);
    [[nodiscard]] bool NavigateToNavigationItem(const wxTreeItemId& item);
    [[nodiscard]] bool ConfirmLeaveActiveForm();
    [[nodiscard]] wxWindow* CreateNavigationPage(
        const wxString& pageTitle, bool formPage,
        const std::optional<wxString>& recordKey = std::nullopt);
    [[nodiscard]] wxWindow* CreateRecordTablePage(const wxString& pageTitle);
    [[nodiscard]] wxWindow* CreateScopedTablePage(const wxString& pageTitle);
    [[nodiscard]] bool OpenFormFromTable(
        const wxString& formTitle, const std::optional<wxString>& recordKey);
    void RefreshActiveTablePage();
    void ReturnToTablePage(const wxString& pageTitle);
    // Replacing the current view destroys the previous workspace child. This is
    // private so workspace navigation must pass through the tree controller.
    bool SetWorkspaceView(wxWindow* view);

    void OnOpenDatabase(wxCommandEvent& event);
    void OnCreateNewDatabase(wxCommandEvent& event);
    void OnSaveBackup(wxCommandEvent& event);
    void OnRestoreBackup(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
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
    wxTreeItemId m_activeNavigationItem;
    bool m_pendingFormOpen{false};
    wxString m_pendingFormTitle;
    std::optional<wxString> m_pendingFormRecordKey;
};

} // namespace msms

#endif // MSMS_MAIN_FRAME_H
