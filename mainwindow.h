#pragma once

#include <QMainWindow>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QSqlQueryModel;

namespace MSMS {

class AllMembersView;
class QueryResultView;
class QueryEntryBar;
class PlaceholderView;
class RawTableView;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool initializeDatabase();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNavigationItemClicked(QTreeWidgetItem *item, int column);
    void onOpenSwitchDatabase();
    void onRefreshCurrentView();
    void onVerifyIntegrity();
    void onOpenQueryConsole();
    void onExecuteQuery(const QString &sql);
    void onRawTableTriggered(const QString &tableName);
    void onFormTriggered(const QString &formName);
    void onAbout();

private:
    void setupUi();
    void setupMenuBar();
    void setupNavigationTree();
    void setupWorkspace();
    void setupShortcuts();
    void showAllMembersView();
    void showPlaceholder(const QString &title, const QString &type, const QString &description);
    void showRawTable(const QString &tableName);
    void updateWindowTitle();

    QTreeWidget *m_navTree = nullptr;
    QStackedWidget *m_workspace = nullptr;
    QueryEntryBar *m_queryBar = nullptr;

    AllMembersView *m_allMembersView = nullptr;
    QueryResultView *m_queryResultView = nullptr;
    PlaceholderView *m_placeholderView = nullptr;
    RawTableView *m_rawTableView = nullptr;

    QSqlQueryModel *m_queryModel = nullptr;

    QTreeWidgetItem *m_allMembersItem = nullptr;
};

} // namespace MSMS
