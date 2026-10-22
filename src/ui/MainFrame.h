#ifndef MSMS_MAIN_FRAME_H
#define MSMS_MAIN_FRAME_H

#include <wx/frame.h>
#include <wx/string.h>

namespace msms {

class DbService;

class MainFrame : public wxFrame {
public:
    MainFrame(DbService& dbService);

    void UpdateTitle();

private:
    DbService& m_dbService;
};

} // namespace msms

#endif // MSMS_MAIN_FRAME_H
