#include "forms/RecordForm.h"

#include <utility>

#include <wx/accel.h>
#include <wx/button.h>
#include <wx/dateevt.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/sizer.h>

#include "util/ErrorReporter.h"

namespace msms {
namespace {

wxString RequiredErrorFallback() {
    return "The record could not be saved. Review the fields and try again.";
}

} // namespace

RecordForm::RecordForm(wxWindow* parent, RecordFormOptions options)
    : wxPanel(parent, wxID_ANY), m_options(std::move(options)) {
    BuildControls();
}

void RecordForm::BuildControls() {
    m_rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, m_options.title);
    wxFont titleFont = title->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleFont.SetPointSize(titleFont.GetPointSize() + 2);
    title->SetFont(titleFont);
    m_rootSizer->Add(title, 0, wxALL, 14);

    m_fieldSizer = new wxFlexGridSizer(0, 2, 8, 0);
    m_fieldSizer->AddGrowableCol(1, 1);
    m_rootSizer->Add(m_fieldSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    m_saveButton = new wxButton(this, wxID_SAVE, "Save");
    auto* cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
    m_deleteButton = new wxButton(this, wxID_DELETE, "Delete");

    m_deleteButton->Enable(IsEditMode());
    actions->AddStretchSpacer(1);
    actions->Add(m_deleteButton, 0, wxRIGHT, 8);
    actions->Add(cancelButton, 0, wxRIGHT, 8);
    actions->Add(m_saveButton, 0);
    m_rootSizer->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
    SetSizer(m_rootSizer);

    m_saveCommandId = wxWindow::NewControlId();
    m_cancelCommandId = wxWindow::NewControlId();
    wxAcceleratorEntry shortcuts[2];
    shortcuts[0].Set(wxACCEL_CTRL, 'S', m_saveCommandId);
    shortcuts[1].Set(wxACCEL_NORMAL, WXK_ESCAPE, m_cancelCommandId);
    SetAcceleratorTable(wxAcceleratorTable(2, shortcuts));

    m_saveButton->Bind(wxEVT_BUTTON, &RecordForm::OnSave, this);
    cancelButton->Bind(wxEVT_BUTTON, &RecordForm::OnCancel, this);
    m_deleteButton->Bind(wxEVT_BUTTON, &RecordForm::OnDelete, this);
    Bind(wxEVT_MENU, &RecordForm::OnSave, this, m_saveCommandId);
    Bind(wxEVT_MENU, &RecordForm::OnCancel, this, m_cancelCommandId);
}

void RecordForm::AddFieldRow(const wxString& label, wxWindow* control) {
    auto* fieldLabel = new wxStaticText(this, wxID_ANY, label);
    fieldLabel->Wrap(220);
    m_fieldSizer->Add(fieldLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    m_fieldSizer->Add(control, 1, wxEXPAND);
    Layout();
}

wxTextCtrl* RecordForm::AddTextField(const wxString& label, const wxString& initialValue,
                                    long style) {
    auto* control = new wxTextCtrl(this, wxID_ANY, initialValue, wxDefaultPosition,
                                   wxDefaultSize, style);
    AddFieldRow(label, control);
    TrackField(control);
    return control;
}

wxChoice* RecordForm::AddChoiceField(const wxString& label, const wxArrayString& choices,
                                     int initialSelection) {
    auto* control = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
    if (initialSelection >= 0 && initialSelection < choices.GetCount()) {
        control->SetSelection(initialSelection);
    } else if (choices.IsEmpty()) {
        control->SetSelection(wxNOT_FOUND);
    }
    AddFieldRow(label, control);
    TrackField(control);
    return control;
}

wxComboBox* RecordForm::AddComboField(const wxString& label, const wxArrayString& choices,
                                          const wxString& initialValue) {
    auto* control = new wxComboBox(this, wxID_ANY, initialValue, wxDefaultPosition,
                                   wxDefaultSize, choices, wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    AddFieldRow(label, control);
    TrackField(control);
    return control;
}

wxDatePickerCtrl* RecordForm::AddDateField(const wxString& label,
                                           const wxDateTime& initialValue) {
    auto* control = new wxDatePickerCtrl(this, wxID_ANY, initialValue, wxDefaultPosition,
                                         wxDefaultSize, wxDP_DEFAULT | wxDP_ALLOWNONE);
    AddFieldRow(label, control);
    TrackField(control);
    return control;
}

wxTimePickerCtrl* RecordForm::AddTimeField(const wxString& label,
                                           const wxDateTime& initialValue) {
    auto* control = new wxTimePickerCtrl(this, wxID_ANY, initialValue, wxDefaultPosition,
                                         wxDefaultSize, wxTP_DEFAULT);
    AddFieldRow(label, control);
    TrackField(control);
    return control;
}

bool RecordForm::TrackField(wxWindow* control) {
    if (!control) {
        return false;
    }

    TrackedField field;
    field.control = control;

    if (auto* text = dynamic_cast<wxTextCtrl*>(control)) {
        field.kind = TrackedKind::Text;
        text->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            UpdateDirtyState();
            event.Skip();
        });
    } else if (auto* choice = dynamic_cast<wxChoice*>(control)) {
        field.kind = TrackedKind::Choice;
        choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent& event) {
            UpdateDirtyState();
            event.Skip();
        });
    } else if (auto* combo = dynamic_cast<wxComboBox*>(control)) {
        field.kind = TrackedKind::Combo;
        combo->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
            UpdateDirtyState();
            event.Skip();
        });
        combo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& event) {
            UpdateDirtyState();
            event.Skip();
        });
    } else if (auto* date = dynamic_cast<wxDatePickerCtrl*>(control)) {
        field.kind = TrackedKind::Date;
        date->Bind(wxEVT_DATE_CHANGED, [this](wxDateEvent& event) {
            UpdateDirtyState();
            event.Skip();
        });
    } else if (auto* time = dynamic_cast<wxTimePickerCtrl*>(control)) {
        field.kind = TrackedKind::Time;
        time->Bind(wxEVT_TIME_CHANGED, [this](wxDateEvent& event) {
            UpdateDirtyState();
            event.Skip();
        });
    } else {
        return false;
    }

    field.baselineValue = ReadFieldValue(field);
    if (field.kind == TrackedKind::Date) {
        field.baselineDateTime = static_cast<wxDatePickerCtrl*>(control)->GetValue();
    } else if (field.kind == TrackedKind::Time) {
        field.baselineDateTime = static_cast<wxTimePickerCtrl*>(control)->GetValue();
    }
    m_fields.push_back(std::move(field));
    return true;
}

