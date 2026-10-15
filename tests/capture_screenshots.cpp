#include <QApplication>
#include <QDir>
#include <QTimer>
#include <QPixmap>

#include "mainwindow.h"
#include "database/DatabaseManager.h"
#include "ui/Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MSMS::Theme::applyNativeTheme(app);

    QDir().mkpath("screenshots");

    QString dbPath = "msms.db";
    QString err;
    if (!MSMS::DatabaseManager::instance().openDatabase(dbPath, &err)) {
        qWarning("Could not open msms.db: %s", qPrintable(err));
        return 1;
    }

    MSMS::MainWindow window;
    window.resize(1100, 720);
    window.show();

    // 1. Capture All Members
    QTimer::singleShot(500, [&]() {
        window.grab().save("screenshots/all_members.png");

        // 2. Switch to Active Members and capture
        window.activeMembersView();
        QMetaObject::invokeMethod(&window, "showActiveMembersView");
        QTimer::singleShot(500, [&]() {
            window.grab().save("screenshots/active_members.png");

            // 3. Switch to Member Form and capture
            QMetaObject::invokeMethod(&window, "showEditMemberForm", Q_ARG(int, 1));
            QTimer::singleShot(500, [&]() {
                window.grab().save("screenshots/member_form.png");
                app.quit();
            });
        });
    });

    return app.exec();
}
