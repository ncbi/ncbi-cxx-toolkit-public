;NSIS Modern User Interface
; $Id$

Var hCtl_custom_page_v1
Var hCtl_custom_page_v1_TextBox1
Var hCtl_share_data_chk
Var share_data_opt_in
Var hCtl_summary_page
Var hCtl_summary_install_path
Var hCtl_summary_opt_text



;--------------------------------
;Include Modern UI

  !include "MUI.nsh"
  !include "EnvVarUpdate.nsh"  
  !include "x64.nsh"
  !include "nsDialogs.nsh"
  !include "LogicLib.nsh"
  !include "winmessages.nsh"
;---  !include "EnvVar.nsh"

;--- define some constants
!define SCROLL_CONTENT_TOP 6
!define NSIS_CUSTOM_PAGE_LAYOUT 1018
	
;--------------------------------
; Initialization function to properly set the installation directory
Function .onInit
  StrCpy $share_data_opt_in 0
  ${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\NCBI\blast-BLAST_VERSION+"
  ${EndIf}
FunctionEnd

Function fnc_data_usage_page_Create
  nsDialogs::Create ${NSIS_CUSTOM_PAGE_LAYOUT}
  Pop $hCtl_custom_page_v1
  ${If} $hCtl_custom_page_v1 == error
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "Current settings" "Review usage sharing preferences before installing NCBI BLAST package"

  ${NSD_CreateLabel} 8u 8u 277u 12u "Current settings:"
  Pop $0

  ${NSD_CreateCheckbox} 8u 24u 277u 12u "Share usage data with NCBI"
  Pop $hCtl_share_data_chk
  ${NSD_Uncheck} $hCtl_share_data_chk

  nsDialogs::CreateControl EDIT \
    "${WS_VISIBLE}|${WS_CHILD}|${WS_TABSTOP}|${WS_VSCROLL}|${ES_MULTILINE}|${ES_AUTOVSCROLL}|${ES_WANTRETURN}" \
    ${WS_EX_CLIENTEDGE} 8u 42u 277u 84u ""
  Pop $hCtl_custom_page_v1_TextBox1

  ; Load the privacy notice embedded in the installer.
  SetOutPath "$PLUGINSDIR"
  File /oname=BLAST_PRIVACY "BLAST_PRIVACY"
  ; Open file for reading
  FileOpen $R2 "$PLUGINSDIR\\BLAST_PRIVACY" r
  
  ; Read entire file using a simple loop
  
  ${Do}
    ClearErrors
    FileRead $R2 $R1
    ${If} ${Errors}
      ${ExitDo}
    ${EndIf}
    ; OLD  Append line with newline
    ; OLD StrCpy $R0 "$R0$R1"
	; NEW: Append new text to a one in control
	SendMessage $hCtl_custom_page_v1_TextBox1 ${EM_SETSEL} -1 -1
	SendMessage $hCtl_custom_page_v1_TextBox1 ${EM_REPLACESEL} 0, "STR:$R1"
  ${Loop}
  
  ; Close file
  FileClose $R2
  
  ;OLD: SendMessagee $hCtl_custom_page_v1_TextBox1 ${WM_SETTEXT} 0 "STR:$R0"
  SendMessage $hCtl_custom_page_v1_TextBox1 ${WM_VSCROLL} ${SCROLL_CONTENT_TOP} 0
FunctionEnd

Function fnc_data_usage_page_Show
  Call fnc_data_usage_page_Create

  ; Present CONTINUE as the forward action on this page.
  GetDlgItem $0 $HWNDPARENT 1
  SendMessage $0 ${WM_SETTEXT} 0 "STR:CONTINUE"

  nsDialogs::Show
FunctionEnd

Function fnc_data_usage_page_Leave
  ${NSD_GetState} $hCtl_share_data_chk $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $share_data_opt_in 1
  ${Else}
    StrCpy $share_data_opt_in 0
  ${EndIf}
FunctionEnd

Function fnc_summary_page_Create
  nsDialogs::Create ${NSIS_CUSTOM_PAGE_LAYOUT}
  Pop $hCtl_summary_page
  ${If} $hCtl_summary_page == error
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "Installation Summary" "Review installation details before continuing"

  ${NSD_CreateLabel} 8u 8u 277u 12u "Where software will be installed"
  Pop $0
  ${NSD_CreateLabel} 8u 22u 277u 14u "$INSTDIR"
  Pop $hCtl_summary_install_path

  ${NSD_CreateLabel} 8u 44u 277u 12u "Usage data setting"
  Pop $0

  ${If} $share_data_opt_in == 0
    ${NSD_CreateLabel} 8u 58u 277u 14u "No usage information will be share with NCBI"
    Pop $hCtl_summary_opt_text
  ${Else}
    ${NSD_CreateLabel} 8u 58u 277u 14u "Will share usage data with NCBI"
    Pop $hCtl_summary_opt_text
  ${EndIf}

  ${NSD_CreateLabel} 8u 82u 277u 24u "Press CONTINUE to proceed with installation or ABORT to cancel."
  Pop $0
FunctionEnd

Function fnc_summary_page_Show
  Call fnc_summary_page_Create

  ; Present only ABORT / CONTINUE on this page.
  GetDlgItem $0 $HWNDPARENT 3
  ShowWindow $0 ${SW_HIDE}
  GetDlgItem $0 $HWNDPARENT 1
  SendMessage $0 ${WM_SETTEXT} 0 "STR:CONTINUE"
  GetDlgItem $0 $HWNDPARENT 2
  SendMessage $0 ${WM_SETTEXT} 0 "STR:ABORT"

  nsDialogs::Show
FunctionEnd

Function fnc_summary_page_Leave
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
  Page custom fnc_data_usage_page_Show fnc_data_usage_page_Leave
  !insertmacro MUI_PAGE_DIRECTORY
  Page custom fnc_summary_page_Show fnc_summary_page_Leave
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

  ${If} $share_data_opt_in == 0
    WriteINIStr "$PROFILE\bin\ncbi.ini" BLAST BLAST_USAGE_REPORT false
  ${Else}
    WriteINIStr "$PROFILE\bin\ncbi.ini" BLAST BLAST_USAGE_REPORT true 
  ${EndIf}
  
  
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
  ; -- Env::AddValue "PATH" "$INSTDIR\bin"
  Pop $0
  
SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  Delete "$INSTDIR\Uninstall-ncbi-blast-BLAST_VERSION+.exe"
  Delete "$INSTDIR\bin\ncbi.ini"
  
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
  Delete "$INSTDIR\doc\BLAST_PRIVACY"
  RmDir "$INSTDIR\bin"
  RmDir "$INSTDIR\doc"
  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\NCBI\blast-BLAST_VERSION+"
  
  ; Remove installation directory from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR\bin" 
  ; -- EnvVar::DeleteValue "PATH" "$INSTDIR\bin"
  
  Pop $0

SectionEnd
