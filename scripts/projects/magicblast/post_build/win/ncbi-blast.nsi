;NSIS Modern User Interface

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"
  !include "EnvVarUpdate.nsh"
  !include "x64.nsh"
  !include "unix2dos.nsh"
  
;--------------------------------
; Initialization function to properly set the installation directory
Function .onInit
  ${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\NCBI\magicblast-BLAST_VERSION"
  ${EndIf}
FunctionEnd

;--------------------------------
;General

  ;Name and file
  Name "NCBI Magic-BLAST BLAST_VERSION"
  OutFile "ncbi-magicblast-BLAST_VERSION.exe"
  ; Install/uninstall icons
  !define MUI_ICON "ncbilogo.ico"
  !define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\nsis1-uninstall.ico"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\NCBI\magicblast-BLAST_VERSION"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\NCBI\magicblast-BLAST_VERSION" ""

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_LICENSE "LICENSE"
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  ;!insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "DefaultSection" SecDflt
  
  SetOutPath "$INSTDIR\bin"
  
  File "magicblast.exe"
  File "ncbi-vdb-md.dll"
  File "makeblastdb.exe"
  File "libgcc_s_seh-1.dll"
  File "libgmp-10.dll"
  File "libgnutls-30.dll"
  File "libhogweed-4-2.dll"
  File "libnettle-6-2.dll"
  File "libp11-kit-0.dll"
  
  SetOutPath "$INSTDIR\doc"
  File "README"
  Push "$INSTDIR\doc\README"
  Push "$INSTDIR\doc\README.txt"
  Call unix2dos

  ;Store installation folder
  WriteRegStr HKCU "Software\NCBI\magicblast-BLAST_VERSION" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall-ncbi-magicblast-BLAST_VERSION.exe"
  
  ;Update PATH
  ${EnvVarUpdate} $0 "PATH" "P" "HKCU" "$INSTDIR\bin"
  
SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  Delete "$INSTDIR\Uninstall-ncbi-magicblast-BLAST_VERSION.exe"
  RMDir /r "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\NCBI\magicblast-BLAST_VERSION"
  
  ; Remove installation directory from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR\bin"

SectionEnd
