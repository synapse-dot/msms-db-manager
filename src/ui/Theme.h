#pragma once

#include <QString>
class QApplication;

namespace MSMS {

class Theme {
public:
    static void applyNativeTheme(QApplication &app);
    // Backward compatibility alias
    static void applyCatppuccinMocha(QApplication &app) { applyNativeTheme(app); }
    static QString restrainedQss();
};

} // namespace MSMS
