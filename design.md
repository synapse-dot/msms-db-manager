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

# MSMS Manager — Final Design Specification

**Document status:** Consolidated design requirements  
**Application:** MSMS Manager  
**Purpose:** Offline desktop application for managing MSMS membership and society records

## 1. Purpose and design principles

MSMS Manager is a properly installed desktop application intended to make it straightforward for current and future MSMS secretaries to manage membership and society records without requiring technical knowledge of SQL for routine tasks.

The application is designed around three ways of working with data:

1. **Tables** — inspect records in read-only views.
2. **Forms** — create and edit records through structured fields and controls.
3. **SQL queries** — access the database directly through a controlled SQL interface for users who need it.

The application must work offline and be usable on multiple operating systems. The database is an SQLite file that the secretary can choose where to store, copy, back up, and move between compatible installations. Prefer a zero-cost implementation and avoid mandatory paid licences or subscriptions.

A central online data source is desirable **if it can be provided at no cost and without compromising offline use**. The requirements settled here define a user-selected SQLite database file, but do not define a synchronisation service, conflict resolution, or multi-device merge behaviour. Therefore, cloud synchronisation is a future/optional design extension, not something to simulate by allowing several computers to write concurrently to the same SQLite file in a shared folder. The initial implementation must remain reliable offline.

Design priorities are usability for future secretaries, data integrity, preservation of historical records, clear error handling, and avoiding accidental destructive changes.

## 2. Main window and visual design

### 2.1 Overall layout

The main window contains, from top to bottom and left to right:

- A conventional title bar.
- A menu bar directly below the title bar.
- A collapsible navigation tree on the left.
- A main workspace to the right of the navigation tree.
- A compact SQL query-entry area along the bottom of the main window.

The main workspace occupies the area to the right of navigation and above the SQL query-entry area.

### 2.2 Navigation pane

- The left pane is a **collapsible tree view** organised by functional category.
- Tree items open the associated table view or form in the main workspace.
- The user may collapse and expand categories to reduce clutter.

### 2.3 Main workspace and page behaviour

- **Only one page is displayed at a time.** Selecting a different table, form, or query result replaces the current page. Do not use tabs for workspace pages.
- Tables and forms appear in the main workspace.
- Forms normally appear in the workspace rather than in separate windows or dialogs.
- Tables are read-only. The user selects a row and uses an explicit **Edit** action to open the relevant form with that record loaded.
- Query results also appear in the main workspace and replace the page currently shown.

### 2.4 Window behaviour

- The window is resizable and supports maximising and restoring.
- It opens at a sensible default size and has a minimum size that preserves the usability of the navigation pane, workspace, and query-entry area.
- The application uses the system font where practical and conventional platform-style icons where available.
- Use standard keyboard shortcuts where appropriate, including:
  - `Ctrl+S` — save the current form.
  - `Ctrl+F` — search the current table.
  - `Ctrl+Q` — quit the application.
- Shortcuts must respect the context of the current page and must not bypass save/discard protections.

### 2.5 Theme

Use **Catppuccin Mocha** as the application theme. Use system fonts and familiar platform iconography rather than requiring a custom font or an elaborate icon system.

## 3. Navigation tree

In this section, **Table** means a read-only view and **Form** means a view used to create or edit records.

### 3.1 Members

- **All Members** (Table)
  - Shows both active and former members.
- **Active Members** (Table)
  - Shows only members whose status is `Active`.
- **New / Edit Member** (Form)

### 3.2 Meetings

- **Annual General Meetings** (Table)
- **Extraordinary General Meetings** (Table)
- **Executive Committee Meetings** (Table)
- **Attendees** (Table)
  - Displays attendees for one meeting at a time.
  - Provides previous/next (`<` and `>`) controls and a meeting-ID text box at the bottom-right for navigating to a meeting.
  - Displays the columns `member_id`, `name`, `phone`, and `email`.
- **New / Edit Meeting** (Form)
- **Attendance Register** (Form)
  - The user selects a meeting and then selects members from a scrollable list.

### 3.3 Projects

- **All Projects** (Table)
- **Ongoing Projects** (Table)
- **Completed Projects** (Table)
- **Project Teams** (Table)
  - Displays members assigned to one project at a time.
  - Provides previous/next (`<` and `>`) controls and a project-ID text box for navigating to a project.
  - Displays the columns `member_id`, `name`, `phone`, `email`, and `role`.
