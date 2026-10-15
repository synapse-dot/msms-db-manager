# MSMS Manager

Desktop application for managing MSMS membership and society records, built with C++17, Qt 6 Widgets, Qt SQL, and SQLite.

## Milestone 2: Members Module & Native Platform Appearance

Milestone 2 completes the Members module and replaces the earlier Catppuccin theme with native platform styling with restrained customisation.

### Implemented Features

1. **Visual Styling:**
   - Native platform look with restrained customisation.
   - Standard Qt platform widgets, system typography, and OS-appropriate palettes.
   - Removed Catppuccin Mocha theme overrides.

2. **Members Module:**
   - **All Members (`Members → All Members`):** Displays all active and former members in a read-only table with hidden internal `member_id`, real-time `Ctrl+F` search, predefined sorting, and explicit `New Member` and `Edit Member` actions.
   - **Active Members (`Members → Active Members`):** Displays only members whose status is `Active` with current-table search and empty state handling.
   - **New / Edit Member Form (`Members → New / Edit Member`):**
     - Full creation and editing with prepared statements.
     - Validates required Name and Join Date (YYYY-MM-DD), plus email and phone formats.
     - Warns on duplicate member names without blocking legitimate duplicates.
     - Save keeps form open; Cancel confirms discarding unsaved changes.
     - Delete confirms deletion and enforces foreign key restrictions (explaining why deletion is prevented if related historical records refer to the member).
     - Protects against navigating away from unsaved changes.


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
