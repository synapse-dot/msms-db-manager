#ifndef MSMS_RECORD_FORMS_H
#define MSMS_RECORD_FORMS_H

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <wx/arrstr.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/textctrl.h>

#include "db/DbService.h"
#include "forms/RecordForm.h"

namespace msms {

class MemberForm final : public RecordForm {
public:
    MemberForm(wxWindow* parent, DbService& db, RecordFormMode mode,
               std::optional<std::int64_t> recordId, std::function<void()> onReturnToParent);

protected:
    bool ValidateFields(wxString* outError) const override;
    bool SaveRecord(wxString* outError) override;
    bool DeleteRecord(wxString* outError) override;

private:
    bool LoadRecord(wxString* outError);

    DbService& m_db;
    std::optional<std::int64_t> m_id;
    wxTextCtrl* m_name{nullptr};
    wxTextCtrl* m_class{nullptr};
    wxTextCtrl* m_phone{nullptr};
    wxTextCtrl* m_email{nullptr};
    wxChoice* m_status{nullptr};
    wxTextCtrl* m_joinDate{nullptr};
    bool m_recordLoaded{true};
    wxString m_loadError;
};

class MeetingForm final : public RecordForm {
public:
    MeetingForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                std::optional<std::int64_t> recordId, std::function<void()> onReturnToParent);

protected:
    bool ValidateFields(wxString* outError) const override;
    bool SaveRecord(wxString* outError) override;
    bool DeleteRecord(wxString* outError) override;

private:
    bool LoadRecord(wxString* outError);

    DbService& m_db;
    std::optional<std::int64_t> m_id;
    wxTextCtrl* m_title{nullptr};
    wxChoice* m_type{nullptr};
    wxTextCtrl* m_date{nullptr};
    wxTextCtrl* m_startTime{nullptr};
    wxTextCtrl* m_endTime{nullptr};
    wxTextCtrl* m_location{nullptr};
    bool m_recordLoaded{true};
    wxString m_loadError;
};

class ProjectForm final : public RecordForm {
public:
    ProjectForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                std::optional<std::int64_t> recordId, std::function<void()> onReturnToParent);

protected:
    bool ValidateFields(wxString* outError) const override;
    bool SaveRecord(wxString* outError) override;
    bool DeleteRecord(wxString* outError) override;

private:
    bool LoadRecord(wxString* outError);

    DbService& m_db;
    std::optional<std::int64_t> m_id;
    wxTextCtrl* m_title{nullptr};
    wxChoice* m_status{nullptr};
    wxTextCtrl* m_startDate{nullptr};
    wxTextCtrl* m_targetEndDate{nullptr};
    wxTextCtrl* m_actualEndDate{nullptr};
    bool m_recordLoaded{true};
    wxString m_loadError;
};

class AppointmentForm final : public RecordForm {
public:
    AppointmentForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                    const std::optional<wxString>& recordKey,
                    std::function<void()> onReturnToParent);

protected:
    bool ValidateFields(wxString* outError) const override;
    bool SaveRecord(wxString* outError) override;
    bool DeleteRecord(wxString* outError) override;

private:
    bool LoadRecord(wxString* outError);
    bool PopulateMemberChoices(std::optional<std::int64_t> assignedMemberId,
                               wxArrayString* labels, wxString* outError);

    DbService& m_db;
    wxString m_startDateKey;
    wxString m_officeKey;
    std::optional<std::int64_t> m_assignedMemberId;
    wxString m_initialEndDate;
    wxTextCtrl* m_office{nullptr};
    wxChoice* m_member{nullptr};
    wxTextCtrl* m_startDate{nullptr};
    wxTextCtrl* m_endDate{nullptr};
    std::vector<std::int64_t> m_memberIds;
    bool m_recordLoaded{true};
    wxString m_loadError;
};

class ActivityForm final : public RecordForm {
public:
    ActivityForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                 std::optional<std::int64_t> recordId, std::function<void()> onReturnToParent);

protected:
    bool ValidateFields(wxString* outError) const override;
    bool SaveRecord(wxString* outError) override;
    bool DeleteRecord(wxString* outError) override;

private:
    bool LoadRecord(wxString* outError);

    DbService& m_db;
    std::optional<std::int64_t> m_id;
    wxTextCtrl* m_title{nullptr};
    wxComboBox* m_type{nullptr};
    wxTextCtrl* m_date{nullptr};
    wxTextCtrl* m_startTime{nullptr};
    wxTextCtrl* m_endTime{nullptr};
    wxTextCtrl* m_location{nullptr};
    bool m_recordLoaded{true};
    wxString m_loadError;
};

class CompetitionForm final : public RecordForm {
public:
    CompetitionForm(wxWindow* parent, DbService& db, RecordFormMode mode,
                    std::optional<std::int64_t> recordId, std::function<void()> onReturnToParent);

protected:
    bool ValidateFields(wxString* outError) const override;
    bool SaveRecord(wxString* outError) override;
    bool DeleteRecord(wxString* outError) override;

private:
    bool LoadRecord(wxString* outError);

    DbService& m_db;
    std::optional<std::int64_t> m_id;
    wxTextCtrl* m_title{nullptr};
    wxTextCtrl* m_year{nullptr};
    bool m_recordLoaded{true};
    wxString m_loadError;
};

} // namespace msms

#endif // MSMS_RECORD_FORMS_H