- **New / Edit Project** (Form)
- **New / Edit Project Members** (Form)

### 3.4 Committee

- **All Appointments** (Table)
- **Current Committee** (Table)
- **New / Edit Appointment** (Form)

### 3.5 Activities

- **All Activities** (Table)
  - A drop-down control at the bottom filters activities by activity type.
- **New / Edit Activity** (Form)

### 3.6 External Competition

- **All Competitions** (Table)
- **New / Edit Competitions** (Form) — retain this exact plural label.
- **Competition Teams** (Table)
  - Displays all members participating in the selected competition. It does not represent separate named teams within a competition.
- **New / Edit Competition Team Assignments** (Form)

## 4. Menu bar

The menu bar appears in this order: **File**, **Tools**, **Tables**, **Forms**, **Help**.

### 4.1 File

- **Export Database (`.sql`)** — export the database schema and records as SQL.
- **Save Database Backup...** — manually save a database backup to a location chosen by the user.
- **Restore Database Backup...** — restore the working database from a selected backup, following the safeguards in Section 8. The restore command must be available through the application.
- **Refresh** — refresh the current view from the database.
- **Exit** — exit the application, subject to the unsaved-change and in-progress-operation safeguards.

### 4.2 Tools

- **Open Query Console** — open or focus the bottom SQL query-entry area/dialog.
- **Verify Database Integrity** — run SQLite's `PRAGMA integrity_check` and display the result.

### 4.3 Tables — raw database inspector

Each entry opens a read-only view of the named physical SQLite table:

- `members`
- `meetings`
- `meeting_attendance`
- `projects`
- `project_members`
- `committee_appointments`
- `activities`
- `external_competitions`
- `competition_participants`

These are raw database inspection views; they are distinct from the task-oriented tables in the navigation tree. Raw table views must remain read-only.

### 4.4 Forms — direct form launchers

Each item opens the form in the main workspace:

- **New / Edit Member**
- **New / Edit Meeting**
- **Attendance Register**
- **New / Edit Project**
- **New / Edit Project Members**
- **New / Edit Appointment**
- **New / Edit Activity**
- **New / Edit Competitions**
- **New / Edit Competition Team Assignments**

### 4.5 Help

- **About**

## 5. Table, search, filter, and refresh behaviour

### 5.1 Read-only tables

- Application tables and raw database inspection tables are read-only.
- Do not allow editing directly inside table cells.
- A user edits a record by selecting it and invoking **Edit**, which opens the matching form in the main workspace.
- Deletion is performed through a deletion action on the appropriate form, not by editing or clearing table cells.

### 5.2 Search and sorting

- `Ctrl+F` searches within the **currently displayed table only**.
- Table sorting is predefined. Column headers do not act as clickable sort controls.
- Provide predefined, relevant filters on appropriate table views, in addition to the specifically required activity-type filter and relationship selectors. Do not implement arbitrary user-configurable filter builders as a requirement.

### 5.3 Refresh behaviour

- After a successful database change, automatically refresh affected views so that displayed information reflects the database.
- **File → Refresh** remains available as a manual refresh action.
- Refreshing must not silently discard unsaved form edits. Apply the established unsaved-change confirmation behaviour if the refresh would replace a dirty form.

### 5.4 Relationship views

- **Attendees** is scoped to the selected meeting.
- **Project Teams** is scoped to the selected project.
- **Competition Teams** is scoped to the selected competition and lists participants and their roles; it does not create or imply named subteams.
- The meeting/project selection controls must make the currently selected record clear. Users can navigate with previous/next controls or enter the relevant record ID where specified.

## 6. Forms and record editing

### 6.1 General form behaviour

- Forms create or edit one logical record, or manage one relationship/assignment set where applicable.
- Forms appear in the main workspace.
- Forms can be launched from the navigation tree or from the **Forms** menu.
- Forms provide **Save** and **Cancel** actions only. Do not add separate Back or Close buttons.
- Saving a form saves the record and **keeps the form open**.
- Leaving the form is done through the navigation tree or menus.

