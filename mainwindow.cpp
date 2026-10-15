#include "mainwindow.h"
#include "database/DatabaseManager.h"
#include "ui/DatabaseSelectionDialog.h"
#include "ui/MembersTableView.h"
#include "ui/MemberFormView.h"
#include "ui/MeetingsTableView.h"
#include "ui/MeetingFormView.h"
#include "ui/AttendeesTableView.h"
#include "ui/AttendanceRegisterView.h"
#include "ui/QueryConsoleView.h"
#include "ui/PlaceholderView.h"
#include "ui/RawTableView.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QMessageBox>
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QFileInfo>
#include <QCloseEvent>
#include <QShortcut>
#include <QKeySequence>

namespace MSMS {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupShortcuts();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    resize(1100, 720);
    setMinimumSize(850, 500);

    QWidget *centralContainer = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(centralContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, centralContainer);
    splitter->setChildrenCollapsible(false);

    setupNavigationTree();
    splitter->addWidget(m_navTree);

    setupWorkspace();
    splitter->addWidget(m_workspace);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 860});

    containerLayout->addWidget(splitter, 1);

    m_queryBar = new QueryEntryBar(centralContainer);
    connect(m_queryBar, &QueryEntryBar::executeQueryRequested, this, &MainWindow::onExecuteQuery);
    connect(m_queryBar, &QueryEntryBar::openDialogRequested, this, &MainWindow::onOpenQueryConsole);
    containerLayout->addWidget(m_queryBar);

    setCentralWidget(centralContainer);

    setupMenuBar();
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupMenuBar()
{
    QMenuBar *mBar = menuBar();

    // 1. File
    QMenu *fileMenu = mBar->addMenu(tr("&File"));

    QAction *actExportSql = fileMenu->addAction(tr("Export Database (.sql)"));
    actExportSql->setEnabled(false);
    actExportSql->setToolTip(tr("Feature scheduled for a future milestone."));

    QAction *actBackup = fileMenu->addAction(tr("Save Database Backup..."));
    actBackup->setEnabled(false);
    actBackup->setToolTip(tr("Feature scheduled for a future milestone."));

    QAction *actRestore = fileMenu->addAction(tr("Restore Database Backup..."));
    actRestore->setEnabled(false);
    actRestore->setToolTip(tr("Feature scheduled for a future milestone."));

    fileMenu->addSeparator();

    QAction *actSwitchDb = fileMenu->addAction(tr("&Open / Switch Database..."));
    connect(actSwitchDb, &QAction::triggered, this, &MainWindow::onOpenSwitchDatabase);

    QAction *actRefresh = fileMenu->addAction(tr("&Refresh"));
    actRefresh->setShortcut(QKeySequence::Refresh);
    connect(actRefresh, &QAction::triggered, this, &MainWindow::onRefreshCurrentView);

    fileMenu->addSeparator();

    QAction *actExit = fileMenu->addAction(tr("E&xit"));
    actExit->setShortcut(QKeySequence(tr("Ctrl+Q")));
    connect(actExit, &QAction::triggered, this, &QMainWindow::close);

    // 2. Tools
    QMenu *toolsMenu = mBar->addMenu(tr("&Tools"));

    QAction *actOpenQuery = toolsMenu->addAction(tr("&Open Query Console"));
    actOpenQuery->setShortcut(QKeySequence(tr("Ctrl+`")));
    connect(actOpenQuery, &QAction::triggered, this, &MainWindow::onOpenQueryConsole);

    QAction *actVerifyIntegrity = toolsMenu->addAction(tr("&Verify Database Integrity"));
    connect(actVerifyIntegrity, &QAction::triggered, this, &MainWindow::onVerifyIntegrity);

    // 3. Tables (raw inspector)
    QMenu *tablesMenu = mBar->addMenu(tr("&Tables"));
    const QStringList rawTables = {
        "members", "meetings", "meeting_attendance", "projects", "project_members",
        "committee_appointments", "activities", "external_competitions", "competition_participants"
    };
    for (const QString &tbl : rawTables) {
        QAction *tblAct = tablesMenu->addAction(tbl);
        connect(tblAct, &QAction::triggered, this, [this, tbl]() { onRawTableTriggered(tbl); });
    }

    // 4. Forms
    QMenu *formsMenu = mBar->addMenu(tr("F&orms"));
    const QStringList directForms = {
        "New / Edit Member",
        "New / Edit Meeting",
        "Attendance Register",
        "New / Edit Project",
        "New / Edit Project Members",
        "New / Edit Appointment",
        "New / Edit Activity",
        "New / Edit Competitions",
        "New / Edit Competition Team Assignments"
    };
    for (const QString &frm : directForms) {
        QAction *frmAct = formsMenu->addAction(frm);
        connect(frmAct, &QAction::triggered, this, [this, frm]() { onFormTriggered(frm); });
    }

    // 5. Help
    QMenu *helpMenu = mBar->addMenu(tr("&Help"));
    QAction *actAbout = helpMenu->addAction(tr("&About"));
    connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::setupNavigationTree()
{
    m_navTree = new QTreeWidget(this);
    m_navTree->setHeaderHidden(true);
    m_navTree->setIndentation(16);
    m_navTree->setAnimated(true);

    auto addCategory = [this](const QString &catName) -> QTreeWidgetItem* {
        QTreeWidgetItem *cat = new QTreeWidgetItem(m_navTree);
        cat->setText(0, catName);
        cat->setFlags(Qt::ItemIsEnabled);
        QFont f = cat->font(0);
        f.setBold(true);
        cat->setFont(0, f);
        cat->setExpanded(true);
        return cat;
    };

    auto addSubItem = [](QTreeWidgetItem *parent, const QString &text, const QString &typeKey, const QString &idKey) -> QTreeWidgetItem* {
        QTreeWidgetItem *item = new QTreeWidgetItem(parent);
        item->setText(0, text);
        item->setData(0, Qt::UserRole, typeKey);
        item->setData(0, Qt::UserRole + 1, idKey);
        return item;
    };

    // 3.1 Members
    QTreeWidgetItem *catMembers = addCategory(tr("Members"));
    m_allMembersItem   = addSubItem(catMembers, tr("All Members"),       "table", "all_members");
    m_activeMembersItem= addSubItem(catMembers, tr("Active Members"),    "table", "active_members");
    m_newMemberItem    = addSubItem(catMembers, tr("New / Edit Member"), "form",  "form_member");

    // 3.2 Meetings
    QTreeWidgetItem *catMeetings = addCategory(tr("Meetings"));
    m_agmItem               = addSubItem(catMeetings, tr("Annual General Meetings"),        "table", "agm");
    m_egmItem               = addSubItem(catMeetings, tr("Extraordinary General Meetings"), "table", "egm");
    m_ecmItem               = addSubItem(catMeetings, tr("Executive Committee Meetings"),   "table", "ecm");
    m_attendeesItem         = addSubItem(catMeetings, tr("Attendees"),                      "table", "attendees");
    m_newMeetingItem        = addSubItem(catMeetings, tr("New / Edit Meeting"),             "form",  "form_meeting");
    m_attendanceRegisterItem= addSubItem(catMeetings, tr("Attendance Register"),            "form",  "form_attendance");

    // 3.3 Projects
    QTreeWidgetItem *catProjects = addCategory(tr("Projects"));
    addSubItem(catProjects, tr("All Projects"),              "table", "all_projects");
    addSubItem(catProjects, tr("Ongoing Projects"),          "table", "ongoing_projects");
    addSubItem(catProjects, tr("Completed Projects"),        "table", "completed_projects");
    addSubItem(catProjects, tr("Project Teams"),             "table", "project_teams");
    addSubItem(catProjects, tr("New / Edit Project"),        "form",  "form_project");
    addSubItem(catProjects, tr("New / Edit Project Members"),"form",  "form_project_members");

    // 3.4 Committee
    QTreeWidgetItem *catCommittee = addCategory(tr("Committee"));
    addSubItem(catCommittee, tr("All Appointments"),       "table", "all_appointments");
    addSubItem(catCommittee, tr("Current Committee"),      "table", "current_committee");
    addSubItem(catCommittee, tr("New / Edit Appointment"), "form",  "form_appointment");

    // 3.5 Activities
    QTreeWidgetItem *catActivities = addCategory(tr("Activities"));
    addSubItem(catActivities, tr("All Activities"),      "table", "all_activities");
    addSubItem(catActivities, tr("New / Edit Activity"), "form",  "form_activity");

    // 3.6 External Competition
    QTreeWidgetItem *catCompetition = addCategory(tr("External Competition"));
    addSubItem(catCompetition, tr("All Competitions"),                         "table", "all_competitions");
    addSubItem(catCompetition, tr("New / Edit Competitions"),                  "form",  "form_competitions");
    addSubItem(catCompetition, tr("Competition Teams"),                        "table", "competition_teams");
    addSubItem(catCompetition, tr("New / Edit Competition Team Assignments"),  "form",  "form_competition_assignments");

    connect(m_navTree, &QTreeWidget::itemClicked, this, &MainWindow::onNavigationItemClicked);
}

void MainWindow::setupWorkspace()
{
    m_workspace = new QStackedWidget(this);

    // Members
    m_allMembersView = new MembersTableView(MembersFilter::All, m_workspace);
    connect(m_allMembersView, &MembersTableView::newMemberRequested,  this, &MainWindow::showNewMemberForm);
    connect(m_allMembersView, &MembersTableView::editMemberRequested, this, &MainWindow::showEditMemberForm);
    m_workspace->addWidget(m_allMembersView);

    m_activeMembersView = new MembersTableView(MembersFilter::ActiveOnly, m_workspace);
    connect(m_activeMembersView, &MembersTableView::newMemberRequested,  this, &MainWindow::showNewMemberForm);
    connect(m_activeMembersView, &MembersTableView::editMemberRequested, this, &MainWindow::showEditMemberForm);
    m_workspace->addWidget(m_activeMembersView);

    m_memberFormView = new MemberFormView(m_workspace);
    connect(m_memberFormView, &MemberFormView::memberSaved,    this, &MainWindow::onMemberSaved);
    connect(m_memberFormView, &MemberFormView::memberDeleted,  this, &MainWindow::onMemberDeleted);
    connect(m_memberFormView, &MemberFormView::cancelRequested,this, &MainWindow::showAllMembersView);
    m_workspace->addWidget(m_memberFormView);

    // Meetings — AGM
    m_agmView = new MeetingsTableView("AGM", m_workspace);
    connect(m_agmView, &MeetingsTableView::newMeetingRequested,  this, &MainWindow::showNewMeetingForm);
    connect(m_agmView, &MeetingsTableView::editMeetingRequested, this, &MainWindow::showEditMeetingForm);
    m_workspace->addWidget(m_agmView);

    // Meetings — EGM
    m_egmView = new MeetingsTableView("EGM", m_workspace);
    connect(m_egmView, &MeetingsTableView::newMeetingRequested,  this, &MainWindow::showNewMeetingForm);
    connect(m_egmView, &MeetingsTableView::editMeetingRequested, this, &MainWindow::showEditMeetingForm);
    m_workspace->addWidget(m_egmView);

    // Meetings — ECM
    m_ecmView = new MeetingsTableView("ECM", m_workspace);
    connect(m_ecmView, &MeetingsTableView::newMeetingRequested,  this, &MainWindow::showNewMeetingForm);
    connect(m_ecmView, &MeetingsTableView::editMeetingRequested, this, &MainWindow::showEditMeetingForm);
    m_workspace->addWidget(m_ecmView);

    // Attendees
    m_attendeesView = new AttendeesTableView(m_workspace);
    connect(m_attendeesView, &AttendeesTableView::openRegisterRequested,
            this, &MainWindow::showAttendanceRegisterForm);
    m_workspace->addWidget(m_attendeesView);

    // Meeting form
    m_meetingFormView = new MeetingFormView(m_workspace);
    connect(m_meetingFormView, &MeetingFormView::meetingSaved,    this, &MainWindow::onMeetingSaved);
    connect(m_meetingFormView, &MeetingFormView::meetingDeleted,  this, &MainWindow::onMeetingDeleted);
    connect(m_meetingFormView, &MeetingFormView::cancelRequested, this, &MainWindow::showAgmView);
    m_workspace->addWidget(m_meetingFormView);

    // Attendance register
    m_attendanceRegisterView = new AttendanceRegisterView(m_workspace);
    connect(m_attendanceRegisterView, &AttendanceRegisterView::attendanceChanged,
            this, &MainWindow::onAttendanceChanged);
    connect(m_attendanceRegisterView, &AttendanceRegisterView::cancelRequested,
            this, &MainWindow::showAttendeesView);
    m_workspace->addWidget(m_attendanceRegisterView);

    // Query result
    m_queryResultView = new QueryResultView(m_workspace);
    m_workspace->addWidget(m_queryResultView);

    // Placeholder
    m_placeholderView = new PlaceholderView(tr("Placeholder"), tr("View"), tr("Details"), m_workspace);
    m_workspace->addWidget(m_placeholderView);

    // Raw table
    m_rawTableView = new RawTableView("members", m_workspace);
    m_workspace->addWidget(m_rawTableView);
}

void MainWindow::setupShortcuts()
{
    QShortcut *searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        QWidget *cur = m_workspace->currentWidget();
        if (cur == m_allMembersView)           m_allMembersView->focusSearch();
        else if (cur == m_activeMembersView)   m_activeMembersView->focusSearch();
        else if (cur == m_agmView)             m_agmView->focusSearch();
        else if (cur == m_egmView)             m_egmView->focusSearch();
        else if (cur == m_ecmView)             m_ecmView->focusSearch();
        else if (cur == m_attendeesView)       m_attendeesView->focusSearch();
    });
}

