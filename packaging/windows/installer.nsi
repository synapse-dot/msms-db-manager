Unicode true
!include "MUI2.nsh"

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
InstallDirRegKey HKLM "${UNINSTALL_REG_KEY}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${PRODUCT_NAME} installer"

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

Section "MSMS Manager" SEC_MAIN
  SectionIn RO
  SetShellVarContext all
  SetOutPath "$INSTDIR"

  ; Static release executable: no wxWidgets or MSVC runtime DLLs are packaged.
  File /oname=msms-manager.exe "${APP_BINARY}"
  File /oname=LICENSE.txt "${LICENSE_FILE}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\MSMS Manager"
  CreateShortcut "$SMPROGRAMS\MSMS Manager\MSMS Manager.lnk" "$INSTDIR\msms-manager.exe"
  CreateShortcut "$DESKTOP\MSMS Manager.lnk" "$INSTDIR\msms-manager.exe"

  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "Publisher" "MSMS"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_REG_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_REG_KEY}" "NoRepair" 1
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
  DeleteRegKey HKLM "${UNINSTALL_REG_KEY}"
SectionEnd
