;NSIS Modern User Interface
; $Id$
Var hCtl_custom_page_v1
Var hCtl_custom_page_v1_TextBox1
Var data_collect_notice_text

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"
  !include "EnvVarUpdate.nsh"
  !include "x64.nsh"
  !include "nsDialogs.nsh"
  
;--------------------------------
; Initialization function to properly set the installation directory
Function .onInit
  ${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\NCBI\blast-BLAST_VERSION+"
	StrCpy $data_collect_notice_text "\
To help improve the quality of this product and ensure proper funding and$\r$\n\
resource allocation, we collect usage data. For a listing of data collected$\r$\n\
and how this is handled, please visit our$\r$\n\
privacy policy <https://ncbi.nlm.nih.gov/books/NBK569851/>.$\r$\n$\r$\n\
You may choose to opt out of this collection any time by setting the following$\r$\n\
environment variable:$\r$\n  $\r$\n\
                      BLAST_USAGE_REPORT=false$\r$\n"
  ${EndIf}
FunctionEnd

Function fnc_data_usage_page_Create

  ; === custom_page_v1 (type: Dialog) ===
  nsDialogs::Create 1018
  Pop $hCtl_custom_page_v1
  ${If} $hCtl_custom_page_v1 == error
    Abort
  ${EndIf}
  !insertmacro MUI_HEADER_TEXT "Data Collection Notice" "Please review data collection notice before installing NCBI BLAST package"

  ; === TextBox1 (type: TextMultiline) ===
  nsDialogs::CreateControl EDIT ${DEFAULT_STYLES}|${ES_AUTOHSCROLL}|${ES_AUTOVSCROLL}|${ES_MULTILINE}|${ES_WANTRETURN}|${WS_HSCROLL}|${WS_VSCROLL} ${WS_EX_WINDOWEDGE}|${WS_EX_CLIENTEDGE} 8u 7u 277u 121u $data_collect_notice_text

  
  Pop $hCtl_custom_page_v1_TextBox1
  SetCtlColors $hCtl_custom_page_v1_TextBox1 0x000000 0xFFFFFF
  SendMessage $hCtl_custom_page_v1_TextBox1 ${EM_SETREADONLY} 1 0
  SendMessage $hCtl_custom_page_v1_TextBox1 ${ES_MULTILINE} 0 0

FunctionEnd

; dialog show function
Function fnc_data_usage_page_Show
  Call fnc_data_usage_page_Create
  nsDialogs::Show
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
  Page custom fnc_data_usage_page_Show
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
  File "cleanup-blastdb-volumes.py"
  File "get_species_taxids.sh"
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
  File "nghttp2.dll"
  File "ncbi-vdb-md.dll"
  File "blastn_vdb.exe"
  File "tblastn_vdb.exe"
  File "blast_formatter_vdb.exe"
  
  SetOutPath "$INSTDIR\doc"
  File "README.txt"
  File "BLAST_PRIVACY"
  
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
  Delete "$INSTDIR\bin\cleanup-blastdb-volumes.py"
  Delete "$INSTDIR\bin\get_species_taxids.sh"
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
  Delete "$INSTDIR\bin\nghttp2.dll"
  Delete "$INSTDIR\bin\ncbi-vdb-md.dll"
  Delete "$INSTDIR\bin\blastn_vdb.exe"
  Delete "$INSTDIR\bin\tblastn_vdb.exe"
  Delete "$INSTDIR\bin\blast_formatter_vdb.exe"
  Delete "$INSTDIR\doc\README.txt"
  RmDir "$INSTDIR\bin"
  RmDir "$INSTDIR\doc"
  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\NCBI\blast-BLAST_VERSION+"
  
  ; Remove installation directory from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR\bin" 

SectionEnd
