#ifndef MSMS_TABLE_PAGE_H
#define MSMS_TABLE_PAGE_H

#include <functional>
#include <optional>
#include <vector>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/searchctrl.h>
#include <wx/string.h>

namespace msms {

struct TableColumn {
    wxString title;
    int width{140};
};

struct TableRow {
    long long id{0}; // Kept as metadata; never rendered as a column.
    std::vector<wxString> cells;
    wxString filterValue;
};

struct TableFilterOption {
    wxString label;
    wxString value;
};

struct TablePageOptions {
    wxString title;
    std::vector<TableColumn> columns;
    std::vector<TableRow> rows; // Rows arrive in the page's fixed specification order.
    std::vector<TableFilterOption> filterOptions; // Empty means no filter widget.
    wxString allFilterLabel{"All"};
    std::function<void()> onNew;
    std::function<void(long long)> onEdit;
};

// Reusable read-only table. Search covers all displayed cells; row ordering
// follows the fixed order supplied by the page's query/specification.
class TablePage final : public wxPanel {
public:
    TablePage(wxWindow* parent, TablePageOptions options);

    void SetRows(std::vector<TableRow> rows);
    [[nodiscard]] std::optional<long long> SelectedId() const;
    [[nodiscard]] wxListCtrl* GetListControl() const noexcept { return m_table; }

private:
    void BuildControls();
    void ApplyFilters();
    void OnSearch(wxCommandEvent& event);
    void OnFilter(wxCommandEvent& event);
    void OnSelectionChanged(wxListEvent& event);
    void OnNew(wxCommandEvent& event);
    void OnEdit(wxCommandEvent& event);

    TablePageOptions m_options;
    std::vector<TableRow> m_rows;
    wxSearchCtrl* m_search{nullptr};
    wxChoice* m_filter{nullptr};
    wxButton* m_editButton{nullptr};
    wxListCtrl* m_table{nullptr};
};

} // namespace msms

#endif // MSMS_TABLE_PAGE_H
