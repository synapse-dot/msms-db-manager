#ifndef MSMS_MAIN_FRAME_H
#define MSMS_MAIN_FRAME_H

#include <wx/frame.h>
#include <wx/string.h>
#include <wx/treectrl.h>

namespace msms {

class DbService;
class wxPanel;
class wxBoxSizer;
class wxWindow;

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
    void BuildNavigationTree();
    void OnNavigationSelection(wxTreeEvent& event);
    void ShowPlaceholderPage(const wxString& pageTitle);

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
