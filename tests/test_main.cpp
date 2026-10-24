#include <wx/log.h>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    // wxLogError can route to a modal GUI target on Windows. Tests must always
    // use a non-interactive target so expected error-path cases cannot block CI.
    wxLog* previousTarget = wxLog::SetActiveTarget(new wxLogStderr());
    wxLog::EnableLogging(true);

    const int result = Catch::Session().run(argc, argv);

    // Restore the previous target and release the temporary stderr target.
    wxLog* testTarget = wxLog::SetActiveTarget(previousTarget);
    delete testTarget;
    return result;
}
