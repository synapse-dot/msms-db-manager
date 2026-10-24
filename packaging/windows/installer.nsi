Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!ifndef APP_VERSION
!define APP_VERSION "1.0.0"
!endif
!ifndef OUTPUT_DIR
!define OUTPUT_DIR "out"
!endif
!ifndef APP_BINARY
!define APP_BINARY "out\msms-manager.exe"
!endif
!ifndef LICENSE_FILE
!define LICENSE_FILE "LICENSE"
!endif

!define PRODUCT_NAME "MSMS Manager"
!define UNINSTALL_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\MSMSManager"

Var Backup64RegistrationExists
Var Backup64DisplayName
Var Backup64DisplayVersion
Var Backup64Publisher
Var Backup64InstallLocation
Var Backup64UninstallString
Var Backup64NoModify
Var Backup64NoRepair
Var LegacyRegistrationExists
Var UninstallerWriteSucceeded

Name "${PRODUCT_NAME}"
Caption "${PRODUCT_NAME} Setup"
OutFile "${OUTPUT_DIR}\MSMSManager-${APP_VERSION}-win64.exe"
InstallDir "$PROGRAMFILES64\MSMS Manager"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show

VIProductVersion "${APP_VERSION}.0"
VIFileVersion "${APP_VERSION}.0"
; Use numeric LANGID 1033 (English, United States); do not rely on MUI macros.
VIAddVersionKey /LANG=1033 "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "FileVersion" "${APP_VERSION}.0"
VIAddVersionKey /LANG=1033 "CompanyName" "MSMS"
VIAddVersionKey /LANG=1033 "FileDescription" "${PRODUCT_NAME} installer"

!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ; Serialize setup and uninstaller processes so they cannot interleave the
  ; registration checks and legacy-key migration.
  System::Call 'kernel32::CreateMutexW(p 0, i 0, w "Global\MSMSManagerSetupMutex-v1") p .r1 ?e'
  Pop $R0
  StrCmp $R0 "0" setup_mutex_acquired
  System::Call 'kernel32::CloseHandle(p r1) i .r0'
  MessageBox MB_OK|MB_ICONEXCLAMATION "Another MSMS Manager setup or uninstaller is already running, or the setup lock could not be acquired. Close it and try again."
  Abort

setup_mutex_acquired:
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "MSMS Manager requires 64-bit Windows."
    Abort
  ${EndIf}

  ; InstallDirRegKey does not obey SetRegView. Read the target view explicitly,
  ; then fall back to the legacy 32-bit view for older installs.
  SetRegView 64
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
  IfErrors 0 setup_install_dir_checked_64
  StrCpy $0 ""
setup_install_dir_checked_64:
  StrCmp $0 "" 0 setup_install_dir_found
  SetRegView 32
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
  IfErrors 0 setup_install_dir_checked_32
  StrCpy $0 ""
setup_install_dir_checked_32:
  SetRegView 64
setup_install_dir_found:
  StrCmp $0 "" setup_init_done
  StrCpy $INSTDIR $0
setup_init_done:
FunctionEnd

Function un.onInit
  ; Use the same lock as setup to prevent install/uninstall overlap.
  System::Call 'kernel32::CreateMutexW(p 0, i 0, w "Global\MSMSManagerSetupMutex-v1") p .r1 ?e'
  Pop $R0
  StrCmp $R0 "0" un_setup_mutex_acquired
  System::Call 'kernel32::CloseHandle(p r1) i .r0'
  MessageBox MB_OK|MB_ICONEXCLAMATION "Another MSMS Manager setup or uninstaller is already running, or the setup lock could not be acquired. Close it and try again."
  Abort

un_setup_mutex_acquired:
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "The MSMS Manager uninstaller requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "MSMS Manager" SEC_MAIN
  SectionIn RO
  SetRegView 64
  SetShellVarContext all
  SetOutPath "$INSTDIR"

  ; Write and verify the new uninstaller before changing files or registry data.
  StrCpy $UninstallerWriteSucceeded "0"
  ClearErrors
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  IfErrors uninstaller_write_failed
  IfFileExists "$INSTDIR\Uninstall.exe" uninstaller_write_succeeded uninstaller_write_failed

