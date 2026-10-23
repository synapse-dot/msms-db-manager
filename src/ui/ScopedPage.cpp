#include "ui/ScopedPage.h"

#include <utility>

#include <wx/sizer.h>

namespace msms {

ScopedPage::ScopedPage(wxWindow* parent, ScopedPageOptions options)
    : wxPanel(parent, wxID_ANY), m_options(std::move(options)) {
    BuildControls();
    if (!m_options.parents.empty()) {
        m_parentChoice->SetSelection(0);
        LoadRows();
    }
}

void ScopedPage::BuildControls() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    // --- Navigation bar ---
    auto* navBar = new wxBoxSizer(wxHORIZONTAL);

    auto* label = new wxStaticText(this, wxID_ANY, m_options.title + ":");
    wxFont labelFont = label->GetFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(labelFont);
    navBar->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    wxArrayString parentLabels;
    for (const auto& p : m_options.parents) {
        parentLabels.Add(p.label);
    }
    m_parentChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, parentLabels);
    navBar->Add(m_parentChoice, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    m_prevButton = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x80\xb9"),
                                wxDefaultPosition, wxSize(32, -1));
    m_nextButton = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x80\xba"),
                                wxDefaultPosition, wxSize(32, -1));
    navBar->Add(m_prevButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    navBar->Add(m_nextButton, 0, wxALIGN_CENTER_VERTICAL);

    root->Add(navBar, 0, wxEXPAND | wxALL, 10);

    // --- Header showing selected parent ---
    m_headerLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
    wxFont headerFont = m_headerLabel->GetFont();
    headerFont.SetPointSize(headerFont.GetPointSize() + 2);
    headerFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_headerLabel->SetFont(headerFont);
    root->Add(m_headerLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // --- Read-only grid ---
    m_grid = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    for (std::size_t col = 0; col < m_options.columns.size(); ++col) {
        const auto& spec = m_options.columns[col];
        m_grid->InsertColumn(static_cast<long>(col), spec.title, wxLIST_FORMAT_LEFT, spec.width);
    }
    root->Add(m_grid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(root);

    // --- Bindings ---
    m_parentChoice->Bind(wxEVT_CHOICE, &ScopedPage::OnParentChanged, this);
    m_prevButton->Bind(wxEVT_BUTTON, &ScopedPage::OnPrevious, this);
    m_nextButton->Bind(wxEVT_BUTTON, &ScopedPage::OnNext, this);
}

void ScopedPage::LoadRows() {
    const int sel = m_parentChoice->GetSelection();
    const bool hasSelection =
        sel != wxNOT_FOUND && static_cast<std::size_t>(sel) < m_options.parents.size();

    // Update header
    m_headerLabel->SetLabel(hasSelection ? m_options.parents[static_cast<std::size_t>(sel)].label
                                         : wxString("(no selection)"));

    // Update stepper state
    m_prevButton->Enable(hasSelection && sel > 0);
    m_nextButton->Enable(hasSelection &&
                         static_cast<std::size_t>(sel) + 1 < m_options.parents.size());

    // Populate grid
    m_grid->Freeze();
    m_grid->DeleteAllItems();

    if (hasSelection && m_options.rowLoader) {
        const long long parentId = m_options.parents[static_cast<std::size_t>(sel)].id;
        const auto rows = m_options.rowLoader(parentId);
        for (const auto& row : rows) {
            const long itemIndex = m_grid->InsertItem(
                m_grid->GetItemCount(), row.cells.empty() ? wxString{} : row.cells[0]);
            for (std::size_t col = 1; col < m_options.columns.size(); ++col) {
                const wxString value = col < row.cells.size() ? row.cells[col] : wxString{};
                m_grid->SetItem(itemIndex, static_cast<int>(col), value);
            }
        }
    }

    m_grid->Thaw();
    GetParent()->Layout();
}

void ScopedPage::SetParents(std::vector<ScopedParent> parents) {
    m_options.parents = std::move(parents);
    m_parentChoice->Clear();
    for (const auto& p : m_options.parents) {
        m_parentChoice->Append(p.label);
    }
    if (!m_options.parents.empty()) {
        m_parentChoice->SetSelection(0);
    }
    LoadRows();
}

void ScopedPage::OnParentChanged(wxCommandEvent& /*event*/) {
    LoadRows();
}

void ScopedPage::OnPrevious(wxCommandEvent& /*event*/) {
    const int sel = m_parentChoice->GetSelection();
    if (sel > 0) {
        m_parentChoice->SetSelection(sel - 1);
        LoadRows();
    }
}

void ScopedPage::OnNext(wxCommandEvent& /*event*/) {
    const int sel = m_parentChoice->GetSelection();
    if (sel != wxNOT_FOUND && static_cast<std::size_t>(sel) + 1 < m_options.parents.size()) {
        m_parentChoice->SetSelection(sel + 1);
        LoadRows();
    }
}

} // namespace msms
