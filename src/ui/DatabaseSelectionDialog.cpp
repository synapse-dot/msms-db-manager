#include "DatabaseSelectionDialog.h"

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "db/DbService.h"

namespace msms {

DatabaseSelectionDialog::DatabaseSelectionDialog(wxWindow* parent, DbService& dbService)
    : wxDialog(parent, wxID_ANY, "Select Database — MSMS Manager", wxDefaultPosition,
               wxSize(480, 240), wxDEFAULT_DIALOG_STYLE),
      m_dbService(dbService) {
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* titleLabel =
        new wxStaticText(this, wxID_ANY, "Welcome to MSMS Manager", wxDefaultPosition,
                         wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
    wxFont titleFont = titleLabel->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 2);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleLabel->SetFont(titleFont);

    auto* descLabel = new wxStaticText(
        this, wxID_ANY, "Choose an existing SQLite database file or create a new one to begin.",
        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);

    rootSizer->AddSpacer(20);
    rootSizer->Add(titleLabel, 0, wxEXPAND | wxLEFT | wxRIGHT, 20);
    rootSizer->AddSpacer(8);
    rootSizer->Add(descLabel, 0, wxEXPAND | wxLEFT | wxRIGHT, 20);
    rootSizer->AddSpacer(25);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* openBtn =
        new wxButton(this, wxID_ANY, "Open Existing Database…", wxDefaultPosition, wxSize(180, 36));
    auto* createBtn =
        new wxButton(this, wxID_ANY, "Create New Database…", wxDefaultPosition, wxSize(180, 36));

    btnSizer->Add(openBtn, 1, wxRIGHT, 10);
    btnSizer->Add(createBtn, 1, wxLEFT, 10);

    rootSizer->Add(btnSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 30);
    rootSizer->AddSpacer(20);

    SetSizer(rootSizer);
    CentreOnScreen();

    openBtn->Bind(wxEVT_BUTTON, &DatabaseSelectionDialog::OnOpenExisting, this);
    createBtn->Bind(wxEVT_BUTTON, &DatabaseSelectionDialog::OnCreateNew, this);
}

void DatabaseSelectionDialog::OnOpenExisting(wxCommandEvent& /*event*/) {
    wxFileDialog openFileDialog(this, "Open SQLite Database", wxEmptyString, wxEmptyString,
                                "SQLite Database (*.db)|*.db|All Files (*.*)|*.*",
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    wxString path = openFileDialog.GetPath();
    wxString error;
    if (!m_dbService.Open(path, &error)) {
        wxMessageBox(wxString::Format("Could not open database:\n\n%s", error),
                     "Database Open Error", wxOK | wxICON_ERROR, this);
        return;
    }

    if (!m_dbService.CheckCompatibility(&error)) {
        m_dbService.Close();
        wxMessageBox(wxString::Format("Database incompatible:\n\n%s", error),
                     "Compatibility Check Failed", wxOK | wxICON_ERROR, this);
        return;
    }

    m_selectedPath = path;
    EndModal(wxID_OK);
}

void DatabaseSelectionDialog::OnCreateNew(wxCommandEvent& /*event*/) {
    wxFileDialog saveFileDialog(this, "Create New SQLite Database", wxEmptyString, "members.db",
                                "SQLite Database (*.db)|*.db|All Files (*.*)|*.*",
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    wxString path = saveFileDialog.GetPath();
    if (wxFileExists(path)) {
        int answer = wxMessageBox(
            wxString::Format("File '%s' already exists.\nOverwrite and recreate?", path),
            "Confirm Overwrite", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this);
        if (answer != wxYES) {
            return;
        }
        wxRemoveFile(path);
    }

    wxString error;
    if (!m_dbService.CreateNew(path, &error)) {
        wxMessageBox(wxString::Format("Could not create database:\n\n%s", error),
                     "Database Creation Error", wxOK | wxICON_ERROR, this);
        return;
    }

    m_selectedPath = path;
    EndModal(wxID_OK);
}

} // namespace msms
