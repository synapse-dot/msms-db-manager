Unicode true
!include "MUI2.nsh"
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
; LANGID 1033 is English (United States). Use a numeric ID so version-resource
; language metadata does not depend on when MUI_LANGUAGE defines LANG_ENGLISH.
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
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "MSMS Manager requires 64-bit Windows."
    Abort
  ${EndIf}

  ; Current builds register in the 64-bit view. Fall back to the 32-bit view
  ; to preserve custom install paths written by earlier installer versions.
  SetRegView 64
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
  ${If} $0 == ""
    SetRegView 32
    ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
    SetRegView 64
  ${EndIf}
  ${If} $0 != ""
    StrCpy $INSTDIR $0
  ${EndIf}
FunctionEnd

Function un.onInit
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

  ; Static release executable: no wxWidgets or MSVC runtime DLLs are packaged.
  File /oname=msms-manager.exe "${APP_BINARY}"
  File /oname=LICENSE.txt "${LICENSE_FILE}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\MSMS Manager"
  CreateShortcut "$SMPROGRAMS\MSMS Manager\MSMS Manager.lnk" "$INSTDIR\msms-manager.exe"
  CreateShortcut "$DESKTOP\MSMS Manager.lnk" "$INSTDIR\msms-manager.exe"

  ; Do not remove the legacy registration unless the new 64-bit entry can be
  ; written and read back. Clear prior errors so this check covers these writes.
  ClearErrors
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "Publisher" "MSMS"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoRepair" 1
  IfErrors preserve_legacy_uninstall_entry

  SetRegView 64
  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayName"
  IfErrors preserve_legacy_uninstall_entry
  StrCmp $0 "${PRODUCT_NAME}" 0 preserve_legacy_uninstall_entry

  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion"
  IfErrors preserve_legacy_uninstall_entry
  StrCmp $0 "${APP_VERSION}" 0 preserve_legacy_uninstall_entry

  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
  IfErrors preserve_legacy_uninstall_entry
  StrCmp $0 "$INSTDIR" 0 preserve_legacy_uninstall_entry

  ClearErrors
  ReadRegStr $0 HKLM "${UNINSTALL_REG_KEY}" "UninstallString"
  IfErrors preserve_legacy_uninstall_entry
  StrCmp $0 "" preserve_legacy_uninstall_entry

  ; The 64-bit registration is present and has the expected values.
  ; Only now is it safe to remove the legacy 32-bit-view entry.
  SetRegView 32
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}"
  SetRegView 64
  Goto uninstall_registration_check_done

preserve_legacy_uninstall_entry:
  ; Keep the old Add/Remove Programs entry if the new entry cannot be verified.
  SetRegView 64
  MessageBox MB_OK|MB_ICONEXCLAMATION "The new Windows uninstall registration could not be verified. The previous uninstall entry has been retained."

uninstall_registration_check_done:
SectionEnd

Section "Uninstall"
  SetRegView 64
  SetShellVarContext all
  Delete "$SMPROGRAMS\MSMS Manager\MSMS Manager.lnk"
  RMDir "$SMPROGRAMS\MSMS Manager"
  Delete "$DESKTOP\MSMS Manager.lnk"

  Delete "$INSTDIR\msms-manager.exe"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  ; Never recursively delete the install directory: it may contain a user DB.
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}"
SectionEnd
