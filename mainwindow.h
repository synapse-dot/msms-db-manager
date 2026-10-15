#pragma once

#include <QMainWindow>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QSqlQueryModel;

namespace MSMS {

class MembersTableView;
class MemberFormView;
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

    // Accessors for verification / testing
    MembersTableView* allMembersView() const { return m_allMembersView; }
    MembersTableView* activeMembersView() const { return m_activeMembersView; }
    MemberFormView* memberFormView() const { return m_memberFormView; }

public slots:
    void showAllMembersView();
    void showActiveMembersView();
    void showNewMemberForm();
    void showEditMemberForm(int memberId);

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

    void onMemberSaved(int memberId);
    void onMemberDeleted(int memberId);

private:
    void setupUi();
    void setupMenuBar();
    void setupNavigationTree();
    void setupWorkspace();
    void setupShortcuts();

    bool preparePageSwitch();
    void showPlaceholder(const QString &title, const QString &type, const QString &description);
    void showRawTable(const QString &tableName);
    void updateWindowTitle();

    QTreeWidget *m_navTree = nullptr;
    QStackedWidget *m_workspace = nullptr;
    QueryEntryBar *m_queryBar = nullptr;

    MembersTableView *m_allMembersView = nullptr;
    MembersTableView *m_activeMembersView = nullptr;
    MemberFormView *m_memberFormView = nullptr;
    QueryResultView *m_queryResultView = nullptr;
    PlaceholderView *m_placeholderView = nullptr;
    RawTableView *m_rawTableView = nullptr;

    QSqlQueryModel *m_queryModel = nullptr;

    QTreeWidgetItem *m_allMembersItem = nullptr;
    QTreeWidgetItem *m_activeMembersItem = nullptr;
    QTreeWidgetItem *m_newMemberItem = nullptr;
};

} // namespace MSMS
