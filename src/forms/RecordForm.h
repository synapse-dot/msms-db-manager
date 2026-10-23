#ifndef MSMS_RECORD_FORM_H
#define MSMS_RECORD_FORM_H

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/datectrl.h>
#include <wx/datetime.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/timectrl.h>
#include <wx/window.h>

#include "db/DbService.h"
#include "ui/INavigationForm.h"

namespace msms {

enum class RecordFormMode {
    Create,
    Edit
};

struct RecordFormOptions {
    wxString title;
    RecordFormMode mode{RecordFormMode::Create};
    DbService* database{nullptr};

    // Used by the shared Delete error path to explain foreign-key restrictions.
    wxString deleteTable;
    std::optional<std::int64_t> recordId;
    wxString compositeKeyDescription;

    // Must navigate to the form's owning table and refresh that table's data.
    std::function<void()> onReturnToParent;
};

// Common chrome, keyboard handling, dirty tracking, and action flow for record
// editors. Concrete forms supply fields and table-specific validation/SQL.
class RecordForm : public wxPanel, public INavigationForm {
public:
    RecordForm(wxWindow* parent, RecordFormOptions options);
    ~RecordForm() override = default;

    [[nodiscard]] bool IsDirty() const override;
    // Validates and persists the record, but deliberately does not navigate.
    // This is also called by MainFrame's unsaved-changes navigation guard.
    bool Save(wxString* outError = nullptr) override;
    void DiscardChanges() override;

    [[nodiscard]] RecordFormMode Mode() const noexcept { return m_options.mode; }
    [[nodiscard]] bool IsEditMode() const noexcept {
        return m_options.mode == RecordFormMode::Edit;
    }

protected:
    wxFlexGridSizer* FieldSizer() const noexcept { return m_fieldSizer; }

    wxTextCtrl* AddTextField(const wxString& label, const wxString& initialValue = wxEmptyString,
                             long style = 0);
    wxChoice* AddChoiceField(const wxString& label, const wxArrayString& choices,
                             int initialSelection = 0);
    wxDatePickerCtrl* AddDateField(const wxString& label,
                                   const wxDateTime& initialValue = wxDefaultDateTime);
    wxTimePickerCtrl* AddTimeField(const wxString& label,
                                   const wxDateTime& initialValue = wxDefaultDateTime);

    // Track other supported controls added by a concrete form. Returns false
    // for control classes outside the documented set.
    bool TrackField(wxWindow* control);

    // Call after a concrete form has populated its initial values (especially
    // when loading a record in Edit mode) to establish the clean baseline.
    void MarkClean();

    void ReturnToParent();

    virtual bool ValidateFields(wxString* outError) const = 0;
    virtual bool SaveRecord(wxString* outError) = 0;
    virtual bool DeleteRecord(wxString* outError) = 0;

private:
    enum class TrackedKind {
        Text,
        Choice,
        Date,
        Time
    };

    struct TrackedField {
        wxWindow* control{nullptr};
        TrackedKind kind{TrackedKind::Text};
        wxString baselineValue;
        wxDateTime baselineDateTime;
    };

    void BuildControls();
    void AddFieldRow(const wxString& label, wxWindow* control);
    [[nodiscard]] static wxString ReadFieldValue(const TrackedField& field);
    void UpdateDirtyState();
    void OnSave(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void HandleDeleteFailure(const wxString& suppliedError);

    RecordFormOptions m_options;
    wxBoxSizer* m_rootSizer{nullptr};
    wxFlexGridSizer* m_fieldSizer{nullptr};
    wxButton* m_saveButton{nullptr};
    wxButton* m_deleteButton{nullptr};
    std::vector<TrackedField> m_fields;
    wxWindowID m_saveCommandId{wxID_NONE};
    wxWindowID m_cancelCommandId{wxID_NONE};
};

} // namespace msms

#endif // MSMS_RECORD_FORM_H
