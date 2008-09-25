;NSIS Modern User Interface

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"
  !include "EnvVarUpdate.nsh"

;--------------------------------
;General

  ;Name and file
  Name "NCBI BLAST BLAST_VERSION+"
  OutFile "ncbi-blast-BLAST_VERSION+.exe"
  ; Install/uninstall icons
  !define MUI_ICON "ncbilogo.ico"
  !define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\nsis1-uninstall.ico"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\NCBI\blast-BLAST_VERSION+"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\NCBI\blast-BLAST_VERSION+" ""

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
  
  File "blastn.exe"
  File "blastp.exe"
  File "blastx.exe"
  File "tblastn.exe"
  File "tblastx.exe"
  File "psiblast.exe"
  File "rpsblast.exe"
  File "rpstblastn.exe"
  File "legacy_blast.pl"
  File "makeblastdb.exe"
  File "blastdbcmd.exe"
  File "blastdb_aliastool.exe"
  File "segmasker.exe"
  File "dustmasker.exe"
  File "windowmasker.exe"
  
  SetOutPath "$INSTDIR\doc"
  File "user_manual.pdf"
  
  ;Store installation folder
  WriteRegStr HKCU "Software\NCBI\blast-BLAST_VERSION+" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall-ncbi-blast-BLAST_VERSION+.exe"
  
  ;Update PATH
  ${EnvVarUpdate} $0 "PATH" "P" "HKCU" "$INSTDIR\bin"
  
SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  Delete "$INSTDIR\Uninstall-ncbi-blast-BLAST_VERSION+.exe"
  
  Delete "$INSTDIR\bin\blastn.exe"
  Delete "$INSTDIR\bin\blastp.exe"
  Delete "$INSTDIR\bin\blastx.exe"
  Delete "$INSTDIR\bin\tblastn.exe"
  Delete "$INSTDIR\bin\tblastx.exe"
  Delete "$INSTDIR\bin\psiblast.exe"
  Delete "$INSTDIR\bin\rpsblast.exe"
  Delete "$INSTDIR\bin\rpstblastn.exe"
  Delete "$INSTDIR\bin\legacy_blast.pl"
  Delete "$INSTDIR\bin\makeblastdb.exe"
  Delete "$INSTDIR\bin\blastdbcmd.exe"
  Delete "$INSTDIR\bin\blastdb_aliastool.exe"
  Delete "$INSTDIR\bin\segmasker.exe"
  Delete "$INSTDIR\bin\dustmasker.exe"
  Delete "$INSTDIR\bin\windowmasker.exe"
  Delete "$INSTDIR\doc\user_manual.pdf"
  RmDir "$INSTDIR\bin"
  RmDir "$INSTDIR\doc"
  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\NCBI\blast-BLAST_VERSION+"
  
  ; Remove installation directory from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR\bin" 

SectionEnd
