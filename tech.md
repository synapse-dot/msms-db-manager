# MSMS Manager — Final Technology Stack Specification

**Status:** Final, supersedes all earlier stack discussions  
**Companion document:** MSMS Manager — Final Design Specification  
**Project:** Open source, offline desktop application  
**Licence:** GNU General Public License v3.0 or later (GPL-3.0-or-later)

---

## 1. Purpose

This document fixes the implementation stack for MSMS Manager so that the design specification can be built without further technology debate, and so that future maintainers inherit a single, explicit set of choices.

The stack satisfies four constraints simultaneously:

1. **Lightweight** — small binaries, low memory, no heavy runtime.
2. **Native look** — real platform widgets, per §2.5 of the design specification.
3. **Quick to build** — fast local iteration, small consistent code surface.
4. **Open source friendly** — GNU-licensed, permissively usable dependencies, reproducible builds.

---

## 2. Supported platforms

The application is supported on exactly these platforms. Nothing else is claimed.

| Platform | Minimum version | Architecture | glibc floor |
|---|---|---|---|
| Windows 10 | 21H2 | x86_64 | n/a |
| Windows 11 | 22H2 | x86_64 | n/a |
| Debian | 11 (Bullseye) | x86_64 | **2.31** |
| Ubuntu | 20.04 LTS | x86_64 | **2.31** |
| Fedora | 39 | x86_64 | 2.38 |
| Arch Linux | current rolling | x86_64 | ≥ 2.40 |

**Linux glibc floor: 2.31.** Every listed Linux distribution provides at least 2.31 (Debian 11 and Ubuntu 20.04 are the oldest). All Linux artefacts are built against this floor, so they run on the newest releases of Fedora and Arch as well, because glibc is forward-compatible.

The Linux AppImage does **not** target musl-based distributions (e.g. Alpine), NixOS, or distributions older than the floor above. This is stated explicitly so that support claims match reality.

---

## 3. Stack summary

| Layer | Decision | Version |
|---|---|---|
| Language | **C++** | C++17 (C++20 permitted where all target toolchains accept it) |
| GUI toolkit | **wxWidgets** | 3.2.x (minimum 3.2.0; 3.2.5+ preferred) |
| Build system | **CMake** | ≥ 3.27 |
| Database | **SQLite amalgamation** (public domain) | 3.45.x or newer |
| DB wrapper | **In-project `DbService` class** | n/a |
| Testing | **Catch2** | 3.x |
| Logging / errors | **wxLog** + in-project `ErrorReporter` | n/a |
| Formatting / lint | **clang-format** + **clang-tidy** | latest stable |
| Packaging — Windows | **NSIS** | latest stable |
| Packaging — Linux | **AppImage** built with `linuxdeploy` + **`linuxdeploy-plugin-gtk`** | latest stable |
| CI | **GitHub Actions**, two-platform matrix | n/a |
| Dependency fetch | **vcpkg** (Windows), system packages (Linux CI) | latest stable |
| Project licence | **GPL-3.0-or-later** | n/a |
| Code signing | **None** | n/a |

---

## 4. Layer-by-layer decisions

### 4.1 Language: C++17

C++17 is the minimum: fully supported by MSVC 2019+, Clang 12+, GCC 9+, and wxWidgets 3.2. C++20 features may be used where all CI toolchains accept them; the project must not require C++20 to build.

### 4.2 GUI: wxWidgets 3.2.x

wxWidgets is selected because it is the only option that delivers **true native widgets** on Windows while remaining in the single-digit-to-low-double-digit megabyte range.

- **Widgets used:** `wxTreeCtrl`, `wxListCtrl` (report mode) or `wxGrid`, `wxChoice`, `wxCheckListBox`, `wxDatePickerCtrl`, `wxTextCtrl`, `wxMessageDialog`, `wxAboutBox`.
- **Linking:**
  - Windows: static linking of wxWidgets into the executable.
  - Linux: static `wxGTK` (`--disable-shared`) so the binary does not require a system `libwxgtk`.
- **Licence:** wxWindows Licence (LGPL with static-linking exception). Fully compatible with GPL-3.0-or-later. Static linking explicitly permitted.

**Important Linux note:** static wxGTK removes the need for a system wxWidgets runtime, but **GTK 3 itself cannot be statically linked** (this is a GTK limitation, not a wxWidgets one). The Linux artefact therefore bundles the GTK 3 runtime inside the AppImage using `linuxdeploy-plugin-gtk`. See §8.

### 4.3 Build system: CMake ≥ 3.27

- Use the config-file module shipped by wxWidgets, not CMake’s legacy `UsewxWidgets` module:

  ```cmake
  find_package(wxWidgets COMPONENTS Core Base REQUIRED)
  target_link_libraries(msms PRIVATE wxWidgets::Core wxWidgets::Base)
  ```

