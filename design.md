# MSMS Manager — Final Design Specification

**Status:** Consolidated design, superseding all prior drafts  
**Application:** MSMS Manager  
**Purpose:** Offline desktop application for managing MSMS membership and society records  
**Database:** SQLite file, user-selected, per `schema.sql`

---

## 1. Purpose and principles

MSMS Manager is a desktop application for current and future MSMS secretaries. It must let a non-technical user view, create and edit society records without SQL knowledge, and it must not lose or corrupt data.

Design principles, in priority order:

1. **Data safety.** No silent overwrites, no cascading deletes, no discarded edits.
2. **Predictability.** The same actions behave the same way everywhere.
3. **Simplicity.** One obvious way to reach each page and each form. Nothing the secretary does not need.
4. **Buildability.** A small, consistent set of components that can be built quickly.

The application works offline. The database is a single SQLite file the secretary chooses, copies, backs up and moves. There is no cloud sync, no network dependency, no paid licences.

---

## 2. Scope

### 2.1 In scope

- Read-only table views for all nine tables, task-oriented.
- Forms for creating and editing records.
- Three scoped relationship pages (attendance, project team, competition participants).
- Three relationship forms for managing those relationships.
- Database selection, creation, switching.
- Manual backup and restore.
- Clear validation and error reporting.

### 2.2 Explicitly out of scope

These are omitted to keep the build small and the interface simple. They may be added later if the need arises, but the design does not assume them.

- SQL console or any query interface.
- Raw database inspector.
- `.sql` export.
- Tools, Tables and Forms menus.
- Manual refresh action.
- Column-click sorting, arbitrary filter builders, CSV export.
- Custom themes, icons or styling beyond the platform default.
- Automatic or scheduled backups.
- Any synchronisation or multi-device behaviour.

---

## 3. Main window

```
┌──────────────────────────────────────────────────────────────┐
│ MSMS Manager — members.db                                    │
├──────────────────────────────────────────────────────────────┤
│ File   Help                                                  │
├────────────────┬─────────────────────────────────────────────┤
│ ▾ Members      │                                             │
│    All Members │                                             │
│    New/Edit    │                                             │
│ ▾ Meetings     │          Main workspace                     │
│    All Meetings│          (one page at a time)                │
│    Attendees   │                                             │
│    ...         │                                             │
└────────────────┴─────────────────────────────────────────────┘
```

- Title bar shows **`MSMS Manager — <database filename>`**, always. No other place for that information is required.
- Menu bar directly below the title bar.
- Collapsible navigation tree on the left.
- Main workspace fills the rest.
- No bottom bar, no status bar, no tabs.
- Resizable; opens at roughly 1000×700; minimum roughly 800×500.
- Native Qt styling, system font, conventional widgets. No custom QSS beyond what is strictly needed.

---

## 4. Navigation model

The whole app follows one rule:

> **The tree is the only way to change pages. Buttons inside a page only open forms. Every form returns to its parent table on Save, Cancel or Delete.**

Consequences:

- There are no Back buttons, breadcrumbs, tabs or history.
- A form launched from the tree and a form launched by an **Edit** button are the same form, differing only in whether it is populated.
- A scoped relationship page (Attendees, Project Teams, Competition Teams) is reached from the tree, exactly like every other page.

---

## 5. Navigation tree

```
Members
  All Members                         (table)
  New / Edit Member                   (form)

Meetings
  All Meetings                        (table)
  Attendees                           (scoped table)
  New / Edit Meeting                  (form)
  Attendance Register                 (relationship form)

Projects
  All Projects                        (table)
  Project Teams                       (scoped table)
  New / Edit Project                  (form)
  New / Edit Project Members          (relationship form)

Committee
  All Appointments                    (table)
  Current Committee                   (table)
  New / Edit Appointment              (form)

Activities
  All Activities                      (table)
  New / Edit Activity                 (form)

External Competitions
  All Competitions                    (table)
  Competition Teams                   (scoped table)
  New / Edit Competitions             (form)
  New / Edit Competition Team Assignments (relationship form)
```

Notes:

- **Active Members is not a page.** It is a status filter on All Members.
- **AGM / EGM / ECM are not separate pages.** Type filter on All Meetings.
- **Ongoing / Completed are not separate pages.** Status filter on All Projects.
- Form labels match the original specification exactly, including the plural **New / Edit Competitions**.
- Clicking a form item in the tree opens a **blank** form (create mode).
- The **Edit** button on a table opens the same form **populated** with the selected row.

---

## 6. Table pages

### 6.1 Common layout

```
┌────────────────────────────────────────────────────────────┐
│ [Search…]      [Filter ▾]                    [New] [Edit]  │
├────────────────────────────────────────────────────────────┤
│ read-only grid                                             │
└────────────────────────────────────────────────────────────┘
```

- **Search** filters visible rows across all columns.
- **Filter** appears only on pages that need one; otherwise the control is absent, not disabled.
- **New** opens the corresponding form blank.
- **Edit** is disabled until a row is selected.
- The grid is read-only. No cell editing.
- Sorting is fixed per page and not user-changeable. Column headers are not clickable.
- IDs are not displayed in these task tables.

### 6.2 Record pages

| Page | Columns | Filter |
|---|---|---|
| All Members | Name, Class, Phone, Email, Status, Join Date | Status: All / Active / Former |
| All Meetings | Title, Type, Date, Start, End, Location | Type: All / AGM / EGM / ECM |
| All Projects | Title, Status, Start, Target End, Actual End | Status: All / Planned / Ongoing / Completed / Cancelled |
| All Appointments | Office, Member, Start Date, End Date | — |
| Current Committee | Office, Member, Start Date, End Date | — |
| All Activities | Title, Type, Date, Start, End, Location | Type: All / (distinct types in data) |
| All Competitions | Title, Year | — |

Default sort orders:

- Members, Projects, Competitions: by title/name ascending.
- Meetings, Activities: by date descending, blanks last.
- Appointments: by start date descending.
- Current Committee: by office ascending.

Current Committee is computed, not stored: `start_date <= today AND (end_date IS NULL OR end_date >= today)`.

### 6.3 Scoped relationship pages

Attendees, Project Teams and Competition Teams share one layout:

```
┌────────────────────────────────────────────────────────────┐
│ Meeting: [ AGM 2024 — 2024-03-12 ▾ ]                  ‹ ›  │
├────────────────────────────────────────────────────────────┤
│ grid                                                       │
└────────────────────────────────────────────────────────────┘
```

- **One dropdown** listing parents by human-readable label (title + date, title + year, etc.). The user never sees or enters an ID.
- **`‹` / `›`** step to the previous / next parent.
- The selected parent's label is the header so the user always knows what they're viewing.
- The grid is read-only. Editing the relationship is done on the corresponding form.

| Page | Parent dropdown shows | Columns |
|---|---|---|
| Attendees | Meeting title + date | member_id, name, phone, email |
| Project Teams | Project title | member_id, name, phone, email, role |
| Competition Teams | Competition title + year | member_id, name, role |

`member_id` is shown on these pages because the specification calls for it, and because it is genuinely useful when cross-referencing.

---

## 7. Forms

### 7.1 Common behaviour

- Forms appear in the workspace, replacing the current page.
- **Save** validates; on success writes and closes the form, returning to the parent table, refreshed. On failure the form stays open with all values intact and the error shown.
- **Cancel** returns to the parent table. If the form is dirty, prompt first.
- **Delete** (record forms only) confirms, deletes and returns to the parent table. Foreign-key `RESTRICT` failures are shown verbatim plus a one-line plain-language explanation.
- Required fields are marked.
- IDs are hidden and never editable.
- No "Save and keep open", no "Create another". Save → table → click **New** if another record is needed.
- `Ctrl+S` saves; `Esc` cancels.
- Leaving a dirty form via the tree, via File → Open Database, or by quitting prompts Save / Discard / Cancel.

### 7.2 Parent table for each form

| Form | Returns to |
|---|---|
| New / Edit Member | All Members |
| New / Edit Meeting | All Meetings |
| Attendance Register | All Meetings |
| New / Edit Project | All Projects |
| New / Edit Project Members | All Projects |
| New / Edit Appointment | All Appointments |
| New / Edit Activity | All Activities |
| New / Edit Competitions | All Competitions |
| New / Edit Competition Team Assignments | All Competitions |

### 7.3 Record forms

