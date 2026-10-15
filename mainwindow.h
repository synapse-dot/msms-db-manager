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
class MeetingsTableView;
class MeetingFormView;
class AttendeesTableView;
class AttendanceRegisterView;
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
    MeetingsTableView* agmView() const { return m_agmView; }
    MeetingsTableView* egmView() const { return m_egmView; }
    MeetingsTableView* ecmView() const { return m_ecmView; }
    AttendeesTableView* attendeesView() const { return m_attendeesView; }
    MeetingFormView* meetingFormView() const { return m_meetingFormView; }
    AttendanceRegisterView* attendanceRegisterView() const { return m_attendanceRegisterView; }

public slots:
    void showAllMembersView();
    void showActiveMembersView();
    void showNewMemberForm();
    void showEditMemberForm(int memberId);

    void showAgmView();
    void showEgmView();
    void showEcmView();
    void showAttendeesView();
    void showNewMeetingForm(const QString &defaultType = QString());
    void showEditMeetingForm(int meetingId);
    void showAttendanceRegisterForm(int meetingId = -1);

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

    void onMeetingSaved(int meetingId);
    void onMeetingDeleted(int meetingId);
    void onAttendanceChanged(int meetingId);

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
    void refreshMeetingTables();

    QTreeWidget *m_navTree = nullptr;
    QStackedWidget *m_workspace = nullptr;
    QueryEntryBar *m_queryBar = nullptr;

    // Members
    MembersTableView *m_allMembersView = nullptr;
    MembersTableView *m_activeMembersView = nullptr;
    MemberFormView *m_memberFormView = nullptr;

    // Meetings
    MeetingsTableView *m_agmView = nullptr;
    MeetingsTableView *m_egmView = nullptr;
    MeetingsTableView *m_ecmView = nullptr;
    AttendeesTableView *m_attendeesView = nullptr;
    MeetingFormView *m_meetingFormView = nullptr;
    AttendanceRegisterView *m_attendanceRegisterView = nullptr;

    QueryResultView *m_queryResultView = nullptr;
    PlaceholderView *m_placeholderView = nullptr;
    RawTableView *m_rawTableView = nullptr;

    QSqlQueryModel *m_queryModel = nullptr;

    // Nav tree items
    QTreeWidgetItem *m_allMembersItem = nullptr;
    QTreeWidgetItem *m_activeMembersItem = nullptr;
    QTreeWidgetItem *m_newMemberItem = nullptr;

    QTreeWidgetItem *m_agmItem = nullptr;
    QTreeWidgetItem *m_egmItem = nullptr;
    QTreeWidgetItem *m_ecmItem = nullptr;
    QTreeWidgetItem *m_attendeesItem = nullptr;
    QTreeWidgetItem *m_newMeetingItem = nullptr;
    QTreeWidgetItem *m_attendanceRegisterItem = nullptr;
};

} // namespace MSMS