bool MainWindow::initializeDatabase()
{
    QSettings settings("MSMS", "MSMSManager");
    QString lastDb = settings.value("lastDatabasePath").toString();

    bool opened = false;
    if (!lastDb.isEmpty() && QFileInfo::exists(lastDb)) {
        QString err;
        if (DatabaseManager::instance().openDatabase(lastDb, &err)) {
            opened = true;
        }
    }

    if (!opened) {
        DatabaseSelectionDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            QString path = dlg.selectedPath();
            if (!path.isEmpty()) {
                settings.setValue("lastDatabasePath", path);
                opened = true;
            }
        }
    }

    if (opened) {
        updateWindowTitle();
        showAllMembersView();
        statusBar()->showMessage(
            tr("Connected to database: %1").arg(DatabaseManager::instance().databasePath()), 5000);
        return true;
    }

    return false;
}

bool MainWindow::preparePageSwitch()
{
    QWidget *cur = m_workspace->currentWidget();
    if (cur == m_memberFormView) {
        if (!m_memberFormView->maybeConfirmDiscard()) return false;
    } else if (cur == m_meetingFormView) {
        if (!m_meetingFormView->maybeConfirmDiscard()) return false;
    } else if (cur == m_attendanceRegisterView) {
        if (!m_attendanceRegisterView->maybeConfirmDiscard()) return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────
// Members navigation
// ──────────────────────────────────────────────────────────

void MainWindow::showAllMembersView()
{
    if (!preparePageSwitch()) return;
    m_workspace->setCurrentWidget(m_allMembersView);
    m_allMembersView->refresh();
    if (m_allMembersItem) m_navTree->setCurrentItem(m_allMembersItem);
}

void MainWindow::showActiveMembersView()
{
    if (!preparePageSwitch()) return;
    m_workspace->setCurrentWidget(m_activeMembersView);
    m_activeMembersView->refresh();
    if (m_activeMembersItem) m_navTree->setCurrentItem(m_activeMembersItem);
}

void MainWindow::showNewMemberForm()
{
    if (!preparePageSwitch()) return;
    m_memberFormView->setCreateMode();
    m_workspace->setCurrentWidget(m_memberFormView);
    if (m_newMemberItem) m_navTree->setCurrentItem(m_newMemberItem);
}

void MainWindow::showEditMemberForm(int memberId)
{
    if (!preparePageSwitch()) return;
    if (m_memberFormView->loadMember(memberId)) {
        m_workspace->setCurrentWidget(m_memberFormView);
        if (m_newMemberItem) m_navTree->setCurrentItem(m_newMemberItem);
    }
}

// ──────────────────────────────────────────────────────────
// Meetings navigation
// ──────────────────────────────────────────────────────────

void MainWindow::showAgmView()
{
    if (!preparePageSwitch()) return;
    m_workspace->setCurrentWidget(m_agmView);
    m_agmView->refresh();
    if (m_agmItem) m_navTree->setCurrentItem(m_agmItem);
}

void MainWindow::showEgmView()
{
    if (!preparePageSwitch()) return;
    m_workspace->setCurrentWidget(m_egmView);
    m_egmView->refresh();
    if (m_egmItem) m_navTree->setCurrentItem(m_egmItem);
}

void MainWindow::showEcmView()
{
    if (!preparePageSwitch()) return;
    m_workspace->setCurrentWidget(m_ecmView);
    m_ecmView->refresh();
    if (m_ecmItem) m_navTree->setCurrentItem(m_ecmItem);
}

void MainWindow::showAttendeesView()
{
    if (!preparePageSwitch()) return;
    m_workspace->setCurrentWidget(m_attendeesView);
    m_attendeesView->refresh();
    if (m_attendeesItem) m_navTree->setCurrentItem(m_attendeesItem);
}

void MainWindow::showNewMeetingForm(const QString &defaultType)
{
    if (!preparePageSwitch()) return;
    m_meetingFormView->setCreateMode(defaultType.isEmpty() ? "AGM" : defaultType);
    m_workspace->setCurrentWidget(m_meetingFormView);
    if (m_newMeetingItem) m_navTree->setCurrentItem(m_newMeetingItem);
}

void MainWindow::showEditMeetingForm(int meetingId)
{
    if (!preparePageSwitch()) return;
    if (m_meetingFormView->loadMeeting(meetingId)) {
        m_workspace->setCurrentWidget(m_meetingFormView);
        if (m_newMeetingItem) m_navTree->setCurrentItem(m_newMeetingItem);
    }
}

void MainWindow::showAttendanceRegisterForm(int meetingId)
{
    if (!preparePageSwitch()) return;
    // Reload meeting list
    m_attendanceRegisterView->loadMeetings();
    if (meetingId > 0) {
        m_attendanceRegisterView->selectMeeting(meetingId);
    }
    m_workspace->setCurrentWidget(m_attendanceRegisterView);
    if (m_attendanceRegisterItem) m_navTree->setCurrentItem(m_attendanceRegisterItem);
}

// ──────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────

void MainWindow::refreshMeetingTables()
{
    m_agmView->refresh();
    m_egmView->refresh();
    m_ecmView->refresh();
    m_attendeesView->refresh();
}

void MainWindow::showPlaceholder(const QString &title, const QString &type, const QString &description)
{
    if (!preparePageSwitch()) return;
    m_placeholderView->updateDetails(title, type, description);
    m_workspace->setCurrentWidget(m_placeholderView);
}

void MainWindow::showRawTable(const QString &tableName)
{
    if (!preparePageSwitch()) return;
    m_rawTableView->loadTable(tableName);
    m_workspace->setCurrentWidget(m_rawTableView);
    m_navTree->clearSelection();
}

void MainWindow::updateWindowTitle()
{
    QString path = DatabaseManager::instance().databasePath();
    if (path.isEmpty()) {
        setWindowTitle(tr("MSMS HQ — No Database Connected"));
    } else {
        QFileInfo fi(path);
        setWindowTitle(tr("MSMS HQ — %1 [%2]").arg(fi.fileName(), fi.absolutePath()));
    }
}

// ──────────────────────────────────────────────────────────
// Navigation handler
// ──────────────────────────────────────────────────────────

void MainWindow::onNavigationItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    QString typeKey = item->data(0, Qt::UserRole).toString();
    QString idKey   = item->data(0, Qt::UserRole + 1).toString();

    if (idKey.isEmpty()) {
        item->setExpanded(!item->isExpanded());
        return;
    }

    // Members
    if      (idKey == "all_members")    showAllMembersView();
    else if (idKey == "active_members") showActiveMembersView();
    else if (idKey == "form_member")    showNewMemberForm();

    // Meetings
    else if (idKey == "agm")             showAgmView();
    else if (idKey == "egm")             showEgmView();
    else if (idKey == "ecm")             showEcmView();
    else if (idKey == "attendees")       showAttendeesView();
    else if (idKey == "form_meeting")    showNewMeetingForm();
    else if (idKey == "form_attendance") showAttendanceRegisterForm();

    // Not-yet-implemented items → placeholder
    else {
        QString typeLabel = (typeKey == "form") ? tr("Form") : tr("Table View");
        QString desc = tr("The '%1' %2 will be implemented in a subsequent milestone as defined in design.md.")
                           .arg(item->text(0), typeLabel.toLower());
        showPlaceholder(item->text(0), typeLabel, desc);
    }
}

// ──────────────────────────────────────────────────────────
// Slots
// ──────────────────────────────────────────────────────────

void MainWindow::onOpenSwitchDatabase()
{
    if (!preparePageSwitch()) return;
    DatabaseSelectionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString path = dlg.selectedPath();
        if (!path.isEmpty()) {
            QSettings settings("MSMS", "MSMSManager");
            settings.setValue("lastDatabasePath", path);
            updateWindowTitle();
            showAllMembersView();
            statusBar()->showMessage(tr("Switched to database: %1").arg(path), 5000);
        }
    }
}