| Form | Fields | Validation |
|---|---|---|
| **Member** | Name\*, Class, Phone, Email, Status (Active / Former, default Active), Join Date\* | Join date required. Duplicate name → non-blocking warning. |
| **Meeting** | Title\*, Type\* (AGM / EGM / ECM), Date, Start Time, End Time, Location | End time ≥ start time when both present. |
| **Project** | Title\*, Status\* (Planned / Ongoing / Completed / Cancelled, default Planned), Start Date, Target End Date, Actual End Date | Start ≤ target end when both present. Start ≤ actual end when both present. |
| **Appointment** | Office\*, Member\* (active members only in the picker), Start Date\*, End Date | End ≥ start when present. Existing former-member appointments remain editable. See §11.1. |
| **Activity** | Title\*, Type\* (editable combo), Date, Start Time, End Time, Location | End time ≥ start time when both present. |
| **Competition** | Title\*, Year\* | `(title, year)` unique — enforced by schema; raw error shown on collision. |

Time and date fields accept ISO `YYYY-MM-DD` and `HH:MM` respectively; date pickers and time pickers are acceptable input widgets as long as the stored value is in those formats. Blank optional values are stored as NULL.

### 7.4 Relationship forms

Attendance Register, New / Edit Project Members and New / Edit Competition Team Assignments share one layout:

```
┌────────────────────────────────────────────────────────────┐
│ Meeting: [ AGM 2024 — 2024-03-12 ▾ ]                       │
├────────────────────────────────────────────────────────────┤
│ ☑ Alice Tan                                                │
│ ☑ Carol Ng (Former)                                        │
│ ☐ Bob Lim                                                  │
│ ☐ Dan Wong                                                 │
├────────────────────────────────────────────────────────────┤
│                                          [Cancel] [Save]   │
└────────────────────────────────────────────────────────────┘
```

- Top: parent dropdown (same lists as the scoped relationship pages).
- Checklist of members. Composition rule:
  - All members whose status is **Active** are listed.
  - Members whose status is **Former** are listed only if already assigned to this parent; they are marked "(Former)".
  - Existing assignments are pre-checked.
- Project Members and Competition Team Assignments add a **Role** field per checked member. Project role is a dropdown (`Member` / `Lead`); competition role is free text (schema imposes no allowed values).
- **Save** replaces the relationship set for the selected parent.
- Unchecking an existing member and saving removes the assignment. There is no separate Delete button on relationship forms.
- Cancel returns to the parent table. Save returns to the parent table, refreshed.

This is what makes the historical-data rule concrete: **existing former members remain visible and editable; the picker only offers active members for new assignments.**

---

## 8. Menus

Only two menus.

**File**
- Open Database…
- Create New Database…
- Save Backup…
- Restore Backup…
- Exit

**Help**
- About

Behaviour:

- Open Database, Create New Database, Save Backup and Restore Backup all use the standard file dialog.
- Open Database mid-session prompts to save / discard / cancel any dirty form first.
- Exit prompts only when a form is dirty.
- `Ctrl+Q` quits.
- No Tools, Tables, Forms, or Refresh menu.

---

## 9. Database handling

### 9.1 Startup

1. Read the remembered database path.
2. If it exists and is a valid MSMS database, open it and land on **All Members**.
3. Otherwise, show a **Database Selection** screen with two buttons: **Open Existing Database** and **Create New Database**.
4. The landing page is always **All Members**; other pages are never restored.

### 9.2 Create New Database

- File dialog for the destination.
- If the destination exists, show a strong warning and require explicit overwrite confirmation.
- Initialise from `schema.sql`, including indexes and `PRAGMA foreign_keys = ON`.
- Open the new database and land on All Members.

### 9.3 Open Existing Database

- Perform a lightweight compatibility check: the nine expected tables must exist with the expected columns.
- On failure, reject with a clear message. Do not modify the file.
- On success, open and land on All Members.

### 9.4 Backup

- `File → Save Backup…` asks for a destination and writes a copy of the current database using SQLite's online backup API, not a raw file copy.
- Report success or failure; never report success before the copy completes.

### 9.5 Restore

`File → Restore Backup…`:

1. Ask for explicit confirmation.
2. Validate the selected file: run `PRAGMA integrity_check` and the same compatibility check used on open.
3. Write a timestamped safety copy of the current database alongside it.
4. Atomically replace the working database (write to a temporary file, then rename).
5. If any step fails, leave the working database untouched and report the failure.
6. On success, refresh the workspace and land on All Members.

### 9.6 Foreign keys

`PRAGMA foreign_keys = ON` is issued on every connection. Every `DELETE` that a foreign key blocks surfaces the raw SQLite error plus a plain-language line naming the relationship that blocks it.

---

## 10. Error handling

