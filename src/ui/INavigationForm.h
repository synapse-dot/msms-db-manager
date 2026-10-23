#ifndef MSMS_NAVIGATION_FORM_H
#define MSMS_NAVIGATION_FORM_H

#include <wx/string.h>

namespace msms {

// Workspace form contract used by MainFrame to protect unsaved changes.
class INavigationForm {
public:
    virtual ~INavigationForm() = default;
    [[nodiscard]] virtual bool IsDirty() const = 0;

    // Save validates and persists but does not navigate away. The caller owns
    // navigation so Save/Discard/Cancel remains safe during tree transitions.
    virtual bool Save(wxString* outError = nullptr) = 0;
    virtual void DiscardChanges() = 0;
};

} // namespace msms

#endif // MSMS_NAVIGATION_FORM_H
