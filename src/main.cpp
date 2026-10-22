#include <wx/app.h>
#include <wx/fileconf.h>
#include <wx/filefn.h>

#include "db/DbService.h"
#include "ui/DatabaseSelectionDialog.h"
#include "ui/MainFrame.h"

namespace msms {

class App : public wxApp {
public:
    bool OnInit() override;

private:
    DbService m_dbService;
};

bool App::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    SetAppName("MSMSManager");
    SetVendorName("MSMS");

    wxFileConfig config("MSMSManager");
    wxString lastDbPath;
    config.Read("LastDatabasePath", &lastDbPath);

    bool dbLoaded = false;

    if (!lastDbPath.IsEmpty() && wxFileExists(lastDbPath)) {
        wxString error;
        if (m_dbService.Open(lastDbPath, &error) && m_dbService.CheckCompatibility(&error)) {
            dbLoaded = true;
        } else {
            m_dbService.Close();
        }
    }

    if (!dbLoaded) {
        DatabaseSelectionDialog dlg(nullptr, m_dbService);
        if (dlg.ShowModal() != wxID_OK) {
            return false;
        }
        lastDbPath = dlg.GetSelectedPath();
        config.Write("LastDatabasePath", lastDbPath);
        config.Flush();
    }

    auto* mainFrame = new MainFrame(m_dbService);
    SetTopWindow(mainFrame);
    mainFrame->Show(true);

    return true;
}

} // namespace msms

wxIMPLEMENT_APP(msms::App);