uninstaller_write_succeeded:
  StrCpy $UninstallerWriteSucceeded "1"
  Goto uninstaller_write_check_done
uninstaller_write_failed:
  MessageBox MB_OK|MB_ICONSTOP "Could not create and verify the new uninstaller. Setup will stop without changing the uninstall registration."
  Quit
uninstaller_write_check_done:

  ; Back up the current 64-bit registration to allow a failed update to roll back.
  ClearErrors
  ReadRegStr $Backup64DisplayName HKLM "${UNINSTALL_REG_KEY}" "DisplayName"
  IfErrors no_previous_64_registration
  StrCpy $Backup64RegistrationExists "1"
  ClearErrors
  ReadRegStr $Backup64DisplayVersion HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion"
  ReadRegStr $Backup64Publisher HKLM "${UNINSTALL_REG_KEY}" "Publisher"
  ReadRegStr $Backup64InstallLocation HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
  ReadRegStr $Backup64UninstallString HKLM "${UNINSTALL_REG_KEY}" "UninstallString"
  ReadRegDWORD $Backup64NoModify HKLM "${UNINSTALL_REG_KEY}" "NoModify"
  ReadRegDWORD $Backup64NoRepair HKLM "${UNINSTALL_REG_KEY}" "NoRepair"
  IfErrors existing_64_registration_backup_failed
  Goto previous_64_registration_backup_done

no_previous_64_registration:
  StrCpy $Backup64RegistrationExists "0"
  StrCpy $Backup64DisplayName ""
  StrCpy $Backup64DisplayVersion ""
  StrCpy $Backup64Publisher ""
  StrCpy $Backup64InstallLocation ""
  StrCpy $Backup64UninstallString ""
  StrCpy $Backup64NoModify "0"
  StrCpy $Backup64NoRepair "0"
  ClearErrors
  Goto previous_64_registration_backup_done

existing_64_registration_backup_failed:
  MessageBox MB_OK|MB_ICONSTOP "The existing 64-bit uninstall registration could not be backed up. Setup will stop before changing registration data."
  Quit

previous_64_registration_backup_done:
  ; Record whether a legacy fallback exists before writing the new registration.
  SetRegView 32
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayName"
  IfErrors no_legacy_registration
  StrCpy $LegacyRegistrationExists "1"
  Goto legacy_registration_snapshot_done
no_legacy_registration:
  StrCpy $LegacyRegistrationExists "0"
  ClearErrors
legacy_registration_snapshot_done:
  SetRegView 64
  ; Static release executable: no wxWidgets or MSVC runtime DLLs are packaged.
  File /oname=msms-manager.exe "${APP_BINARY}"
  File /oname=LICENSE.txt "${LICENSE_FILE}"

  CreateDirectory "$SMPROGRAMS\MSMS Manager"
  CreateShortcut "$SMPROGRAMS\MSMS Manager\MSMS Manager.lnk" "$INSTDIR\msms-manager.exe"
  CreateShortcut "$DESKTOP\MSMS Manager.lnk" "$INSTDIR\msms-manager.exe"

  ClearErrors
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "Publisher" "MSMS"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoRepair" 1
  IfErrors registration_failed

  ; Verify the 64-bit registration, including the exact uninstaller command.
  SetRegView 64
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayName"
  IfErrors registration_failed
  StrCmp $0 "${PRODUCT_NAME}" 0 registration_failed
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion"
  IfErrors registration_failed
  StrCmp $0 "${APP_VERSION}" 0 registration_failed
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
  IfErrors registration_failed
  StrCmp $0 "$INSTDIR" 0 registration_failed
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "UninstallString"
  IfErrors registration_failed
  StrCmp $0 "$\"$INSTDIR\Uninstall.exe$\"" 0 registration_failed
  StrCmp $UninstallerWriteSucceeded "1" 0 registration_failed
  IfFileExists "$INSTDIR\Uninstall.exe" 0 registration_failed

  ; The shared mutex prevents another current installer from changing these
  ; values between verification and legacy-key deletion.
  StrCmp $LegacyRegistrationExists "1" 0 uninstall_registration_check_done
  SetRegView 32
  ClearErrors
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}\"
  IfErrors legacy_entry_cleanup_failed
  SetRegView 64
  Goto uninstall_registration_check_done

