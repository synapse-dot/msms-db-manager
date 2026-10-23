#ifndef MSMS_RELATIONSHIP_FORM_H
#define MSMS_RELATIONSHIP_FORM_H

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>

#include "db/DbService.h"
#include "ui/INavigationForm.h"

namespace msms {

enum class RelationshipKind {
    MeetingAttendance,
    ProjectMembers,
    CompetitionParticipants
};

// Shared editor for the many-to-many assignment tables. The selected parent
// stays local to the form; switching it prompts before any dirty state is lost.
class RelationshipForm final : public wxPanel, public INavigationForm {
public:
    RelationshipForm(wxWindow* parent, DbService& database, RelationshipKind kind,
                     std::function<void()> onReturnToParent);

    [[nodiscard]] bool IsDirty() const override;
    bool Save(wxString* outError = nullptr) override;
    void DiscardChanges() override;

private:
    struct ParentRecord {
        std::int64_t id{0};
        wxString label;
    };

    struct MemberRow {
        std::int64_t memberId{0};
        wxString label;
        bool isFormer{false};
        bool baselineChecked{false};
        wxString baselineRole;
        wxCheckBox* checkBox{nullptr};
        wxChoice* projectRole{nullptr};
        wxTextCtrl* competitionRole{nullptr};
    };

    enum class UnsavedDecision {
        Save,
        Discard,
        Cancel
    };

    void BuildControls();
    bool LoadParents(wxString* outError);
    bool LoadParentAt(int index, wxString* outError);
    bool LoadMemberRows(std::int64_t parentId, std::vector<MemberRow>* rows,
                        wxString* outError);
    bool SaveCurrentParent(wxString* outError);
    void RebuildChecklist(std::vector<MemberRow> rows);
    void MarkClean();
    void UpdateRowEnabledState(MemberRow& row);
    void OnParentChanged(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnMemberToggled(wxCommandEvent& event);
    void OnRoleChanged(wxCommandEvent& event);
    void ReturnToParent();
    UnsavedDecision PromptUnsavedChanges();
    bool ResolveDirtyStateBeforeSwitch(int targetIndex);
    [[nodiscard]] wxString ParentNoun() const;
    [[nodiscard]] wxString ParentEmptyPlaceholder() const;
    [[nodiscard]] wxString ParentTableTitle() const;
    [[nodiscard]] bool HasRoleControls() const noexcept;
    [[nodiscard]] wxString CurrentRole(const MemberRow& row) const;

    DbService& m_database;
    RelationshipKind m_kind;
    std::function<void()> m_onReturnToParent;

    std::vector<ParentRecord> m_parents;
    std::vector<MemberRow> m_rows;
    std::optional<std::int64_t> m_currentParentId;
    int m_currentParentIndex{-1};
    bool m_loading{false};

    wxStaticText* m_parentLabel{nullptr};
    wxChoice* m_parentChoice{nullptr};
    wxScrolledWindow* m_scrolledWindow{nullptr};
    wxBoxSizer* m_scrolledSizer{nullptr};
    wxButton* m_saveButton{nullptr};
    wxButton* m_cancelButton{nullptr};
};

} // namespace msms

#endif // MSMS_RELATIONSHIP_FORM_H
