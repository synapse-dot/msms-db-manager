#include "ui/TablePage.h"

#include <algorithm>
#include <utility>

#include <wx/sizer.h>
#include <wx/stattext.h>

namespace msms {

TablePage::TablePage(wxWindow* parent, TablePageOptions options)
    : wxPanel(parent, wxID_ANY), m_options(std::move(options)), m_rows(m_options.rows) {
    BuildControls();
    ApplyFilters();
}

void TablePage::BuildControls() {
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* controls = new wxBoxSizer(wxHORIZONTAL);

    m_search = new wxSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                wxSize(260, -1), wxTE_PROCESS_ENTER);
    m_search->SetDescriptiveText("Search…");
    controls->Add(m_search, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    if (!m_options.filterOptions.empty()) {
        std::vector<wxString> labels;
        labels.reserve(m_options.filterOptions.size() + 1);
        labels.push_back(m_options.allFilterLabel);
        for (const auto& option : m_options.filterOptions) {
            labels.push_back(option.label);
        }
        m_filter = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, labels);
        m_filter->SetSelection(0);
        controls->Add(m_filter, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        m_filter->Bind(wxEVT_CHOICE, &TablePage::OnFilter, this);
    }

    auto* newButton = new wxButton(this, wxID_ADD, "New");
    m_editButton = new wxButton(this, wxID_EDIT, "Edit");
    m_editButton->Enable(false);
    controls->Add(newButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    controls->Add(m_editButton, 0, wxALIGN_CENTER_VERTICAL);

    m_table = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    for (std::size_t column = 0; column < m_options.columns.size(); ++column) {
        const auto& spec = m_options.columns[column];
        m_table->InsertColumn(static_cast<long>(column), spec.title, wxLIST_FORMAT_LEFT,
                              spec.width);
    }

    root->Add(controls, 0, wxEXPAND | wxALL, 10);
    root->Add(m_table, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(root);

    m_search->Bind(wxEVT_TEXT, &TablePage::OnSearch, this);
    m_table->Bind(wxEVT_LIST_ITEM_SELECTED, &TablePage::OnSelectionChanged, this);
    m_table->Bind(wxEVT_LIST_ITEM_DESELECTED, &TablePage::OnSelectionChanged, this);
    newButton->Bind(wxEVT_BUTTON, &TablePage::OnNew, this);
    m_editButton->Bind(wxEVT_BUTTON, &TablePage::OnEdit, this);
}

void TablePage::SetRows(std::vector<TableRow> rows) {
    m_rows = std::move(rows);
    ApplyFilters();
}

std::optional<long long> TablePage::SelectedId() const {
    const long selected = m_table->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selected == -1 || static_cast<std::size_t>(selected) >= m_visibleRows.size()) {
        return std::nullopt;
    }
    return m_rows[m_visibleRows[static_cast<std::size_t>(selected)]].id;
}

void TablePage::ApplyFilters() {
    if (!m_table) {
        return;
    }

    const wxString query = m_search ? m_search->GetValue().Lower() : wxString{};
    const int selectedFilter = m_filter ? m_filter->GetSelection() : 0;
    const bool hasFilter = m_filter && selectedFilter > 0;
    const wxString filterValue =
        hasFilter ? m_options.filterOptions[static_cast<std::size_t>(selectedFilter - 1)].value
                  : wxString{};

    m_table->Freeze();
    m_table->DeleteAllItems();
    m_visibleRows.clear();

    for (std::size_t rowIndex = 0; rowIndex < m_rows.size(); ++rowIndex) {
        const auto& row = m_rows[rowIndex];
        if (hasFilter && row.filterValue != filterValue) {
            continue;
        }

        bool matches = query.IsEmpty();
        if (!matches) {
            for (const auto& cell : row.cells) {
                if (cell.Lower().Find(query) != wxNOT_FOUND) {
                    matches = true;
                    break;
                }
            }
        }
        if (!matches) {
            continue;
        }

        const long itemIndex = m_table->InsertItem(m_table->GetItemCount(),
                                                    row.cells.empty() ? wxString{} : row.cells[0]);
        for (std::size_t column = 1; column < m_options.columns.size(); ++column) {
            const wxString value = column < row.cells.size() ? row.cells[column] : wxString{};
            m_table->SetItem(itemIndex, static_cast<int>(column), value);
        }
        m_visibleRows.push_back(rowIndex);
    }

    m_table->Thaw();
    m_editButton->Enable(SelectedId().has_value());
}

void TablePage::OnSearch(wxCommandEvent& /*event*/) {
    ApplyFilters();
}

void TablePage::OnFilter(wxCommandEvent& /*event*/) {
    ApplyFilters();
}

void TablePage::OnSelectionChanged(wxListEvent& /*event*/) {
    m_editButton->Enable(SelectedId().has_value());
}

void TablePage::OnNew(wxCommandEvent& /*event*/) {
    if (m_options.onNew) {
        m_options.onNew();
    }
}

void TablePage::OnEdit(wxCommandEvent& /*event*/) {
    const auto id = SelectedId();
    if (id && m_options.onEdit) {
        m_options.onEdit(*id);
    }
}

} // namespace msms