### 6.2 Saving and unsaved changes

- If a form has unsaved edits, ask for confirmation before discarding them.
- This protection applies when cancelling a form, navigating to another page, switching databases, refreshing in a way that would replace the form, or exiting the application.
- If saving fails, keep the form open, preserve all entered values, and display the database error. Never silently discard the user's work.
- Do not close the form after a successful save; retain it so the user can continue working or edit again.

### 6.3 Validation

Validate input before attempting to save, and also rely on database constraints as a final integrity check. Validation should include:

- Required fields and allowed values.
- Valid dates and times in the formats specified in Section 12.
- Email-address and phone-number format checks, without needlessly imposing a single country-specific phone format.
- Relevant date consistency, including appointment, project, meeting, and activity constraints.
- Duplicate detection where appropriate.
- Clear identification of the field or relationship that needs correction.

Do not treat a person's name alone as a unique identifier. A likely duplicate, such as an existing member with the same name, should trigger a warning, not automatic rejection solely on that basis. Enforce actual schema-defined uniqueness constraints.

### 6.4 IDs

- Integer IDs are generated automatically by SQLite for records that use integer primary keys.
- IDs are not manually editable in ordinary forms.
- Hide IDs in ordinary application forms and tables unless they are needed for a particular workflow. Show them in raw database inspector views and in the specifically defined relationship views/selectors where IDs are required, including the `Attendees` and `Project Teams` views and meeting/project navigation controls.

### 6.5 Deletion

- Provide delete actions on the appropriate forms.
- Always ask for confirmation before deleting a record.
- Do not silently cascade-delete related records.
- If SQLite foreign-key restrictions prevent a deletion, preserve the data and explain clearly that the record cannot be deleted while related records refer to it.
- Relationship tables use `ON DELETE RESTRICT`; the application must not bypass this behaviour.

## 7. Member and relationship rules

### 7.1 Member status and history

Members have one of two statuses: `Active` or `Former`.

- **All Members** shows active and former members.
- **Active Members** shows only active members.
- Historical records for former members must be retained and remain visible in relevant historical views, including meeting attendance, project participation, and committee appointment history.
- Changing a member from active to former must not delete or rewrite historical relationship records.

### 7.2 Member selectors for new records

Normal member selectors used to create new meeting-attendance records, project assignments, and competition-participation records show **active members only**. This rule affects new selections, not existing records; historical records involving former members remain intact and visible.

### 7.3 Meeting attendance

- Attendance is represented by `meeting_attendance`, associating a member with a meeting.
- The **Attendance Register** form allows the user to select a meeting and select members from a scrollable list.
- Existing attendees are preserved when viewing historical meetings, including attendees whose member records are now marked Former.

### 7.4 Project membership

- Project assignments are represented by `project_members` and include the member's role (`Member` or `Lead`).
- **New / Edit Project Members** manages these assignments.
- New assignment selectors show active members only. Existing assignments for former members remain available in the historical project record.

### 7.5 Competition participation

- Competition participation is represented by `competition_participants` and includes a role.
- **New / Edit Competition Team Assignments** manages participant assignments.
- New participant selectors show active members only. Existing participation records for former members remain intact.
- **Competition Teams** means the participant list for the selected competition, not separate named teams.

### 7.6 Committee appointments

- The same member may hold multiple offices during overlapping appointment periods. The application must not reject or warn about overlapping appointment periods solely because the appointments belong to the same member.
- `Current Committee` is calculated from appointment dates, not from a separate status field. Show an appointment when its start date is on or before the current date and its end date is blank or on/after the current date.
- `All Appointments` retains the full appointment history, including ended appointments and appointments for members now marked Former.

## 8. Database selection, creation, switching, backup, and restore

### 8.1 Selecting a database

- The application works with a user-selected SQLite database file rather than requiring one fixed database path.
- The secretary can open an existing database or create a new database at a chosen location.
- On first run, when no database has been selected, show a database-selection screen offering **Open Existing Database** and **Create New Database** before entering the main interface.
- Once a database has been opened successfully, the application starts on **All Members**. It must not start on a home page or attempt to restore whichever page happened to be open previously.
- While running, the user can open or switch to another database. Confirm the switch; if the current form has unsaved changes, offer the appropriate save/discard/cancel choice first. Do not switch databases during an incomplete database operation.