legacy_entry_cleanup_failed:
  SetRegView 64
  MessageBox MB_OK|MB_ICONEXCLAMATION "The new 64-bit uninstall registration is valid, but the legacy 32-bit entry could not be removed. The application remains registered; remove the duplicate entry after checking Add/Remove Programs."
  Goto uninstall_registration_check_done

registration_failed:
  ; Restore the previous 64-bit registration or remove newly-created partial values.
  SetRegView 64
  StrCmp $Backup64RegistrationExists "1" restore_previous_64_registration remove_partial_64_registration

restore_previous_64_registration:
  ClearErrors
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayName" "$Backup64DisplayName"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion" "$Backup64DisplayVersion"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "Publisher" "$Backup64Publisher"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "InstallLocation" "$Backup64InstallLocation"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "UninstallString" "$Backup64UninstallString"
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoModify" $Backup64NoModify
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoRepair" $Backup64NoRepair
  IfErrors restore_previous_64_registration_failed
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayName"
  IfErrors restore_previous_64_registration_failed
  StrCmp $0 "$Backup64DisplayName" 0 restore_previous_64_registration_failed
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion"
  IfErrors restore_previous_64_registration_failed
  StrCmp $0 "$Backup64DisplayVersion" 0 restore_previous_64_registration_failed
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
  IfErrors restore_previous_64_registration_failed
  StrCmp $0 "$Backup64InstallLocation" 0 restore_previous_64_registration_failed
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "UninstallString"
  IfErrors restore_previous_64_registration_failed
  StrCmp $0 "$Backup64UninstallString" 0 restore_previous_64_registration_failed
  Goto registration_failure_reported

restore_previous_64_registration_failed:
  ; If the legacy entry is available, prefer it over a possibly partial 64-bit entry.
  StrCmp $LegacyRegistrationExists "1" remove_failed_64_registration rollback_failed_without_legacy

remove_failed_64_registration:
  SetRegView 64
  ClearErrors
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}\"
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayName"
  IfErrors registration_failure_reported
  Goto rollback_failed

remove_partial_64_registration:
  SetRegView 64
  ClearErrors
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}\"
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayName"
  IfErrors registration_failure_reported
  Goto rollback_failed

rollback_failed:
  MessageBox MB_OK|MB_ICONSTOP "The uninstall registration failed, and NSIS could not fully restore or remove the 64-bit registration. The legacy 32-bit registration has not been deleted."
  Goto registration_failure_reported

rollback_failed_without_legacy:
  MessageBox MB_OK|MB_ICONSTOP "The uninstall registration failed, and NSIS could not fully restore the previous 64-bit registration. No legacy fallback was found; inspect the uninstall registration before another update."
  Goto registration_failure_reported

registration_failure_reported:
  SetRegView 64
  MessageBox MB_OK|MB_ICONEXCLAMATION "The new uninstall registration could not be verified. The old registration was restored or the partial new registration was removed where possible. The legacy 32-bit entry was not deleted."

uninstall_registration_check_done:
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$SMPROGRAMS\MSMS Manager\MSMS Manager.lnk"
  RMDir "$SMPROGRAMS\MSMS Manager"
  Delete "$DESKTOP\MSMS Manager.lnk"

  Delete "$INSTDIR\msms-manager.exe"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  ; Never recursively delete the install directory: it may contain a user DB.
  RMDir "$INSTDIR"
  ; Clean both registry views regardless of which entry launched this uninstaller.
  SetRegView 32
  ClearErrors
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}\"
  SetRegView 64
  ClearErrors
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}\"
SectionEnd
