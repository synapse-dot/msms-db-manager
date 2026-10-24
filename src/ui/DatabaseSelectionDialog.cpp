#include "DatabaseSelectionDialog.h"

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "db/DbService.h"
#include "util/FileIdentity.h"

namespace msms {
namespace {

bool ConfirmDestinationIdentityIsStable(wxWindow* parent, const wxString& path,
                                        FileIdentity& approvedIdentity) {
    if (!approvedIdentity.valid) {
        wxMessageBox("The destination file identity could not be determined safely. "
                     "No replacement was performed.",
                     "Database Creation Error", wxOK | wxICON_ERROR, parent);
        return false;
    }

    while (true) {
        const FileIdentity currentIdentity = CaptureFileIdentity(path);
        if (!currentIdentity.valid) {
            wxMessageBox("The destination file identity can no longer be determined. "
                         "No replacement was performed.",
                         "Database Creation Error", wxOK | wxICON_ERROR, parent);
            return false;
        }
        if (currentIdentity == approvedIdentity) {
            return true;
        }

        wxMessageDialog changedDialog(
            parent,
            wxString("The destination file '") + path +
                "' changed while the new database was being created. Another process may have "
                "created, replaced, or removed it.\n\nDo you still want to replace the destination "
                "in its current state?",
            "Destination Changed", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
        if (changedDialog.ShowModal() != wxID_YES) {
            return false;
        }

        // Accept the exact state seen before this prompt, then recheck it. If it
        // changes while the prompt is open, the loop asks for consent again.
        approvedIdentity = currentIdentity;
    }
}

} // namespace

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
        new wxButton(this, wxID_ANY, "Open Existing Database", wxDefaultPosition, wxSize(180, 36));
    auto* createBtn =
        new wxButton(this, wxID_ANY, "Create New Database", wxDefaultPosition, wxSize(180, 36));

    btnSizer->Add(openBtn, 1, wxRIGHT, 10);
    btnSizer->Add(createBtn, 1, wxLEFT, 10);

    rootSizer->Add(btnSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 30);
    rootSizer->AddSpacer(20);

    SetSizerAndFit(rootSizer);
    CentreOnScreen();

    openBtn->SetDefault();
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

    const wxString path = openFileDialog.GetPath();
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
                                "SQLite Database (*.db)|*.db|All Files (*.*)|*.*", wxFD_SAVE);

    if (saveFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    const wxString path = saveFileDialog.GetPath();
    if (wxDirExists(path)) {
        wxMessageBox("The selected path is a directory. Choose a database filename instead.",
                     "Invalid Database Path", wxOK | wxICON_ERROR, this);
        return;
    }

    FileIdentity approvedIdentity = CaptureFileIdentity(path);
    if (!approvedIdentity.valid) {
        wxMessageBox("The destination file identity could not be determined safely.",
                     "Database Creation Error", wxOK | wxICON_ERROR, this);
        return;
    }

    if (approvedIdentity.exists) {
        wxMessageDialog confirmation(
            this,
            wxString("The file '") + path +
                "' already exists. Creating a new database will replace this file and all records "
                "it contains. This action cannot be undone.\n\nDo you want to continue?",
            "Confirm Database Replacement", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
        if (confirmation.ShowModal() != wxID_YES) {
            return;
        }
    }

    // Build the new database beside the destination. If initialization fails, the existing
    // database remains untouched; replace it only after the new schema has been created.
    wxFileName destination(path);
    destination.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    const wxString tempPrefix =
        destination.GetPathWithSep() + "." + destination.GetFullName() + ".tmp.";
    const wxString tempPath = wxFileName::CreateTempFileName(tempPrefix);
    if (tempPath.IsEmpty()) {
        wxMessageBox("Could not create a temporary database file in the destination directory.",
                     "Database Creation Error", wxOK | wxICON_ERROR, this);
        return;
    }

    wxString error;
    if (!m_dbService.CreateNew(tempPath, &error) || !m_dbService.CheckCompatibility(&error)) {
        m_dbService.Close();
        wxRemoveFile(DatabasePathLockFile(tempPath));
        if (wxFileExists(tempPath)) {
            wxRemoveFile(tempPath);
        }
        wxMessageBox(wxString::Format("Could not create database:\n\n%s", error),
                     "Database Creation Error", wxOK | wxICON_ERROR, this);
        return;
    }

    m_dbService.Close();
    wxRemoveFile(DatabasePathLockFile(tempPath));

    // Recheck immediately before moving the new file into place. This closes the
    // practical race window but is not an atomic compare-and-swap against arbitrary
    // external filesystem writers.
    if (!ConfirmDestinationIdentityIsStable(this, path, approvedIdentity)) {
        wxRemoveFile(DatabasePathLockFile(tempPath));
        if (wxFileExists(tempPath)) {
            wxRemoveFile(tempPath);
        }
        return;
    }

    if (!wxRenameFile(tempPath, path, true)) {
        wxRemoveFile(DatabasePathLockFile(tempPath));
        if (wxFileExists(tempPath)) {
            wxRemoveFile(tempPath);
        }
        wxMessageBox("The new database was initialised, but could not be moved to the selected "
                     "location. The existing file, if any, has not been intentionally modified.",
                     "Database Creation Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Reopen at the selected path so DbService and the main-window title retain the real path.
    if (!m_dbService.Open(path, &error) || !m_dbService.CheckCompatibility(&error)) {
        m_dbService.Close();
        wxMessageBox(wxString::Format(
                         "The database file was created, but could not be reopened and verified:"
                         "\n\n%s",
                         error),
                     "Database Creation Error", wxOK | wxICON_ERROR, this);
        return;
    }

    m_selectedPath = path;
    EndModal(wxID_OK);
}

} // namespace msms