### 8.2 Creating a database

- When creating a new database, initialise the complete MSMS schema, its constraints, and its indexes automatically.
- If the chosen destination already exists, do not overwrite it silently. Show a strong warning and require explicit confirmation before overwriting the file.
- Distinguish the create-new-database operation from opening an existing database; creating a new database at an existing path is a destructive action and must be clearly identified.

### 8.3 Opening and validating an existing database

- Check that an opened SQLite file is a compatible MSMS database with the expected tables and structure.
- Reject incompatible, unrelated, or invalid files with a clear explanation.
- Do not modify an incompatible file while checking it.
- Automatic migration of older schema versions is not part of this design requirement.

### 8.4 Manual backup

- Backups are **manual only**. Do not create scheduled or automatic backups.
- The secretary chooses when to create a backup and where to save it through **File → Save Database Backup...**.
- The operation must produce a usable backup of the database in its current state.

### 8.5 Restore

Provide a built-in database restore function, accessible through **File → Restore Database Backup...**.

Before replacing the working database:

1. Ask for explicit confirmation.
2. Validate the selected backup, including database integrity and MSMS database compatibility.
3. Create a separate safety copy of the current working database.
4. Replace the working database only after validation and safety-copy creation have succeeded.

If validation, safety-copy creation, or restoration fails, preserve the current working database and report the failure. Do not leave the working database partially replaced.

### 8.6 SQL export

**File → Export Database (`.sql`)** generates a full SQL dump capable of recreating the database. The export includes:

- Definitions for all nine tables and their constraints.
- The database indexes.
- All records in the database.
- SQL statements arranged so the exported file can recreate the schema and data with the required relationships intact.

This SQL dump is distinct from a database-file backup. The interactive query console's restrictions do not remove schema definitions from the export file.

## 9. SQL query interface

### 9.1 Layout and operation

- The query interface accepts **SQL**, not natural-language instructions.
- A compact, single-line query-entry area sits at the bottom of the main window.
- An icon on the right opens the query-entry dialog.
- **Tools → Open Query Console** opens or focuses the query-entry area/dialog.
- Query output appears in the main workspace above the query-entry area, replacing the currently displayed page. Query results do not appear inside the query-entry area or its dialog.
- Execute **one SQL statement at a time**. Do not accept multiple statements in a single execution request.

### 9.2 Permitted and prohibited statements

The console permits data-level SQL operations, including `SELECT`, `INSERT`, `UPDATE`, `DELETE`, and other appropriate data-level operations.

- Schema-modifying statements such as `CREATE`, `ALTER`, and `DROP` are prohibited.
- Do not let the user alter or drop the application's schema through the console.
- The console must not permit a multi-statement script to bypass the one-statement-at-a-time rule.

### 9.3 Destructive-operation warnings

Before execution:

- Warn before every `DELETE` statement.
- Warn before an `UPDATE` statement that has no `WHERE` clause.

These warnings supplement, rather than replace, normal SQL validation and transaction handling.

### 9.4 Transactions

- Automatically wrap each data-changing SQL statement in a transaction.
- Commit the transaction only when the statement succeeds.
- Roll back the transaction if execution fails.
- Execute at most one statement per request.

### 9.5 SQL errors and results

- Display the raw SQLite error message for a query execution failure. Do not replace it with an invented or embellished error description.
- Successful queries display their result in the main workspace.
- The application must not report success before execution and transaction completion have actually succeeded.

## 10. Refresh, integrity, and application-operation safeguards

### 10.1 Refreshing views

Refresh affected views automatically after successful changes and provide **File → Refresh** for manual refresh. Refreshing must not silently destroy unsaved form contents.

### 10.2 Database integrity check

**Tools → Verify Database Integrity** runs `PRAGMA integrity_check` on the currently open database and displays the result to the user.

### 10.3 Exit and database switching

- `Ctrl+Q` and **File → Exit** quit the application.
- If a form contains unsaved changes, ask whether to save, discard, or cancel the exit/navigation action as appropriate.
- Protect against exiting or switching databases while a database operation is running or incomplete.
- Do not add an unnecessary confirmation for an ordinary exit when there are no unsaved changes and no operation in progress.