void MainWindow::onRefreshCurrentView()
{
    QWidget *cur = m_workspace->currentWidget();
    if (cur == m_allMembersView)         { m_allMembersView->refresh(); statusBar()->showMessage(tr("All Members view refreshed."), 3000); }
    else if (cur == m_activeMembersView) { m_activeMembersView->refresh(); statusBar()->showMessage(tr("Active Members view refreshed."), 3000); }
    else if (cur == m_rawTableView)      { m_rawTableView->refresh(); statusBar()->showMessage(tr("Raw table refreshed."), 3000); }
    else if (cur == m_agmView)           { m_agmView->refresh(); statusBar()->showMessage(tr("AGM table refreshed."), 3000); }
    else if (cur == m_egmView)           { m_egmView->refresh(); statusBar()->showMessage(tr("EGM table refreshed."), 3000); }
    else if (cur == m_ecmView)           { m_ecmView->refresh(); statusBar()->showMessage(tr("ECM table refreshed."), 3000); }
    else if (cur == m_attendeesView)     { m_attendeesView->refresh(); statusBar()->showMessage(tr("Attendees view refreshed."), 3000); }
    else if (cur == m_memberFormView) {
        if (m_memberFormView->maybeConfirmDiscard()) {
            if (m_memberFormView->currentMemberId() > 0)
                m_memberFormView->loadMember(m_memberFormView->currentMemberId());
            else
                m_memberFormView->setCreateMode();
        }
    } else if (cur == m_meetingFormView) {
        if (m_meetingFormView->maybeConfirmDiscard()) {
            if (m_meetingFormView->currentMeetingId() > 0)
                m_meetingFormView->loadMeeting(m_meetingFormView->currentMeetingId());
            else
                m_meetingFormView->setCreateMode();
        }
    }
}