- Use **imported targets** throughout (`wxWidgets::Core`, `wxWidgets::Base`, `Catch2::Catch2WithMain`, `SQLite::SQLite3`). No manual include paths, no per-platform branching in `CMakeLists.txt`.
- If wxWidgets is not present on the host, fall back to `ExternalProject` fetching it from GitHub into the build tree. Subsequent builds reuse the tree.
- Provide `CMakePresets.json` with `debug`, `release`, `ci-windows`, `ci-linux` presets.

### 4.4 Database: SQLite amalgamation

- Compile `sqlite3.c` and `sqlite3.h` directly into the executable. Public domain, zero external dependencies.
- `PRAGMA foreign_keys = ON` is issued on every connection open, per the design specification.
- Backup uses the **SQLite online backup API** (`sqlite3_backup_*`), never a raw file copy, per §9.4 of the design specification.
- Restore uses write-to-temporary-then-atomic-rename, per §9.5 of the design specification.

**In-project `DbService` class** — the only abstraction between the application and SQLite. Responsibilities:

- Open / create / validate a database file.
- Execute prepared statements and return rows.
- Begin / commit / rollback for multi-row saves (relationship forms).
- Convert between `wxString` and UTF-8 at the boundary.
- Surface raw `sqlite3_errmsg()` text to `ErrorReporter`.

There is **no ORM**. Queries are written as SQL strings in the service layer, matching the design’s preference for transparent behaviour.

**Alternative considered:** wxSQLite3. Rejected because the query surface is small and fixed; the `DbService` boundary is designed so that wxSQLite3 can be substituted later if needed.

### 4.5 Testing: Catch2 3.x

- Fetched via CMake `FetchContent` or vcpkg, pinned to a 3.x release.
- Tests cover the **database service layer**: open, validate, backup, restore, foreign-key `RESTRICT` behaviour, and the date/time ordering rules from §11.2 of the design specification.
- GUI testing is out of scope for the initial build.
- Test binaries are excluded from release artefacts.

### 4.6 Logging and error reporting

- No external logging library.
- Application errors use **`wxLogError`**, which routes to the platform’s standard mechanism.
- `ErrorReporter` formats the raw SQLite error alongside the plain-language line required by §10 of the design specification.
- Debug builds may write to `stderr`; release builds do not.

### 4.7 Formatting and linting

- **`clang-format`** with a checked-in `.clang-format` (base style: LLVM, 4-space indent, 100-column limit).
- **`clang-tidy`** with a checked-in `.clang-tidy` (`bugprone-*`, `modernize-*`, `performance-*`; stylistic checks disabled).
- Both run in CI as an advisory job on pull requests.

---

## 5. Repository layout

```
msms-manager/
├── CMakeLists.txt
├── CMakePresets.json
├── .clang-format
├── .clang-tidy
├── schema.sql
├── LICENSE                      (GPL-3.0)
├── README.md
├── src/
│   ├── main.cpp
│   ├── db/
│   │   ├── DbService.h / .cpp
│   │   └── sqlite3.c / sqlite3.h        (vendored amalgamation)
│   ├── ui/
│   │   ├── MainFrame.h / .cpp
│   │   ├── TablePage.h / .cpp
│   │   ├── ScopedPage.h / .cpp
│   │   ├── RecordForm.h / .cpp
│   │   └── RelationshipForm.h / .cpp
│   ├── forms/                            (per-form instantiations)
│   └── util/
│       ├── ErrorReporter.h / .cpp
│       └── Validation.h / .cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_db_service.cpp
├── packaging/
│   ├── windows/installer.nsi
│   └── linux/appimage.sh
└── .github/workflows/build.yml
```

The `ui/` directory matches the component inventory in §14 of the design specification: every page is an instance of `TablePage` or `ScopedPage`, and every form is an instance of `RecordForm` or `RelationshipForm`.

---

## 6. Build and run

```bash
cmake --preset release
cmake --build --preset release
./build/release/msms-manager
```

Presets:

- `debug` — assertions on, verbose `wxLog`, no optimisation.
- `release` — `-O2`, assertions off, static wxWidgets.
- `ci-windows`, `ci-linux` — used by GitHub Actions.

No manual dependency configuration is required on a clean checkout: CMake fetches wxWidgets and Catch2 if they are not already present.

---

## 7. CI: GitHub Actions

Workflow `.github/workflows/build.yml`, **two-platform matrix**:

- `windows-latest` (x86_64)
- `ubuntu-20.04` (container — **pinned, not `ubuntu-latest`**), which provides glibc 2.31 and matches the Linux glibc floor in §2.

Why Ubuntu 20.04 is pinned: building on a newer runner would raise the required glibc version above the floor and the AppImage would fail on Debian 11 and Ubuntu 20.04. This is the single most important CI detail for Linux compatibility.

Per-platform steps:

1. Install dependencies.
   - **Windows:** `vcpkg` installs wxWidgets and Catch2.
   - **Linux:** `apt-get install` GTK 3 development packages, `ccache`, `linuxdeploy`, `linuxdeploy-plugin-gtk`.
2. Restore cache: **ccache** on Linux, vcpkg prefix and CMake build tree on Windows. Caching wxWidgets’ build result is the difference between a 20-minute and a 4-minute CI run.
3. Configure → build → run tests.
4. Package artefact (see §8).
5. Upload artefact.