## 11. Validation and constraints overview

Application validation and database constraints work together. The database remains the final authority on allowed values, relationships, uniqueness, and referential integrity.

### 11.1 Required fields and allowed values

- Required fields are enforced according to the schema in Section 12.
- Member status must be `Active` or `Former`.
- Meeting type must be `AGM`, `EGM`, or `ECM`.
- Project status must be `Planned`, `Ongoing`, `Completed`, or `Cancelled`.
- Project assignment role must be `Member` or `Lead`.
- Competition title/year pairs must be unique.
- Any uniqueness or allowed-value constraint defined in the schema must be enforced, and violations must be reported without losing entered form data.

### 11.2 Duplicate handling

- Enforce schema-defined unique constraints.
- Warn about likely duplicates where useful, such as a member with the same name as an existing member.
- Do not make member names unique and do not reject a record solely because its name matches another member's name.

### 11.3 Date and time consistency

- Validate date syntax and chronological rules before saving.
- Optional dates may remain blank where the schema permits them.
- When relevant dates are both present, enforce the ordering constraints specified for that entity.
- For meeting/activity times, enforce end time not earlier than start time when both are present.
- For committee appointments, enforce end date not earlier than start date when an end date is present.

## 12. Database schema

The database contains exactly these nine application tables. Dates are stored as ISO `YYYY-MM-DD` text and times as 24-hour `HH:MM` text. Enable `PRAGMA foreign_keys = ON` for each database connection.

### 12.1 `members`

| Column | Type | Rules |
|---|---|---|
| `member_id` | `INTEGER` | Primary key; generated automatically |
| `name` | `TEXT` | Required |
| `class` | `TEXT` | Optional |
| `phone` | `TEXT` | Optional |
| `email` | `TEXT` | Optional |
| `status` | `TEXT` | Required; default `Active`; allowed values `Active`, `Former` |
| `join_date` | `TEXT` | Required; no automatic default; supplied by the user |

Do not impose uniqueness on a member's name merely because two members may share a name.

### 12.2 `meetings`

| Column | Type | Rules |
|---|---|---|
| `meeting_id` | `INTEGER` | Primary key; generated automatically |
| `title` | `TEXT` | Required |
| `type` | `TEXT` | Required; allowed values `AGM`, `EGM`, `ECM` |
| `meeting_date` | `TEXT` | Optional |
| `start_time` | `TEXT` | Optional |
| `end_time` | `TEXT` | Optional; if both times exist, must not precede `start_time` |
| `location` | `TEXT` | Optional |

### 12.3 `meeting_attendance`

| Column | Type | Rules |
|---|---|---|
| `meeting_id` | `INTEGER` | Required; foreign key to `meetings.meeting_id` |
| `member_id` | `INTEGER` | Required; foreign key to `members.member_id` |

- Composite primary key: (`meeting_id`, `member_id`).
- Both foreign keys use `ON DELETE RESTRICT`.

### 12.4 `projects`

| Column | Type | Rules |
|---|---|---|
| `project_id` | `INTEGER` | Primary key; generated automatically |
| `title` | `TEXT` | Required |
| `status` | `TEXT` | Required; default `Planned`; allowed values `Planned`, `Ongoing`, `Completed`, `Cancelled` |
| `start_date` | `TEXT` | Optional |
| `target_end_date` | `TEXT` | Optional |
| `actual_end_date` | `TEXT` | Optional |

Date rules:

- `start_date` must not be later than `target_end_date` when both are present.
- `start_date` must not be later than `actual_end_date` when both are present.
- These dates may each be blank or entered independently, subject to those ordering rules whenever the relevant values exist.
- Do not derive or change project status from project dates. The secretary controls status manually.

### 12.5 `project_members`

| Column | Type | Rules |
|---|---|---|
| `project_id` | `INTEGER` | Required; foreign key to `projects.project_id` |
| `member_id` | `INTEGER` | Required; foreign key to `members.member_id` |
| `role` | `TEXT` | Required; default `Member`; allowed values `Member`, `Lead` |

- Composite primary key: (`project_id`, `member_id`).
- Both foreign keys use `ON DELETE RESTRICT`.

### 12.6 `committee_appointments`

