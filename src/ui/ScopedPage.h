#ifndef MSMS_SCOPED_PAGE_H
#define MSMS_SCOPED_PAGE_H

#include <functional>
#include <vector>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/string.h>

namespace msms {

struct ScopedColumn {
    wxString title;
    int width{140};
};

struct ScopedRow {
    std::vector<wxString> cells;
};

struct ScopedParent {
    long long id{0}; // Internal; never displayed.
    wxString label;  // Human-readable (e.g. "AGM 2024 — 2024-03-12").
};

// Callback: given parent id, return rows for that parent.
using ScopedRowLoader = std::function<std::vector<ScopedRow>(long long parentId)>;

struct ScopedPageOptions {
    wxString title;                    // Page heading prefix (e.g. "Attendees").
    std::vector<ScopedColumn> columns; // Grid column definitions.
    std::vector<ScopedParent> parents; // All selectable parents.
    ScopedRowLoader rowLoader;         // Fetches rows for selected parent.
};

// Reusable scoped relationship page per §6.3 of design.md.
//
//  ┌────────────────────────────────────────────────────────────┐
//  │ Label: [ Parent dropdown ▾ ]                           ‹ › │
//  ├────────────────────────────────────────────────────────────┤
//  │ read-only grid                                             │
//  └────────────────────────────────────────────────────────────┘
class ScopedPage final : public wxPanel {
public:
    ScopedPage(wxWindow* parent, ScopedPageOptions options);

    // Replace parent list and reload (e.g. after db switch).
    void SetParents(std::vector<ScopedParent> parents);

private:
    void BuildControls();
    void LoadRows();
    void OnParentChanged(wxCommandEvent& event);
    void OnPrevious(wxCommandEvent& event);
    void OnNext(wxCommandEvent& event);

    ScopedPageOptions m_options;
    wxStaticText* m_headerLabel{nullptr};
    wxChoice* m_parentChoice{nullptr};
    wxButton* m_prevButton{nullptr};
    wxButton* m_nextButton{nullptr};
    wxListCtrl* m_grid{nullptr};
};

} // namespace msms

#endif // MSMS_SCOPED_PAGE_H