wxString RecordForm::ReadFieldValue(const TrackedField& field) {
    switch (field.kind) {
    case TrackedKind::Text:
        return static_cast<wxTextCtrl*>(field.control)->GetValue();
    case TrackedKind::Choice:
        return wxString::Format("%d", static_cast<wxChoice*>(field.control)->GetSelection());
    case TrackedKind::Combo:
        return static_cast<wxComboBox*>(field.control)->GetValue();
    case TrackedKind::Date: {
        const wxDateTime value = static_cast<wxDatePickerCtrl*>(field.control)->GetValue();
        return value.IsValid() ? value.FormatISODate() : wxString{};
    }
    case TrackedKind::Time: {
        const wxDateTime value = static_cast<wxTimePickerCtrl*>(field.control)->GetValue();
        return value.IsValid() ? value.Format("%H:%M") : wxString{};
    }
    }
    return {};
}

bool RecordForm::IsDirty() const {
    for (const auto& field : m_fields) {
        if (ReadFieldValue(field) != field.baselineValue) {
            return true;
        }
    }
    return false;
}

void RecordForm::UpdateDirtyState() {
    if (m_saveButton) {
        m_saveButton->Enable(true);
    }
}

void RecordForm::MarkClean() {
    for (auto& field : m_fields) {
        field.baselineValue = ReadFieldValue(field);
        if (field.kind == TrackedKind::Date) {
            field.baselineDateTime = static_cast<wxDatePickerCtrl*>(field.control)->GetValue();
        } else if (field.kind == TrackedKind::Time) {
            field.baselineDateTime = static_cast<wxTimePickerCtrl*>(field.control)->GetValue();
        }
    }
}

void RecordForm::DiscardChanges() {
    for (auto& field : m_fields) {
        switch (field.kind) {
        case TrackedKind::Text:
            static_cast<wxTextCtrl*>(field.control)->ChangeValue(field.baselineValue);
            break;
        case TrackedKind::Choice: {
            long selection = wxNOT_FOUND;
            field.baselineValue.ToLong(&selection);
            static_cast<wxChoice*>(field.control)->SetSelection(static_cast<int>(selection));
            break;
        }
        case TrackedKind::Combo:
            static_cast<wxComboBox*>(field.control)->ChangeValue(field.baselineValue);
            break;
        case TrackedKind::Date:
            static_cast<wxDatePickerCtrl*>(field.control)->SetValue(field.baselineDateTime);
            break;
        case TrackedKind::Time:
            static_cast<wxTimePickerCtrl*>(field.control)->SetValue(field.baselineDateTime);
            break;
        }
    }
    UpdateDirtyState();
    Layout();
}

