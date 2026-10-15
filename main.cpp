#include "mainwindow.h"
#include "ui/Theme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("MSMSManager");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("MSMS");
    app.setOrganizationDomain("msms.org");

    // Apply Catppuccin Mocha theme
    MSMS::Theme::applyCatppuccinMocha(app);

    MSMS::MainWindow window;
    if (!window.initializeDatabase()) {
        // User cancelled database selection or failed to select valid DB
        return 0;
    }

    window.show();
    return app.exec();
}