- A failed save keeps the form open and all entered values intact.
- Database errors are shown with the raw SQLite message, plus a one-line plain explanation where one is available.
- Delete failures due to `ON DELETE RESTRICT` show the raw error and identify the blocking relationship.
- Backup validation and restore failures never leave the working database partially replaced.
- The application never reports success before the underlying operation has committed.
- The application never silently overwrites files, discards unsaved edits, or cascades deletes.

---

## 11. Validation rules

Validation is performed in the form before saving, and the schema remains the final authority. On any failure, the form stays open with values intact and shows the error.

### 11.1 Allowed values and required fields

Enforced by the schema `CHECK` constraints and by form controls:

- `members.status` ∈ {Active, Former}
- `meetings.type` ∈ {AGM, EGM, ECM}
- `projects.status` ∈ {Planned, Ongoing, Completed, Cancelled}
- `project_members.role` ∈ {Member, Lead}
- `(external_competitions.title, external_competitions.year)` unique

Required fields: `members.name`, `members.join_date`, `meetings.title`, `meetings.type`, `projects.title`, `projects.status`, `committee_appointments.office`, `committee_appointments.start_date`, `committee_appointments.member_id`, `activities.title`, `activities.type`, `external_competitions.title`, `external_competitions.year`.

`activities.type` and `competition_participants.role` have no allowed-value list in the schema; the form uses an editable combo (activities) and free text (competition role).

### 11.2 Date and time ordering

- Meeting / activity: end time ≥ start time when both present.
- Project: start ≤ target end; start ≤ actual end.
- Appointment: end date ≥ start date when present.
- All dates and times stored as `YYYY-MM-DD` and `HH:MM`.

### 11.3 Duplicates

- Schema constraints enforce hard uniqueness. Collisions are surfaced verbatim.
- Duplicate member names produce a **non-blocking** warning only. Names are never treated as unique.
- A `(title, year)` competition collision is a hard failure; the raw SQLite error is shown.

### 11.4 Known schema points affecting form behaviour

- **`committee_appointments` PK is `(start_date, office)`.** The Edit form treats the pair as identity: editing either field is either disallowed while the record exists, or handled by delete-and-reinsert. The simplest correct behaviour is to disable Start Date and Office while editing and offer Delete + New to change them; this will be chosen at implementation time. The current Committee page can therefore show at most one holder per (office, start date) — this is a property of the schema and is accepted.
- **`activities.type`** is free text. Filter values on All Activities are derived from the distinct values present.
- **`competition_participants.role`** is free text.

---

## 12. Dates, times and formatting

- Dates: `YYYY-MM-DD` on disk and on screen.
- Times: `HH:MM` on disk and on screen, 24-hour.
- Blank optional dates and times are NULL, not placeholder text.
- Meeting and activity dates may be blank.
- `members.join_date` and `committee_appointments.start_date` are required.

---

## 13. Startup and landing page

- No database → Database Selection screen.
- Valid database opened → land on **All Members**.
- Last opened path remembered; attempted on next launch; on failure the selection screen is shown.
- No home page, dashboard, or last-page restore.

---

## 14. Component inventory (build reference)

The whole application is built from a small set of reusable components:

1. **Database service** — open / create / validate / backup / restore / foreign-keys-on-connect. One place.
2. **Tree + workspace shell** — the window, the tree, page swapping.
3. **Table page component** — search box, optional filter control, read-only grid, New and Edit buttons.
4. **Scoped relationship page component** — parent dropdown, prev/next, header, read-only grid.
5. **Record form component** — fields, Save / Cancel / Delete, dirty tracking, validation.
6. **Relationship form component** — parent dropdown, member checklist with Role column, Save / Cancel.
7. **Prompt dialogs** — unsaved-changes, delete confirmation, overwrite confirmation, restore confirmation.
8. **About dialog.**

Every page in the app is an instance of (3) or (4). Every form is an instance of (5) or (6). This keeps the code surface small and consistent.

---

## 15. Build order

1. Database service + Database Selection screen + title bar filename.
2. Window shell: tree, workspace, File / Help menus.
3. Table page component; instantiate all seven record pages plus the three scoped relationship pages.
4. Record form component; instantiate the six record forms with Save / Cancel / Delete and validation.
5. Relationship form component; instantiate Attendance Register, New / Edit Project Members, New / Edit Competition Team Assignments.
6. Backup and Restore with validation and safety copy.
7. Unsaved-change prompts, exit handling, remember-last-database.
8. About dialog.

After step 4 the app is usable for day-to-day member, meeting and project administration. Steps 5–8 complete the requirements without changing any earlier component.