bool RecordForm::Save(wxString* outError) {
    if (outError) {
        outError->Clear();
    }

    wxString error;
    if (!ValidateFields(&error)) {
        if (error.IsEmpty()) {
            error = "Please correct the highlighted fields before saving.";
        }
        if (outError) {
            *outError = error;
        }
        return false;
    }

    if (!SaveRecord(&error)) {
        if (error.IsEmpty()) {
            error = RequiredErrorFallback();
        }
        if (outError) {
            *outError = error;
        }
        return false;
    }

    MarkClean();
    return true;
}

void RecordForm::ReturnToParent() {
    if (!m_options.onReturnToParent) {
        return;
    }

    // The workspace controller may destroy this form while navigating, so post
    // the callback to its persistent parent rather than destroying this object
    // inside one of its own button handlers.
    wxWindow* const stableParent = GetParent();
    const auto callback = m_options.onReturnToParent;
    if (stableParent) {
        stableParent->CallAfter([callback]() { callback(); });
    } else {
        callback();
    }
}

void RecordForm::OnSave(wxCommandEvent& /*event*/) {
    wxString error;
    if (!Save(&error)) {
        wxMessageBox(error, "Could Not Save Record", wxOK | wxICON_ERROR, this);
        return;
    }
    ReturnToParent();
}

void RecordForm::OnCancel(wxCommandEvent& /*event*/) {
    if (!IsDirty()) {
        ReturnToParent();
        return;
    }

    const int result = wxMessageBox(
        "Save your changes before leaving? Choose No to discard the changes, or Cancel to stay on this form.",
        "Unsaved Changes", wxYES_NO | wxCANCEL | wxICON_QUESTION, this);

    if (result == wxYES) {
        wxString error;
        if (!Save(&error)) {
            wxMessageBox(error, "Could Not Save Record", wxOK | wxICON_ERROR, this);
            return;
        }
        ReturnToParent();
    } else if (result == wxNO) {
        DiscardChanges();
        ReturnToParent();
    }
}

void RecordForm::OnDelete(wxCommandEvent& /*event*/) {
    if (!IsEditMode()) {
        return;
    }

    const wxString prompt = "Delete this record? This action cannot be undone.";
    if (wxMessageBox(prompt, "Confirm Delete",
                     wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) {
        return;
    }

    wxString error;
    if (!DeleteRecord(&error)) {
        HandleDeleteFailure(error);
        return;
    }

    // A successful delete supersedes any unsaved edits in this form. Establish
    // a clean state before returning so navigation cannot offer to re-save it.
    DiscardChanges();
    ReturnToParent();
}

void RecordForm::HandleDeleteFailure(const wxString& suppliedError) {
    wxString message = suppliedError;
    DbService* const db = m_options.database;
    if (db && db->IsOpen()) {
        const ErrorInfo reported = ErrorReporter::Report(db->GetRawHandle(), "deleting record");
        const bool isForeignKeyError =
            ErrorReporter::IsForeignKeyViolation(reported.errorCode, reported.extendedErrorCode) ||
            reported.rawMessage.Contains("FOREIGN KEY");
        if (isForeignKeyError && !m_options.deleteTable.IsEmpty()) {
            ErrorInfo explained = reported;
            if (!m_options.compositeKeyDescription.IsEmpty()) {
                explained = ErrorReporter::ReportDeleteError(
                    db->GetRawHandle(), m_options.deleteTable, m_options.compositeKeyDescription);
            } else if (m_options.recordId) {
                explained = ErrorReporter::ReportDeleteError(
                    db->GetRawHandle(), m_options.deleteTable, *m_options.recordId);
            }
            message = explained.FormatForDisplay();
        }
    }

    if (message.IsEmpty()) {
        message = "The record could not be deleted. It may be referenced by other records.";
    }
    wxMessageBox(message, "Could Not Delete Record", wxOK | wxICON_ERROR, this);
}

} // namespace msms
