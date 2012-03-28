;NSIS Modern User Interface

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"
  !include "EnvVarUpdate.nsh"
  !include "x64.nsh"
  
;--------------------------------
; Initialization function to properly set the installation directory
Function .onInit
  ${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\NCBI\igblast-BLAST_VERSION"
  ${EndIf}
FunctionEnd

;--------------------------------
;General

  ;Name and file
  Name "NCBI igBLAST BLAST_VERSION"
  OutFile "ncbi-igblast-BLAST_VERSION.exe"
  ; Install/uninstall icons
  !define MUI_ICON "ncbilogo.ico"
  !define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\nsis1-uninstall.ico"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\NCBI\igblast-BLAST_VERSION"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\NCBI\igblast-BLAST_VERSION" ""

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
  
  File "igblastn.exe"
  File "igblastp.exe"
  
  SetOutPath "$INSTDIR\doc"
  File "README.txt"
  
  ;Store installation folder
  WriteRegStr HKCU "Software\NCBI\igblast-BLAST_VERSION" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall-ncbi-igblast-BLAST_VERSION.exe"
  
  ;Update PATH
  ${EnvVarUpdate} $0 "PATH" "P" "HKCU" "$INSTDIR\bin"
  
SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  Delete "$INSTDIR\Uninstall-ncbi-igblast-BLAST_VERSION.exe"
  
  Delete "$INSTDIR\bin\igblastn.exe"
  Delete "$INSTDIR\bin\igblastp.exe"
  Delete "$INSTDIR\doc\README.txt"
  RmDir "$INSTDIR\bin"
  RmDir "$INSTDIR\doc"
  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\NCBI\igblast-BLAST_VERSION"
  
  ; Remove installation directory from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR\bin" 

SectionEnd