void MainWindow::onVerifyIntegrity()
{
    QString result;
    bool ok = DatabaseManager::instance().verifyIntegrity(&result);
    if (ok) {
        QMessageBox::information(this, tr("Database Integrity Check"),
            tr("Database integrity check PASSED.\n\nResult:\n%1").arg(result));
    } else {
        QMessageBox::critical(this, tr("Database Integrity Check"),
            tr("Database integrity check FAILED.\n\nResult:\n%1").arg(result));
    }
}

void MainWindow::onOpenQueryConsole()
{
    m_queryBar->focusInput();
    statusBar()->showMessage(tr("Ready for SQL query entry."), 3000);
}

void MainWindow::onExecuteQuery(const QString &sql)
{
    auto safety = DatabaseManager::checkStatementSafety(sql);

    if (safety == DatabaseManager::StatementSafety::NeedsDeleteWarning) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this, tr("Confirm DELETE Operation"),
            tr("You are about to execute a DELETE statement:\n\n%1\n\nAre you sure?").arg(sql),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    } else if (safety == DatabaseManager::StatementSafety::NeedsUpdateWithoutWhereWarning) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this, tr("Warning: Unconstrained UPDATE"),
            tr("Warning: No WHERE clause:\n\n%1\n\nThis modifies EVERY row. Proceed?").arg(sql),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    } else if (safety == DatabaseManager::StatementSafety::ProhibitedSchemaModification) {
        m_queryResultView->displayError(sql, tr("Schema-modifying statements are prohibited."));
        m_workspace->setCurrentWidget(m_queryResultView);
        return;
    } else if (safety == DatabaseManager::StatementSafety::MultipleStatementsProhibited) {
        m_queryResultView->displayError(sql, tr("Only one SQL statement may be executed at a time."));
        m_workspace->setCurrentWidget(m_queryResultView);
        return;
    }

    if (!preparePageSwitch()) return;

    QString trimmed = sql.trimmed();
    bool isSelect = trimmed.startsWith("SELECT", Qt::CaseInsensitive) ||
                    trimmed.startsWith("PRAGMA", Qt::CaseInsensitive) ||
                    trimmed.startsWith("EXPLAIN", Qt::CaseInsensitive);

    if (isSelect) {
        if (!m_queryModel) m_queryModel = new QSqlQueryModel(this);
        m_queryModel->setQuery(sql, DatabaseManager::instance().database());
        if (m_queryModel->lastError().isValid())
            m_queryResultView->displayError(sql, m_queryModel->lastError().databaseText());
        else
            m_queryResultView->displaySelectResult(m_queryModel, sql, m_queryModel->rowCount());
    } else {
        QSqlQuery q(DatabaseManager::instance().database());
        QString err;
        if (DatabaseManager::instance().executeControlledQuery(sql, q, &err)) {
            int affected = q.numRowsAffected();
            m_queryResultView->displayModificationResult(sql, affected);
            m_allMembersView->refresh();
            m_activeMembersView->refresh();
            refreshMeetingTables();
        } else {
            m_queryResultView->displayError(sql, err);
        }
    }

    m_workspace->setCurrentWidget(m_queryResultView);
    m_navTree->clearSelection();
}

