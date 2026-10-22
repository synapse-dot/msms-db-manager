#ifndef MSMS_DATABASE_SELECTION_DIALOG_H
#define MSMS_DATABASE_SELECTION_DIALOG_H

#include <wx/dialog.h>
#include <wx/string.h>

namespace msms {

class DbService;

class DatabaseSelectionDialog : public wxDialog {
public:
    DatabaseSelectionDialog(wxWindow* parent, DbService& dbService);

    [[nodiscard]] const wxString& GetSelectedPath() const noexcept { return m_selectedPath; }

private:
    void OnOpenExisting(wxCommandEvent& event);
    void OnCreateNew(wxCommandEvent& event);

    DbService& m_dbService;
    wxString m_selectedPath;
};

} // namespace msms

#endif // MSMS_DATABASE_SELECTION_DIALOG_H
