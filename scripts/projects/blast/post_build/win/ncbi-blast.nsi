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
    StrCpy $INSTDIR "$PROGRAMFILES64\NCBI\blast-BLAST_VERSION+"
  ${EndIf}
FunctionEnd

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
  File "update_blastdb.pl"
  File "makeblastdb.exe"
  File "makembindex.exe"
  File "makeprofiledb.exe"
  File "blastdbcmd.exe"
  File "blastdb_aliastool.exe"
  File "segmasker.exe"
  File "dustmasker.exe"
  File "windowmasker.exe"
  File "convert2blastmask.exe"
  File "blastdbcheck.exe"
  File "blast_formatter.exe"
  File "deltablast.exe"
  
  SetOutPath "$INSTDIR\doc"
  File "README.txt"
  
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
  Delete "$INSTDIR\bin\update_blastdb.pl"
  Delete "$INSTDIR\bin\makeblastdb.exe"
  Delete "$INSTDIR\bin\makembindex.exe"
  Delete "$INSTDIR\bin\makeprofiledb.exe"
  Delete "$INSTDIR\bin\blastdbcmd.exe"
  Delete "$INSTDIR\bin\blastdb_aliastool.exe"
  Delete "$INSTDIR\bin\segmasker.exe"
  Delete "$INSTDIR\bin\dustmasker.exe"
  Delete "$INSTDIR\bin\windowmasker.exe"
  Delete "$INSTDIR\bin\convert2blastmask.exe"
  Delete "$INSTDIR\bin\blastdbcheck.exe"
  Delete "$INSTDIR\bin\blast_formatter.exe"
  Delete "$INSTDIR\bin\deltablast.exe"
  Delete "$INSTDIR\doc\README.txt"
  RmDir "$INSTDIR\bin"
  RmDir "$INSTDIR\doc"
  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\NCBI\blast-BLAST_VERSION+"
  
  ; Remove installation directory from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR\bin" 

SectionEnd
