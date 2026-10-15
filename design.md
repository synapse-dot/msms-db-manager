# MSMS Manager — Design Specification

## 1. Purpose and scope

This document records the design decisions specified for MSMS Manager. It describes the application layout, navigation tree, tables, forms, SQL query interface and menu bar. It intentionally does not decide features or behaviours that have not yet been specified.

The application is intended to be a properly installed desktop application that can operate offline. Its design focuses on three main ways of working with the data:

1. **Tables** — view data. Tables are read-only.
2. **Forms** — create and edit data.
3. **SQL queries** — enter SQL and view query results.

## 2. Main window layout

The main window has a title bar and a menu bar at the top.

The content area is arranged into three parts:

- **Left navigation pane:** a collapsible tree view.
- **Main workspace:** the area to the right of the navigation pane, where the selected table, form or query result is displayed.
- **SQL query input:** a compact, single-line query-entry area along the bottom of the window, with an icon at its right side to open the query-entry dialog.

Query results are displayed in the main workspace, to the right of the navigation pane and above the bottom query-input area. The query-entry area or dialog is used to enter SQL; query results are not displayed inside that entry area.

### 2.1 Page handling

Only one page is displayed in the main workspace at a time. Selecting another navigation item replaces the currently displayed page; pages do not open in tabs.

Forms and tables both appear in the main workspace. Forms are not opened in separate pop-up windows as their normal presentation.

### 2.2 Navigation tree behaviour

The navigation pane is a collapsible tree. The main categories contain their associated table views and forms as subitems. Selecting a subitem opens that view or form in the main workspace.

## 3. Navigation tree

The following is the specified navigation hierarchy. **Table** means read-only; **Form** means a data-entry view used to create or edit records.

### 3.1 Members

- **All Members** (Table)
- **Active Members** (Table)
- **New / Edit Member** (Form)

### 3.2 Meetings

- **Annual General Meetings** (Table)
- **Extraordinary General Meetings** (Table)
- **Executive Committee Meetings** (Table)
- **Attendees** (Table)
  - The view is associated with one meeting at a time.
  - Users can move between meetings using previous/next (`<` and `>`) controls or by entering a meeting ID in a text box at the bottom-right of the view.
  - Display attendee information in a table with the columns `member_id`, `name`, `phone` and `email`.
- **New / Edit Meeting** (Form)
- **Attendance Register** (Form)
  - The user selects a meeting and selects members from a scrollable list.

### 3.3 Projects

- **All Projects** (Table)
- **Ongoing Projects** (Table)
- **Completed Projects** (Table)
- **Project Teams** (Table)
  - The view is associated with one project at a time.
  - Users can move between projects using previous/next (`<` and `>`) controls or by entering a project ID in a text box.
  - Display project-member information in a table with the columns `member_id`, `name`, `phone`, `email` and `role`.
- **New / Edit Project** (Form)
- **New / Edit Project Members** (Form)

### 3.4 Committee

- **All Appointments** (Table)
- **Current Committee** (Table)
- **New / Edit Appointment** (Form)

### 3.5 Activities

- **All Activities** (Table)
  - A drop-down menu at the bottom of the view filters activities by type.
- **New / Edit Activity** (Form)

### 3.6 External Competition

- **All Competitions** (Table)
- **New / Edit Competitions** (Form)
- **Competition Teams** (Table)
  - This view means all members participating in a selected competition. It does not represent multiple separately named teams within a competition.
- **New / Edit Competition Team Assignments** (Form)

## 4. Table and form conventions

### 4.1 Tables

- Tables are view-only; users do not edit data directly in table cells.
- To edit a record from a table, the user selects its row and uses an explicit **Edit** button. The relevant form opens in the main workspace to edit that record.
- The navigation tree includes both general table views and views associated with a selected meeting, project or competition.

### 4.2 Forms

- Forms are used to create and edit records.
- Forms appear in the main workspace, like table pages.
- The navigation tree and the **Forms** menu provide entry points to the forms.

### 4.3 Relationship views

The **Attendees**, **Project Teams** and **Competition Teams** views display the members related to a selected meeting, project or competition, respectively.

- Meeting attendees: `member_id`, `name`, `phone`, `email`.
- Project team members: `member_id`, `name`, `phone`, `email`, `role`.
- Competition participants: all participating members associated with the selected competition.

The **Attendance Register**, **New / Edit Project Members** and **New / Edit Competition Team Assignments** forms are the corresponding data-entry views for these relationships.

## 5. SQL query interface

The query interface is for **SQL**, not plain-language queries.

- A one-line SQL input is located at the bottom of the main window.
- An icon on the right side of the input opens the query-entry dialog.
- The **Tools → Open Query Console** menu item opens or focuses the bottom query-entry dialog.
- The query-entry area or dialog is for entering SQL only.
- Query results are displayed in the main workspace, above the query bar and to the right of the navigation pane.

## 6. Top menu bar

The top menu bar contains **File**, **Tools**, **Tables**, **Forms** and **Help**, in that order.

### 6.1 File

- **Export Database (`.sql`)**
- **Save Database Backup...**
- **Refresh**
- **Exit**

### 6.2 Tools

- **Open Query Console** — opens or focuses the bottom query-entry dialog.
- **Verify Database Integrity** — runs `PRAGMA integrity_check`.

### 6.3 Tables — Raw Database Inspector

Each entry opens a raw, read-only view of the corresponding database table:

- `members`
- `meetings`
- `meeting_attendance`
- `projects`
- `project_members`
- `committee_appointments`
- `activities`
- `external_competitions`
- `competition_participants`

### 6.4 Forms — Direct Form Launchers

Each entry opens the corresponding form in the main workspace:

- **New / Edit Member** (Form)
- **New / Edit Meeting** (Form)
- **Attendance Register** (Form)
- **New / Edit Project** (Form)
- **New / Edit Project Members** (Form)
- **New / Edit Appointment** (Form)
- **New / Edit Activity** (Form)
- **New / Edit Competitions** (Form)
- **New / Edit Competition Team Assignments** (Form)

### 6.5 Help

- **About**

## 7. Database tables referenced by the interface

The navigation tree and menus refer to the following nine database tables:

1. `members`
2. `meetings`
3. `meeting_attendance`
4. `projects`
5. `project_members`
6. `committee_appointments`
7. `activities`
8. `external_competitions`
9. `competition_participants`

The **Tables** menu exposes these tables through the raw database inspector. The navigation tree exposes task-oriented table views and forms built around them.

## 8. Decisions not specified in this document

The following details have not been specified and are therefore left open rather than assumed:

- The visual theme, colours, fonts, icons and spacing.
- The exact window dimensions and minimum size.
- Table sorting, general search, filtering beyond the activity-type filter, and CSV export behaviour.
- Form validation details, save/cancel behaviour and unsaved-change handling.
- SQL permissions, allowed statement types, and query-error handling.
- Backup file format details beyond the named backup command, backup location and restore workflow.
- Keyboard shortcuts.
- Any additional menu items or pages beyond those listed above.
