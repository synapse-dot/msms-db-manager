# MSMS Manager

Desktop application for managing MSMS membership and society records, built with C++17, Qt 6 Widgets, Qt SQL, and SQLite.

## Milestone 1 Overview

Milestone 1 establishes the core architectural foundation, database lifecycle, Catppuccin Mocha theme, navigation layout, raw database inspector, query console interface, and the read-only **All Members** landing view.

### Implemented Features

1. **Database Selection & Lifecycle:**
   - Startup database selection screen: open an existing MSMS database or create a new database.
   - Remembers the last opened database path in user settings.
   - File dialogs for opening existing SQLite files and creating new databases with overwrite confirmation.
   - Dynamic database switching via **File → Open / Switch Database...**.
   - Atomic database creation with transactional rollback on failure.
   - Strict validation rejecting incompatible, empty, or corrupted databases without modifying disk files.
   - Automatic `PRAGMA foreign_keys = ON;` enforcement on all connections.

2. **Database Infrastructure:**
   - Exact nine-table schema defined in `design.md`:
     - `members`
     - `meetings`
     - `meeting_attendance`
     - `projects`
     - `project_members`
     - `committee_appointments`
     - `activities`
     - `external_competitions`
     - `competition_participants`
   - All schema constraints, date/time ordering checks, foreign keys with `ON DELETE RESTRICT`, and unique constraints.
   - Required indexes: `idx_members_status`, `idx_attendance_member`, `idx_project_members_member`, `idx_committee_member`, `idx_competition_participants_member`.

3. **User Interface & Catppuccin Mocha Theme:**
   - Cohesive Catppuccin Mocha palette applied across menus, trees, tables, headers, dialogs, buttons, and status bars.
   - Menu bar in specified order: **File**, **Tools**, **Tables**, **Forms**, **Help**.
   - Collapsible navigation tree with all functional categories (**Members**, **Meetings**, **Projects**, **Committee**, **Activities**, **External Competition**).
   - Single-page main workspace replacing views without tabs.
   - Raw Database Inspector via the **Tables** menu to view raw table contents with primary keys.
   - Clear placeholder cards for features and forms scheduled for future milestones.
   - Compact SQL query entry bar at the bottom with transaction wrapping, statement safety checks, single-statement enforcement, DDL prohibition, and raw SQLite error reporting.

4. **All Members View:**
   - Default landing page immediately after opening a database.
   - Displays all active and former members in a read-only table.
   - Internal `member_id` is hidden; displays Name, Class, Phone, Email, Status, Join Date.
   - Real-time `Ctrl+F` search filter across member fields.
   - Predefined sort order with non-clickable headers.
   - Graceful handling of empty databases with empty state notification.

## Prerequisites

- C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.21+
- Qt 6 (Widgets, Sql, Test)
- SQLite 3

## Build Instructions

```bash
# Configure the build directory
cmake -B build -S .

# Compile the application and test suite
cmake --build build
```

## Running the Application

```bash
./build/MSMSManager
```

On first run, the database selection dialog will prompt you to open an existing database or create a new one.

## Running Automated Tests

Run the full automated test suite using `ctest`:

```bash
ctest --test-dir build --output-on-failure
```

Or run the test binary directly:

```bash
./build/MSMSTests
```

### Test Coverage

The automated tests (`tests/test_database.cpp`) verify:
- Complete nine-table schema creation and column definitions
- Creation and presence of all 5 required indexes
- Valid database opening and integrity verification (`PRAGMA integrity_check`)
- Incompatible database rejection without disk modification (non-existent, non-SQLite, 0-byte, and missing-table databases)
- Important constraints: foreign keys enabled, `ON DELETE RESTRICT`, status `CHECK`, meeting time chronological `CHECK`, project status `CHECK`, competition `UNIQUE(title, year)`, appointment date `CHECK`
- Failed initialization transactional rollback
- `AllMembersView` read-only behavior and internal `member_id` column hiding

## Project Structure

```
msms-db-manager/
├── CMakeLists.txt              # Build configuration
├── README.md                   # Project documentation
├── design.md                   # Product design specification
├── main.cpp                    # Application entry point
├── mainwindow.h / .cpp         # Main application window
├── database/                   # Reference SQL scripts
│   ├── schema.sql              # MSMS schema reference
│   └── seed.sql                # Seed data reference
├── src/
│   ├── database/
│   │   ├── DatabaseManager.h/.cpp # SQLite connection, transactions & validation
│   │   └── Schema.h/.cpp          # DDL statements and schema definitions
│   └── ui/
│       ├── Theme.h/.cpp                  # Catppuccin Mocha stylesheet
│       ├── DatabaseSelectionDialog.h/.cpp# Startup DB selection & creation
│       ├── AllMembersView.h/.cpp         # Read-only members table view
│       ├── QueryConsoleView.h/.cpp       # Query result view & bottom query bar
│       ├── RawTableView.h/.cpp           # Raw database inspector view
│       └── PlaceholderView.h/.cpp        # Milestone placeholder card
└── tests/
    └── test_database.cpp       # Automated QtTest suite
```
