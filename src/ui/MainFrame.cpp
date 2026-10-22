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
        // Keep the em dash as explicit UTF-8 bytes so the source execution charset
        // cannot change how wxString::FromUTF8 decodes the title prefix.
        SetTitle(wxString::FromUTF8("MSMS Manager \xE2\x80\x94 ") + fn.GetFullName());
    } else {
        SetTitle("MSMS Manager");
    }
}

} // namespace msms