| Column | Type | Rules |
|---|---|---|
| `start_date` | `TEXT` | Required |
| `office` | `TEXT` | Required |
| `member_id` | `INTEGER` | Required; foreign key to `members.member_id` |
| `end_date` | `TEXT` | Optional; if present, must not precede `start_date` |

- Composite primary key: (`start_date`, `office`).
- `member_id` uses `ON DELETE RESTRICT`.
- A member may have different offices whose appointment periods overlap.
- `Current Committee` is date-derived: `start_date` is on/before the current date and `end_date` is absent or on/after the current date.

### 12.7 `activities`

| Column | Type | Rules |
|---|---|---|
| `activity_id` | `INTEGER` | Primary key; generated automatically |
| `title` | `TEXT` | Required |
| `type` | `TEXT` | Required |
| `activity_date` | `TEXT` | Optional |
| `start_time` | `TEXT` | Optional |
| `end_time` | `TEXT` | Optional; if both times exist, must not precede `start_time` |
| `location` | `TEXT` | Optional |

`All Activities` provides a drop-down filter by `type`.

### 12.8 `external_competitions`

| Column | Type | Rules |
|---|---|---|
| `competition_id` | `INTEGER` | Primary key; generated automatically |
| `title` | `TEXT` | Required |
| `year` | `INTEGER` | Required |

- Unique constraint: (`title`, `year`).

### 12.9 `competition_participants`

| Column | Type | Rules |
|---|---|---|
| `competition_id` | `INTEGER` | Required; foreign key to `external_competitions.competition_id` |
| `member_id` | `INTEGER` | Required; foreign key to `members.member_id` |
| `role` | `TEXT` | Required |

- Composite primary key: (`competition_id`, `member_id`).
- Both foreign keys use `ON DELETE RESTRICT`.

### 12.10 Indexes

Create these indexes:

- `idx_members_status` on `members(status)`.
- `idx_attendance_member` on `meeting_attendance(member_id)`.
- `idx_project_members_member` on `project_members(member_id)`.
- `idx_committee_member` on `committee_appointments(member_id)`.
- `idx_competition_participants_member` on `competition_participants(member_id)`.

## 13. Dates, times, and display conventions

- Display and store dates in ISO `YYYY-MM-DD` format.
- Display and store times in 24-hour `HH:MM` format.
- Blank optional dates and times should be represented consistently as absent/null values in the database, rather than as invalid placeholder text.
- Meeting and activity dates may be blank to support tentative or not-yet-scheduled records.
- Project start, target end, and actual end dates may each be blank or independently supplied, subject to the date-order rules above.
- Member `join_date` and committee appointment `start_date` are required.

## 14. Startup and default landing page

- If no database is available or selected, show the database-selection screen with **Open Existing Database** and **Create New Database** options.
- After a valid database is opened, the application lands on **All Members** every time. It must not restore another page or display a home/dashboard page.
- Remember the most recently opened database path for convenience. On later launches, attempt to open it after checking that the file exists and is compatible. If it is missing or invalid, show the database-selection screen. The landing page remains **All Members** regardless of which database is opened.

## 15. Error-handling expectations

- Preserve unsaved form values on failed saves.
- Show the raw SQLite error message for SQL execution errors, as specified in Section 9.
- For form or database errors, identify the relevant field or operation where possible without fabricating or obscuring the underlying database error.
- Explain clearly when an incompatible database is rejected, when a record cannot be deleted because of existing references, or when backup/restore cannot safely complete.
- Failed backup validation or restoration must not destroy or partially replace the current database.
- Never silently overwrite files, discard unsaved changes, cascade-delete linked records, or claim that a database operation succeeded when it did not.

## 16. Implementation boundary

This document defines the agreed product behaviour, interface structure, and database schema. The precise list of supported operating-system versions and the final implementation toolkit/build configuration are deployment and implementation choices; they are not treated as settled product requirements in this specification. A zero-cost, offline-capable central synchronisation solution remains a preference for future technical evaluation, not a defined MVP feature: before implementing it, the design must specify safe synchronisation and conflict resolution rather than sharing one writable SQLite file concurrently. Whichever implementation is selected must satisfy the offline desktop, cross-platform, SQLite, UI, validation, and data-safety requirements above.