Additional advisory job: `clang-format` and `clang-tidy` on pull requests.

---

## 8. Packaging

| Platform | Tool | Output |
|---|---|---|
| Windows | NSIS | `MSMSManager-<version>-win64.exe` |
| Linux | `linuxdeploy` + `linuxdeploy-plugin-gtk` | `MSMSManager-<version>-x86_64.AppImage` |

Versions are injected from Git tags (`v1.2.3`) via CMake `project(VERSION ...)`.

**Linux packaging must include `linuxdeploy-plugin-gtk`.** The AppImage bundles:

- The application executable (statically linked against wxWidgets and SQLite).
- GTK 3 runtime libraries, GIO modules, GDK pixbuf loaders, GLib schemas, and theme resources.
- Required non-glibc shared libraries discovered by `linuxdeploy`.

glibc is **not** bundled. The AppImage relies on the target system’s glibc being ≥ 2.31, which is guaranteed by §2.

**No code signing.** Windows binaries are unsigned; users may see a SmartScreen warning on first run. The README documents this and provides a SHA-256 checksum file alongside each release so users can verify downloads. Linux AppImages are unsigned and are made executable with `chmod +x` as usual.

---

## 9. Licence

The project is licensed under **GNU General Public License v3.0 or later** (`GPL-3.0-or-later`), recorded in `LICENSE`.

| Dependency | Licence | GPL-3.0 compatible |
|---|---|---|
| wxWidgets | wxWindows Licence (LGPL + static-linking exception) | Yes |
| SQLite | Public domain | Yes |
| Catch2 | BSL-1.0 | Yes |

No dependency imposes terms stricter than GPL-3.0. Source is published; anyone distributing a modified binary must publish their changes under the same licence.

---

## 10. Alternatives considered and rejected

| Option | Rejected because |
|---|---|
| **C++ + Qt** | Heavier (~20–40 MB); draws its own widgets rather than using native controls; larger API surface for a small app |
| **C++ + FLTK** | Lighter than wxWidgets but draws its own widgets, failing the native-look requirement; no built-in date picker |
| **Python + PySide6** | 80–150 MB bundles, higher memory, conflicts with the lightweight goal |
| **Python + wxPython** | Native widgets but 40–55 MB bundles and a Python runtime dependency |
| **Rust + Iced / Slint** | Excellent small binaries, but drawn (non-native) UI and slower compile cycles |
| **Rust + Tauri** | Smallest binaries, but webview UI and slower build loop |
| **macOS support** | Out of scope for this project; not in the supported platform list |
| **wxSQLite3** | Deferred; `DbService` boundary allows substitution later if needed |

---

## 11. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Building on a newer CI runner raises the glibc floor | Pin the Linux CI container to Ubuntu 20.04 |
| GTK 3 cannot be statically linked | Bundle GTK 3 via `linuxdeploy-plugin-gtk`; documented in §8 |
| Static wxWidgets builds are slow on first configure | CMake preset fetches and builds once into the build tree; CI caches the result |
| `sqlite3.c` recompiled on every clean build | Acceptable (seconds); optionally build it as a separate static-library target |
| Unsigned Windows binary triggers SmartScreen | Documented in README; SHA-256 checksums published per release |
| Arch / Fedora rolling releases outpace the build base | Forward-compatible by design: newer glibc is fine, glibc 2.31 is the floor |

---

## 12. Acceptance criteria

The stack is correctly implemented when:

1. A clean checkout builds on both CI platforms with the documented commands, with no manual dependency installation beyond the platform toolchain.
2. The release binary is under **15 MB** on each platform.
3. The application starts on a cold system in under **1 second**.
4. The Linux AppImage runs successfully on **Debian 11, Ubuntu 20.04, Ubuntu 22.04, Ubuntu 24.04, Fedora 39, and current Arch Linux** — verified manually using [the release checklist](packaging/RELEASE_CHECKLIST.md).
5. The Windows installer runs successfully on **Windows 10 21H2 and Windows 11 22H2** — verified manually using [the release checklist](packaging/RELEASE_CHECKLIST.md).
6. `PRAGMA foreign_keys = ON` is verifiably set on every connection (covered by a unit test).
7. Backup and restore pass the unit-test suite, including the interrupted-restore rollback case.
8. `clang-format` and `clang-tidy` pass without changes on `main`.
9. Every tagged release publishes: the Windows installer, the Linux AppImage, and a `SHA256SUMS` file.

---

## 13. Open items

- **Release packaging and checksums** — the tagged-release workflow is implemented in `.github/workflows/release.yml`. It builds the static Windows installer and static-wxGTK AppImage, generates `SHA256SUMS`, verifies the manifest, and publishes all three release files. Manual cross-platform sign-off remains required using [the release checklist](packaging/RELEASE_CHECKLIST.md).
- **wxSQLite3 fallback** — revisit only if the `DbService` layer exceeds roughly 300 lines of wrapper code.
