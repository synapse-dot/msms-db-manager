#pragma once

#include <QString>
class QApplication;

namespace MSMS {

class Theme {
public:
    static void applyCatppuccinMocha(QApplication &app);
    static QString catppuccinMochaQss();
};

} // namespace MSMS
