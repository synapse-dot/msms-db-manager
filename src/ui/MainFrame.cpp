#include "MainFrame.h"

#include <wx/filename.h>

#include "db/DbService.h"

namespace msms {

MainFrame::MainFrame(DbService& dbService)
    : wxFrame(nullptr, wxID_ANY, "MSMS Manager", wxDefaultPosition, wxSize(1000, 700)),
      m_dbService(dbService) {
    SetMinSize(wxSize(800, 500));
    UpdateTitle();
    Centre();
}

void MainFrame::UpdateTitle() {
    if (m_dbService.IsOpen()) {
        wxFileName fn(m_dbService.GetDbPath());
        // Avoid wxString::Format here: it crashes in the current wxWidgets build while
        // converting the format string. Concatenation is sufficient for a filename title.
        SetTitle(wxString::FromUTF8("MSMS Manager — ") + fn.GetFullName());
    } else {
        SetTitle("MSMS Manager");
    }
}

} // namespace msms
