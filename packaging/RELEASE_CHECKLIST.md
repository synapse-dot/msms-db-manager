# MSMS Manager release verification checklist

Complete this checklist for every release candidate. Record the tag, commit, tester, date, and results. CI success and checksum verification do not replace manual OS compatibility tests.

## Release artefacts and integrity

- [ ] Release tag is a semantic version (vMAJOR.MINOR.PATCH) and matches package filenames.
- [ ] GitHub Release contains one Windows installer, one Linux x86_64 AppImage, and SHA256SUMS.
- [ ] SHA256SUMS contains both package filenames and verifies against the uploaded bytes.
- [ ] Linux: run `sha256sum -c SHA256SUMS --ignore-missing` in the downloaded release directory.
- [ ] Windows: compare `Get-FileHash -Algorithm SHA256 .\MSMSManager-<version>-win64.exe` with the manifest, or use certutil -hashfile.
- [ ] Record the application executable and AppImage sizes; both must meet the §12 acceptance criterion of under 15 MB for release binaries. Record installer size separately.

## Linux AppImage — compatibility matrix

Test on clean x86_64 installations. Record OS image/build, glibc version, date, and result.

| Distribution | Required checks | Result / notes |
|---|---|---|
| Debian 11 (Bullseye; glibc 2.31) | [ ] AppImage starts; [ ] database selection opens; [ ] create/open compatible database; [ ] basic read/write operation; [ ] close and relaunch | |
| Ubuntu 20.04 LTS | [ ] Same smoke tests; [ ] launch as a normal user | |
| Ubuntu 22.04 LTS | [ ] Same smoke tests; [ ] bundled GTK theme and icons render | |
| Ubuntu 24.04 LTS | [ ] Same smoke tests; [ ] no missing-library or GLib schema errors | |
| Fedora 39 | [ ] Same smoke tests; [ ] launch without installing wxWidgets | |
| Current Arch Linux | [ ] Same smoke tests; [ ] launch without installing wxWidgets | |

For each Linux target:
- [ ] Make the file executable with chmod +x MSMSManager-<version>-x86_64.AppImage.
- [ ] Launch as a standard, non-root user.
- [ ] Confirm the main window and database selection dialog render correctly.
- [ ] Open a disposable compatible database; verify a harmless edit persists after close/reopen.
- [ ] Confirm the original database was not changed; never test destructive operations on real society data.
- [ ] Check for startup errors from GTK, GIO modules, GLib schemas, or GDK Pixbuf loaders.

## Windows installer — compatibility matrix

Use clean x86_64 virtual machines. The installer should contain a statically linked executable and should not require wxWidgets or MSVC runtime DLLs beside the executable.

| Operating system | Required checks | Result / notes |
|---|---|---|
| Windows 10 21H2 | [ ] Install; [ ] launch; [ ] open disposable database; [ ] uninstall | |
| Windows 11 22H2 | [ ] Install; [ ] launch; [ ] open disposable database; [ ] uninstall | |

For each Windows target:
- [ ] Document unsigned installer/SmartScreen status.
- [ ] Install to both the default and a non-default directory.
- [ ] Confirm Start Menu and desktop shortcuts work.
- [ ] Confirm no wxWidgets or MSVC runtime DLLs are required next to the executable.
- [ ] Open a disposable database and complete a read/write smoke test.
- [ ] Uninstall and confirm executables, shortcuts, and uninstall registry entry are removed.
- [ ] Confirm uninstall does not delete a database stored outside the installation directory.

## Sign-off

- Release tag:
- Commit SHA:
- Tester:
- Test date:
- CI run:
- Outstanding failures / exceptions:
- Release approved by:
