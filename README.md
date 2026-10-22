# MSMS Manager

**MSMS Manager** is a lightweight, offline desktop application designed for current and future MSMS secretaries to view, create, and manage society records without requiring SQL knowledge.

The application operates entirely offline on a user-selected SQLite database file per the society schema. There is no cloud synchronization, network dependency, or proprietary licensing.

---

## Key Principles & Features

- **Data Safety:** Strict foreign-key enforcement (`PRAGMA foreign_keys = ON`), no silent overwrites, no cascading deletes, and safe atomic backups/restores.
- **Predictability & Simplicity:** A single hierarchical navigation tree, consistent record and relationship forms, and clear plain-language error reporting.
- **Native Look & Feel:** Built with native platform controls using **wxWidgets 3.2** for high responsiveness and a lightweight memory footprint (< 15 MB binary).
- **Core Modules:**
  - **Members:** Directory management, active/former status tracking, join dates.
  - **Meetings & Attendance:** AGM, EGM, ECM tracking, attendance checklists with former-member retention.
  - **Projects & Teams:** Project tracking (Planned, Ongoing, Completed, Cancelled) and member role assignments (Member, Lead).
  - **Committee Appointments:** Office appointments, start/end dates, and computed Current Committee view.
  - **Society Activities:** Events and workshops with dynamic activity-type filtering.
  - **External Competitions:** Competitions and participant team rosters with role tracking.

---

## System Requirements

- **C++ Compiler:** C++17 compatible (GCC 9+, Clang 12+, MSVC 2019+)
- **Build System:** CMake ≥ 3.27
- **GUI Toolkit:** wxWidgets 3.2.x (minimum 3.2.0, 3.2.5+ recommended)
- **Supported Platforms:**
  - **Linux:** x86_64 distributions with glibc ≥ 2.31 (Debian 11+, Ubuntu 20.04+, Fedora 39+, Arch Linux)
  - **Windows:** Windows 10 (21H2+) and Windows 11 (22H2+)

---

## Building and Running

The project uses standard CMake presets for configuration and compilation.

### Prerequisites

#### Linux (Debian / Ubuntu)
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libwxgtk3.2-dev
```

#### Linux (Arch Linux)
```bash
sudo pacman -S base-devel cmake wxwidgets-gtk3
```

#### Windows
Dependencies can be installed via [vcpkg](https://github.com/microsoft/vcpkg):
```cmd
vcpkg install wxwidgets:x64-windows catch2:x64-windows
```

### Build Commands

#### Release Build
```bash
cmake --preset release
cmake --build --preset release
```

Launch the application:
```bash
# Linux
./build/release/msms-manager

# Windows
.\build\release\msms-manager.exe
```

#### Debug Build & Running Tests
```bash
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

---

## Windows SmartScreen Notice

The Windows installer and executable binaries for MSMS Manager are currently unsigned. When running the installer or executable for the first time on Windows 10 or Windows 11, Microsoft Defender SmartScreen may display an alert:

> **"Windows protected your PC — Microsoft Defender SmartScreen prevented an unrecognized app from starting."**

To proceed:
1. Click **More info**.
2. Click **Run anyway**.

Users can independently verify that the downloaded binary has not been tampered with by verifying its SHA-256 checksum against our published release manifest as described below.

---

## Verifying Release Integrity (SHA-256)

Every official release includes a `SHA256SUMS` file alongside installer and AppImage packages.

### Linux
Run the following in the terminal containing the downloaded release files:
```bash
sha256sum -c SHA256SUMS --ignore-missing
```
You should see `OK` next to the verified files.

### Windows (PowerShell)
To calculate the SHA-256 hash of the downloaded installer:
```powershell
Get-FileHash -Algorithm SHA256 .\MSMSManager-<version>-win64.exe
```
Compare the output string with the value published in `SHA256SUMS`.

### Windows (Command Prompt)
```cmd
certutil -hashfile MSMSManager-<version>-win64.exe SHA256
```

---

## License

MSMS Manager is free software licensed under the **GNU General Public License v3.0 or later** ([GPL-3.0-or-later](LICENSE)).
