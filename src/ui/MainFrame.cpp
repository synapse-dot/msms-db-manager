#include "MainFrame.h"

#include <utility>

#include <wx/filename.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

#include "db/DbService.h"

namespace msms {
namespace {

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
// wxSizer::Detach(window) only detaches an item from that sizer, so finding the
// actual owner prevents an ancestor sizer from continuing to manage the window.
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

    m_navigationTree->Bind(wxEVT_TREE_SEL_CHANGED, &MainFrame::OnNavigationSelection, this);

    BuildNavigationTree();
    UpdateTitle();
    Centre();
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

    // Selecting a category does not change the current workspace. Its expansion
    // and collapse behaviour remains native to wxTreeCtrl.
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

    // Reparent first so a failure leaves the existing layout untouched. Once it
    // succeeds, detach the view from its former sizer so that sizer cannot keep
    // positioning it after it moves into the workspace.
    if (view && previousParent != m_workspacePanel && !view->Reparent(m_workspacePanel)) {
        return false;
    }

    // Detach before Clear(true): the incoming view may already be registered
    // with the workspace sizer, and Clear(true) would otherwise destroy it.
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

void MainFrame::UpdateTitle() {
    if (m_dbService.IsOpen()) {
        const wxFileName databaseFile(m_dbService.GetDbPath());
        // Explicit UTF-8 bytes keep the em dash independent of source/execution charset.
        SetTitle(wxString::FromUTF8("MSMS Manager \xE2\x80\x94 ") + databaseFile.GetFullName());
    } else {
        SetTitle("MSMS Manager");
    }
}

} // namespace msms