void MainWindow::onRawTableTriggered(const QString &tableName)
{
    showRawTable(tableName);
}

void MainWindow::onFormTriggered(const QString &formName)
{
    if (formName == "New / Edit Member")       showNewMemberForm();
    else if (formName == "New / Edit Meeting") showNewMeetingForm();
    else if (formName == "Attendance Register")showAttendanceRegisterForm();
    else {
        showPlaceholder(formName, tr("Form"),
            tr("Direct form launcher for '%1' is scheduled for a future milestone.").arg(formName));
        m_navTree->clearSelection();
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this, tr("About MSMS HQ"),
        tr("<h3>MSMS HQ</h3>"
           "<p><b>Version 1.0.0 (Milestone 3)</b></p>"
           "<p>Offline desktop application for managing MSMS membership and society records.</p>"
           "<p>Built with C++17, Qt 6 Widgets &amp; SQL, and SQLite.</p>"
           "<p>Native platform look with restrained customisation.</p>")
    );
}

// ──────────────────────────────────────────────────────────
// Signal handlers
// ──────────────────────────────────────────────────────────

void MainWindow::onMemberSaved(int memberId)
{
    m_allMembersView->refresh();
    m_activeMembersView->refresh();
    statusBar()->showMessage(tr("Member ID %1 saved successfully.").arg(memberId), 5000);
}

void MainWindow::onMemberDeleted(int memberId)
{
    m_allMembersView->refresh();
    m_activeMembersView->refresh();
    showAllMembersView();
    statusBar()->showMessage(tr("Member ID %1 deleted.").arg(memberId), 5000);
}

void MainWindow::onMeetingSaved(int meetingId)
{
    refreshMeetingTables();
    statusBar()->showMessage(tr("Meeting ID %1 saved successfully.").arg(meetingId), 5000);
}

void MainWindow::onMeetingDeleted(int meetingId)
{
    refreshMeetingTables();
    showAgmView();
    statusBar()->showMessage(tr("Meeting ID %1 deleted.").arg(meetingId), 5000);
}

void MainWindow::onAttendanceChanged(int meetingId)
{
    m_attendeesView->refresh();
    statusBar()->showMessage(tr("Attendance register for meeting ID %1 updated.").arg(meetingId), 5000);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!preparePageSwitch()) {
        event->ignore();
        return;
    }
    event->accept();
}

} // namespace MSMS
