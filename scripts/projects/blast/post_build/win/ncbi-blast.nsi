;NSIS Modern User Interface
; $Id$
;
; NCBI BLAST installer v2 (see installv2.txt).
;  - Starts without administrator rights (no UAC prompt at launch).
;  - "Choose Install Location" offers an all-users or a personal install.
;  - An all-users install started by a non-elevated user relaunches the
;    installer with the "runas" verb. Admins only confirm; standard users
;    enter administrator credentials.
;
; Command line:
;   /ALLUSERS       install for all users (system wide)
;   /CURRENTUSER    install for the current user only
;   /SHARE=0|1      usage-data sharing. Default is 1 (share; the checkbox
;                   is checked by default). /SHARE=0 turns sharing off.
;                   Any other value is ignored and the default is kept.
;   /ELEVATED       internal: set on the elevated relaunch; skips the
;                   license, data-usage and location pages
;   /S              silent; /S /ALLUSERS without elevation exits with 740
;   /D=<path>       install folder (must be last, not quoted)

Var hCtl_custom_page_v1
Var hCtl_custom_page_v1_TextBox1
Var hCtl_share_data_chk
Var share_data_opt_in
Var hCtl_summary_page
Var hCtl_summary_install_path
Var hCtl_summary_opt_text

Var hCtl_loc_page
Var hCtl_loc_radio_all
Var hCtl_loc_radio_user
Var hCtl_loc_dir
Var hCtl_loc_browse
Var hCtl_loc_hint
Var hCtl_loc_space

Var install_mode      ; "all" | "user"
Var sys_dir           ; folder last chosen for the all-users mode
Var user_dir          ; folder last chosen for the personal mode
Var is_elevated       ; 1 when the process token is elevated (or no UAC)
Var elevation_type    ; TokenElevationType: 1=default 2=full 3=limited
Var elevated_run      ; 1 in the relaunched (elevated) copy
Var check_path        ; in/out for ValidateInstallPath / CheckWriteAccess
Var check_msg         ; out: error text ("" when OK)
Var check_ok          ; out: 1 OK, 0 failed
Var space_req         ; MB required
Var space_free        ; MB free on the target drive ("" if unknown)

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"
  !include "EnvVarUpdate.nsh"
  !include "x64.nsh"
  !include "nsDialogs.nsh"
  !include "LogicLib.nsh"
  !include "winmessages.nsh"
  !include "FileFunc.nsh"
;--- String replace: use StrFunc's StrRep when StrFunc.nsh is loaded (it is,
;--- via EnvVarUpdate.nsh); otherwise define a local one with the same arguments.
;--- Keep this after all !include lines.
!ifdef StrRep
  !ifndef StrRep_INCLUDED
    ${Using:StrFunc} StrRep
  !endif
!else
  !define StrRep "!insertmacro StrRep"
  !macro StrRep output string old new
      Push `${string}`
      Push `${old}`
      Push `${new}`
      !ifdef __UNINSTALL__
          Call un.StrRep
      !else
          Call StrRep
      !endif
      Pop ${output}
  !macroend

  !macro Func_StrRep un
      Function ${un}StrRep
          Exch $R2 ;new
          Exch 1
          Exch $R1 ;old
          Exch 2
          Exch $R0 ;string
          Push $R3
          Push $R4
          Push $R5
          Push $R6
          Push $R7
          Push $R8
          Push $R9

          StrCpy $R3 0
          StrLen $R4 $R1
          StrLen $R6 $R0
          StrLen $R9 $R2
          loop:
              StrCpy $R5 $R0 $R4 $R3
              StrCmp $R5 $R1 found
              StrCmp $R3 $R6 done
              IntOp $R3 $R3 + 1 ;move offset by 1 to check the next character
              Goto loop
          found:
              StrCpy $R5 $R0 $R3
              IntOp $R8 $R3 + $R4
              StrCpy $R7 $R0 "" $R8
              StrCpy $R0 $R5$R2$R7
              StrLen $R6 $R0
              IntOp $R3 $R3 + $R9 ;move offset by length of the replacement string
              Goto loop
          done:

          Pop $R9
          Pop $R8
          Pop $R7
          Pop $R6
          Pop $R5
          Pop $R4
          Pop $R3
          Push $R0
          Push $R1
          Pop $R0
          Pop $R1
          Pop $R0
          Pop $R2
          Exch $R1
      FunctionEnd
  !macroend
  !insertmacro Func_StrRep ""
  ;!insertmacro Func_StrRep "un."
!endif

;--- define some constants
!define SCROLL_CONTENT_TOP 6
!define NSIS_CUSTOM_PAGE_LAYOUT 1018

!define BLAST_DIRNAME     "blast-BLAST_VERSION+"
!define BLAST_REGKEY      "Software\NCBI\blast-BLAST_VERSION+"
!define BLAST_UNINST_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\NCBI-BLAST-BLAST_VERSION+"
!define BLAST_UNINSTALLER "Uninstall-ncbi-blast-BLAST_VERSION+.exe"
!define ERROR_ELEVATION_REQUIRED 740
!define DRIVE_REMOTE 4

;--------------------------------
;General

  ;Name and file
  Name "NCBI BLAST BLAST_VERSION+"
  OutFile "ncbi-blast-BLAST_VERSION+.exe"
  ; Install/uninstall icons
  !define MUI_ICON "ncbilogo.ico"
  !define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\nsis1-uninstall.ico"

  ; Never ask for elevation up front; it is requested only for an
  ; all-users install (see fnc_relaunch_elevated).
  RequestExecutionLevel user

  ; No InstallDir/InstallDirRegKey: $INSTDIR is chosen in .onInit so that a
  ; /D= value can be told apart from the default.

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !define MUI_PAGE_CUSTOMFUNCTION_PRE fnc_skip_if_elevated
  !insertmacro MUI_PAGE_LICENSE "LICENSE"
  Page custom fnc_data_usage_page_Show fnc_data_usage_page_Leave
  Page custom fnc_location_page_Show fnc_location_page_Leave
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

  ${If} $install_mode == "all"
    SetShellVarContext all
  ${Else}
    SetShellVarContext current
  ${EndIf}

  SetOutPath "$INSTDIR\bin"

  ; Sharing is on only when explicitly 1; any other value means "false".
  ${If} $share_data_opt_in == 1
    WriteINIStr "$INSTDIR\bin\ncbi.ini" BLAST BLAST_USAGE_REPORT true
  ${Else}
    WriteINIStr "$INSTDIR\bin\ncbi.ini" BLAST BLAST_USAGE_REPORT false
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

  ;Store installation folder and mode (SHCTX = HKLM for all users, HKCU otherwise)
  WriteRegStr SHCTX "${BLAST_REGKEY}" "" $INSTDIR
  WriteRegStr SHCTX "${BLAST_REGKEY}" "InstallMode" $install_mode

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\${BLAST_UNINSTALLER}"

  ;Add/Remove Programs entry
  WriteRegStr   SHCTX "${BLAST_UNINST_KEY}" "DisplayName" "NCBI BLAST BLAST_VERSION+"
  WriteRegStr   SHCTX "${BLAST_UNINST_KEY}" "DisplayVersion" "BLAST_VERSION"
  WriteRegStr   SHCTX "${BLAST_UNINST_KEY}" "Publisher" "National Center for Biotechnology Information"
  WriteRegStr   SHCTX "${BLAST_UNINST_KEY}" "UninstallString" '"$INSTDIR\${BLAST_UNINSTALLER}"'
  WriteRegStr   SHCTX "${BLAST_UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\${BLAST_UNINSTALLER}" /S'
  WriteRegStr   SHCTX "${BLAST_UNINST_KEY}" "InstallLocation" $INSTDIR
  WriteRegStr   SHCTX "${BLAST_UNINST_KEY}" "DisplayIcon" "$INSTDIR\${BLAST_UNINSTALLER}"
  WriteRegDWORD SHCTX "${BLAST_UNINST_KEY}" "NoModify" 1
  WriteRegDWORD SHCTX "${BLAST_UNINST_KEY}" "NoRepair" 1
  SectionGetSize ${SecDflt} $0
  WriteRegDWORD SHCTX "${BLAST_UNINST_KEY}" "EstimatedSize" $0

  ;Update PATH
  Call AddToPath

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  ${If} $install_mode == "all"
    SetShellVarContext all
    StrCpy $R9 "HKLM"
  ${Else}
    SetShellVarContext current
    StrCpy $R9 "HKCU"
  ${EndIf}

  Delete "$INSTDIR\${BLAST_UNINSTALLER}"
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

  DeleteRegKey SHCTX "${BLAST_REGKEY}"
  DeleteRegKey SHCTX "${BLAST_UNINST_KEY}"

  ; Remove installation directory from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "$R9" "$INSTDIR\bin"

SectionEnd

;--------------------------------
; Elevation detection (installer and uninstaller)

!macro Func_DetectElevation un
  Function ${un}DetectElevation
    Push $0
    Push $1
    Push $2
    Push $3

    StrCpy $is_elevated 0
    StrCpy $elevation_type 1

    ; "Admin" is returned only for an elevated token (or when UAC is off).
    UserInfo::GetAccountType
    Pop $0
    ${If} $0 == "Admin"
      StrCpy $is_elevated 1
    ${EndIf}

    ; TokenElevationType (18): 3 = admin running with a filtered token, so
    ; Windows will show only a consent prompt.
    System::Call 'advapi32::OpenProcessToken(p -1, i 0x8, *p .r1) i .r0'
    ${If} $0 <> 0
      System::Call 'advapi32::GetTokenInformation(p r1, i 18, *i .r2, i 4, *i .r3) i .r0'
      ${If} $0 <> 0
        StrCpy $elevation_type $2
        ${If} $2 = 2
          StrCpy $is_elevated 1
        ${EndIf}
      ${EndIf}
      System::Call 'kernel32::CloseHandle(p r1)'
    ${EndIf}

    Pop $3
    Pop $2
    Pop $1
    Pop $0
  FunctionEnd
!macroend
!insertmacro Func_DetectElevation ""
!insertmacro Func_DetectElevation "un."

;--------------------------------
; Initialization function: read the command line, find a previous
; install, and choose the install mode and folder.
Function .onInit
  ; Usage-data sharing is ON by default; /SHARE=0 turns it off.
  StrCpy $share_data_opt_in 1
  StrCpy $elevated_run 0
  StrCpy $install_mode ""

  ${If} ${RunningX64}
    SetRegView 64
    StrCpy $sys_dir "$PROGRAMFILES64\NCBI\${BLAST_DIRNAME}"
  ${Else}
    StrCpy $sys_dir "$PROGRAMFILES\NCBI\${BLAST_DIRNAME}"
  ${EndIf}
  StrCpy $user_dir "$PROFILE\${BLAST_DIRNAME}"

  Call DetectElevation

  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/ALLUSERS" $R1
  ${IfNot} ${Errors}
    StrCpy $install_mode "all"
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/CURRENTUSER" $R1
  ${IfNot} ${Errors}
    StrCpy $install_mode "user"
  ${EndIf}
  ClearErrors
  ; /SHARE=1 -> share, /SHARE=0 -> do not share; other values keep the default.
  ${GetOptions} $R0 "/SHARE=" $R1
  ${IfNot} ${Errors}
    ${If} $R1 == "0"
      StrCpy $share_data_opt_in 0
    ${ElseIf} $R1 == "1"
      StrCpy $share_data_opt_in 1
    ${EndIf}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/ELEVATED" $R1
  ${IfNot} ${Errors}
  ${AndIf} $is_elevated == 1
    StrCpy $elevated_run 1
  ${EndIf}

  ; Previous install of this version: preselect its mode and folder.
  StrCpy $R3 ""
  ReadRegStr $R2 HKLM "${BLAST_REGKEY}" ""
  ${If} $R2 != ""
    StrCpy $sys_dir $R2
    StrCpy $R3 "all"
  ${EndIf}
  ReadRegStr $R2 HKCU "${BLAST_REGKEY}" ""
  ${If} $R2 != ""
    ReadRegStr $R4 HKCU "${BLAST_REGKEY}" "InstallMode"
    ${If} $R4 == "user"
      StrCpy $user_dir $R2
      StrCpy $R3 "user"
    ${ElseIf} $R3 == ""
      ; v1 installer: always recorded HKCU, normally for Program Files
      StrCpy $sys_dir $R2
      StrCpy $R3 "all"
    ${EndIf}
  ${EndIf}
  ClearErrors

  ${If} $install_mode == ""
    ${If} $R3 != ""
      StrCpy $install_mode $R3
    ${ElseIf} ${Silent}
    ${AndIf} $is_elevated != 1
      StrCpy $install_mode "user"
    ${Else}
      StrCpy $install_mode "all"
    ${EndIf}
  ${EndIf}

  ; NSIS drops a /D= value it rejects (relative path, drive root) and
  ; removes it from $CMDLINE. Recover it from the raw command line so it
  ; gets reported instead of the default folder being used without notice.
  ${If} $INSTDIR == ""
    System::Call 'kernel32::GetCommandLine() t .r0'
    StrLen $1 $0
    ${DoWhile} $1 > 0
      IntOp $1 $1 - 1
      StrCpy $2 $0 4 $1
      ${If} $2 == " /D="
        IntOp $1 $1 + 4
        StrCpy $INSTDIR $0 "" $1
        ${If} $INSTDIR == ""
          StrCpy $INSTDIR " "   ; empty /D=: let validation report it
        ${EndIf}
        ${ExitDo}
      ${EndIf}
    ${Loop}
  ${EndIf}

  ; $INSTDIR is non-empty only when /D= was given.
  ${If} $INSTDIR != ""
    ${If} $install_mode == "all"
      StrCpy $sys_dir $INSTDIR
    ${Else}
      StrCpy $user_dir $INSTDIR
    ${EndIf}
  ${ElseIf} $install_mode == "all"
    StrCpy $INSTDIR $sys_dir
  ${Else}
    StrCpy $INSTDIR $user_dir
  ${EndIf}

  ${If} ${Silent}
    ${If} $install_mode == "all"
    ${AndIf} $is_elevated != 1
      SetErrorLevel ${ERROR_ELEVATION_REQUIRED}
      Quit
    ${EndIf}
    StrCpy $check_path $INSTDIR
    Call ValidateInstallPath
    ${If} $check_msg != ""
      SetErrorLevel 2
      Quit
    ${EndIf}
    StrCpy $INSTDIR $check_path
    Call CheckWriteAccess
    ${If} $check_ok != 1
      SetErrorLevel 5
      Quit
    ${EndIf}
  ${EndIf}
FunctionEnd

; Pre callback for pages that the elevated copy skips (already answered
; in the first, non-elevated copy).
Function fnc_skip_if_elevated
  ${If} $elevated_run == 1
    Abort
  ${EndIf}
FunctionEnd

;--------------------------------
; Data usage page

Function fnc_data_usage_page_Create
  nsDialogs::Create ${NSIS_CUSTOM_PAGE_LAYOUT}
  Pop $hCtl_custom_page_v1
  ${If} $hCtl_custom_page_v1 == error
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "Current settings" "Review usage sharing preferences before installing NCBI BLAST package"

  ${NSD_CreateLabel} 8u 8u 277u 12u "Current settings:"
  Pop $0

  ; Initial state comes from $share_data_opt_in: checked by default, or
  ; whatever /SHARE= (or an earlier visit to this page) set.
  ${NSD_CreateCheckbox} 8u 24u 277u 12u "Share usage data with NCBI"
  Pop $hCtl_share_data_chk
  ${If} $share_data_opt_in == 1
    ${NSD_Check} $hCtl_share_data_chk
  ${Else}
    ${NSD_Uncheck} $hCtl_share_data_chk
  ${EndIf}

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
        ; ${StrRep} "$result_var" "String" "SubString" "RepString"
        ; Add '\r' to fix Unix style ending.
        ${StrRep} $R3 $R1 "$\n" "$\r$\n"
	; NEW: Append new text to a TextBox in control
	SendMessage $hCtl_custom_page_v1_TextBox1 ${EM_SETSEL} -1 -1
	SendMessage $hCtl_custom_page_v1_TextBox1 ${EM_REPLACESEL} 0, "STR:$R3"
  ${Loop}

  ; Close file
  FileClose $R2

  SendMessage $hCtl_custom_page_v1_TextBox1 ${WM_VSCROLL} ${SCROLL_CONTENT_TOP} 0
FunctionEnd

Function fnc_data_usage_page_Show
  ${If} $elevated_run == 1
    Abort
  ${EndIf}
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

;--------------------------------
; Choose Install Location page

Function fnc_location_page_Show
  ${If} $elevated_run == 1
    Abort
  ${EndIf}

  nsDialogs::Create ${NSIS_CUSTOM_PAGE_LAYOUT}
  Pop $hCtl_loc_page
  ${If} $hCtl_loc_page == error
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "Choose Install Location" "Choose who can use NCBI BLAST and the folder in which to install it."

  ${NSD_CreateLabel} 0u 0u 100% 12u "Install NCBI BLAST for:"
  Pop $0

  ${NSD_CreateRadioButton} 8u 13u 290u 12u "All users of this computer (requires administrator rights)"
  Pop $hCtl_loc_radio_all
  ${NSD_OnClick} $hCtl_loc_radio_all fnc_location_mode_changed

  ${NSD_CreateRadioButton} 8u 27u 290u 12u "Only for me (no administrator rights needed)"
  Pop $hCtl_loc_radio_user
  ${NSD_OnClick} $hCtl_loc_radio_user fnc_location_mode_changed

  ${NSD_CreateLabel} 0u 48u 100% 12u "Destination folder:"
  Pop $0

  ${NSD_CreateDirRequest} 0u 61u 236u 13u "$INSTDIR"
  Pop $hCtl_loc_dir
  ${NSD_OnChange} $hCtl_loc_dir fnc_location_dir_changed

  ${NSD_CreateBrowseButton} 240u 60u 60u 15u "B&rowse..."
  Pop $hCtl_loc_browse
  ${NSD_OnClick} $hCtl_loc_browse fnc_location_browse

  ${NSD_CreateLabel} 0u 82u 100% 30u ""
  Pop $hCtl_loc_hint

  ${NSD_CreateLabel} 0u 118u 100% 12u ""
  Pop $hCtl_loc_space

  ${If} $install_mode == "all"
    ${NSD_Check} $hCtl_loc_radio_all
  ${Else}
    ${NSD_Check} $hCtl_loc_radio_user
  ${EndIf}
  Call fnc_location_update_hint
  Call fnc_location_update_space

  nsDialogs::Show
FunctionEnd

Function fnc_location_update_hint
  ${If} $install_mode == "all"
    ${If} $is_elevated == 1
      ${NSD_SetText} $hCtl_loc_hint "Setup is running with administrator rights. NCBI BLAST will be available to all users of this computer."
    ${ElseIf} $elevation_type = 3
      ${NSD_SetText} $hCtl_loc_hint "NCBI BLAST will be available to all users of this computer. Windows will ask you to confirm administrator permission."
    ${Else}
      ${NSD_SetText} $hCtl_loc_hint "NCBI BLAST will be available to all users of this computer. Windows will ask for an administrator user name and password. If you don't have them, choose 'Only for me'."
    ${EndIf}
  ${Else}
    ${NSD_SetText} $hCtl_loc_hint "No administrator rights are needed. NCBI BLAST will be available only to your Windows account."
  ${EndIf}
FunctionEnd

Function fnc_location_update_space
  ${NSD_GetText} $hCtl_loc_dir $check_path
  Call GetSpaceInfo
  ${If} $space_free == ""
    ${NSD_SetText} $hCtl_loc_space "Space required: $space_req MB"
  ${Else}
    ${NSD_SetText} $hCtl_loc_space "Space required: $space_req MB        Space available: $space_free MB"
  ${EndIf}
FunctionEnd

Function fnc_location_dir_changed
  Pop $0
  Call fnc_location_update_space
FunctionEnd

; Each mode keeps its own folder, so switching back restores the user's edits.
Function fnc_location_mode_changed
  Pop $0
  ${NSD_GetText} $hCtl_loc_dir $1
  ${If} $install_mode == "all"
    StrCpy $sys_dir $1
  ${Else}
    StrCpy $user_dir $1
  ${EndIf}

  ${NSD_GetState} $hCtl_loc_radio_all $2
  ${If} $2 == ${BST_CHECKED}
    StrCpy $install_mode "all"
    ${NSD_SetText} $hCtl_loc_dir $sys_dir
  ${Else}
    StrCpy $install_mode "user"
    ${NSD_SetText} $hCtl_loc_dir $user_dir
  ${EndIf}
  Call fnc_location_update_hint
  Call fnc_location_update_space
FunctionEnd

Function fnc_location_browse
  Pop $0
  ${NSD_GetText} $hCtl_loc_dir $1
  nsDialogs::SelectFolderDialog "Select the folder to install NCBI BLAST in:" "$1"
  Pop $2
  ${If} $2 != error
  ${AndIf} $2 != ""
    ; Same as the stock directory page: add the product folder name
    ; unless the user already picked it.
    ${GetFileName} $2 $3
    ${If} $3 != "${BLAST_DIRNAME}"
      StrCpy $4 $2 1 -1
      ${If} $4 == "\"
        StrCpy $2 "$2${BLAST_DIRNAME}"
      ${Else}
        StrCpy $2 "$2\${BLAST_DIRNAME}"
      ${EndIf}
    ${EndIf}
    ${NSD_SetText} $hCtl_loc_dir $2
  ${EndIf}
FunctionEnd

Function fnc_location_page_Leave
  ${NSD_GetText} $hCtl_loc_dir $check_path
  Call ValidateInstallPath
  ${If} $check_msg != ""
    MessageBox MB_OK|MB_ICONEXCLAMATION "$check_msg"
    Abort
  ${EndIf}
  StrCpy $INSTDIR $check_path
  ${NSD_SetText} $hCtl_loc_dir $INSTDIR
  ${If} $install_mode == "all"
    StrCpy $sys_dir $INSTDIR
  ${Else}
    StrCpy $user_dir $INSTDIR
  ${EndIf}

  ; Disk space (skipped when the drive can't be queried).
  Call GetSpaceInfo
  ${If} $space_free != ""
  ${AndIf} $space_free < $space_req
    MessageBox MB_OK|MB_ICONEXCLAMATION "There is not enough free space on the drive for:$\r$\n$\r$\n$INSTDIR$\r$\n$\r$\nRequired: $space_req MB, available: $space_free MB. Please choose another folder."
    Abort
  ${EndIf}

  ; All users without elevation: the elevated copy takes over from here.
  ${If} $install_mode == "all"
  ${AndIf} $is_elevated != 1
    Call fnc_relaunch_elevated
    Abort
  ${EndIf}

  StrCpy $check_path $INSTDIR
  Call CheckWriteAccess
  ${If} $check_ok == 1
    Return
  ${EndIf}

  ${If} $install_mode == "user"
    ${If} ${Cmd} `MessageBox MB_YESNO|MB_ICONEXCLAMATION "You do not have permission to create or change files in:$\r$\n$\r$\n$INSTDIR$\r$\n$\r$\nClick Yes to choose another folder (the default personal folder is filled in), or No to exit Setup." IDNO`
      Quit
    ${EndIf}
    StrCpy $user_dir "$PROFILE\${BLAST_DIRNAME}"
    ${If} $INSTDIR != $user_dir
      ${NSD_SetText} $hCtl_loc_dir $user_dir
    ${EndIf}
  ${Else}
    ${If} ${Cmd} `MessageBox MB_YESNO|MB_ICONEXCLAMATION "Setup cannot create or change files in:$\r$\n$\r$\n$INSTDIR$\r$\n$\r$\nThe folder may be read-only or protected. Click Yes to choose another folder, or No to exit Setup." IDNO`
      Quit
    ${EndIf}
  ${EndIf}
  Abort
FunctionEnd

; Ask for elevation and start the installer again as administrator. Quits on
; success. Returns if the user stays on the location page.
Function fnc_relaunch_elevated
  ; Drive letters mapped in this session are usually not visible to the
  ; elevated process.
  ${GetRoot} $INSTDIR $0
  StrCpy $1 $0 1 1
  ${If} $1 == ":"
    System::Call 'kernel32::GetDriveType(t "$0\") i .r2'
    ${If} $2 = ${DRIVE_REMOTE}
      ${If} ${Cmd} `MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "$0 is a network drive. Drive letters mapped for your account are often not available after Windows asks for administrator permission.$\r$\n$\r$\nUse a local folder or a \\server\share path instead. Click OK to try anyway, or Cancel to go back." IDCANCEL`
        Return
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ${If} $elevation_type = 3
    StrCpy $0 "Installing for all users needs administrator permission.$\r$\n$\r$\nWindows will ask you to confirm. Click OK to continue."
  ${Else}
    StrCpy $0 "Installing for all users needs administrator rights.$\r$\n$\r$\nWindows will ask for an administrator's user name and password. If you don't have them, click Cancel and choose 'Only for me'."
  ${EndIf}
  ${If} ${Cmd} `MessageBox MB_OKCANCEL|MB_ICONINFORMATION "$0" IDCANCEL`
    Return
  ${EndIf}

  ; The user's choice from the data usage page is passed explicitly as
  ; /SHARE=0 or /SHARE=1 so the elevated copy (which skips that page) agrees.
  ; /D= must be last and unquoted.
  ClearErrors
  ExecShell "runas" "$EXEPATH" "/ALLUSERS /ELEVATED /SHARE=$share_data_opt_in /D=$INSTDIR"
  ${If} ${Errors}
    ${If} ${Cmd} `MessageBox MB_YESNO|MB_ICONQUESTION "Administrator permission was not granted.$\r$\n$\r$\nInstall NCBI BLAST for your account only instead?$\r$\n(Folder: $user_dir)" IDYES`
      ${NSD_Uncheck} $hCtl_loc_radio_all
      ${NSD_Check} $hCtl_loc_radio_user
      StrCpy $install_mode "user"
      ${NSD_SetText} $hCtl_loc_dir $user_dir
      Call fnc_location_update_hint
      Call fnc_location_update_space
    ${EndIf}
    Return
  ${EndIf}
  Quit
FunctionEnd

;--------------------------------
; Path / access helpers

; in: $check_path   out: $check_path (normalized), $check_msg ("" if OK)
Function ValidateInstallPath
  Push $0
  Push $1
  Push $2
  Push $3
  Push $4

  StrCpy $check_msg ""
  StrCpy $0 $check_path

  ; Drop trailing backslashes (keep "C:\").
  ${Do}
    StrLen $1 $0
    ${If} $1 <= 3
      ${ExitDo}
    ${EndIf}
    StrCpy $2 $0 1 -1
    ${If} $2 != "\"
      ${ExitDo}
    ${EndIf}
    StrCpy $0 $0 -1
  ${Loop}
  StrCpy $check_path $0

  StrLen $1 $0
  StrCpy $2 $0 2
  StrCpy $3 $0 2 1
  ${If} $0 == ""
    StrCpy $check_msg "Please enter a destination folder."
  ${ElseIf} $2 != "\\"
  ${AndIf} $3 != ":\"
    StrCpy $check_msg "Please enter a full folder path, for example:$\r$\n$\r$\n$PROFILE\${BLAST_DIRNAME}"
  ${ElseIf} $1 > 200
    StrCpy $check_msg "The folder path is too long (more than 200 characters). Please choose a shorter path."
  ${Else}
    StrCpy $4 0
    ${DoWhile} $4 < $1
      StrCpy $2 $0 1 $4
      ${If} $2 == "<"
      ${OrIf} $2 == ">"
      ${OrIf} $2 == '"'
      ${OrIf} $2 == "|"
      ${OrIf} $2 == "?"
      ${OrIf} $2 == "*"
      ${OrIf} $2 == "/"
        StrCpy $check_msg 'The folder name must not contain any of these characters:  < > " | ? * /'
        ${ExitDo}
      ${EndIf}
      ${If} $2 == ":"
      ${AndIf} $4 <> 1
        StrCpy $check_msg "The folder name must not contain ':' except after the drive letter."
        ${ExitDo}
      ${EndIf}
      IntOp $4 $4 + 1
    ${Loop}
  ${EndIf}

  ${If} $check_msg == ""
    ${GetRoot} $0 $2
    ${If} $0 == $2
    ${OrIf} $0 == "$2\"
      StrCpy $check_msg "Please choose a folder, not the root of a drive or share."
    ${ElseIf} ${FileExists} "$0"
    ${AndIfNot} ${FileExists} "$0\*.*"
      StrCpy $check_msg "A file with this name already exists:$\r$\n$\r$\n$0$\r$\n$\r$\nPlease choose a different folder."
    ${EndIf}
  ${EndIf}

  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd

; Tries to create the folder and a file in it, then removes anything the
; check created.   in: $check_path   out: $check_ok (1 / 0)
Function CheckWriteAccess
  Push $0
  Push $1
  Push $2

  StrCpy $check_ok 0

  ; Nearest folder that already exists.
  StrCpy $0 $check_path
  ${Do}
    ${If} $0 == ""
    ${OrIf} ${FileExists} "$0\*.*"
      ${ExitDo}
    ${EndIf}
    ${GetParent} $0 $0
  ${Loop}
  StrCpy $1 $0

  ClearErrors
  CreateDirectory "$check_path"
  ${IfNot} ${Errors}
    ClearErrors
    FileOpen $2 "$check_path\~blast_write_test.tmp" w
    ${IfNot} ${Errors}
      FileClose $2
      Delete "$check_path\~blast_write_test.tmp"
      StrCpy $check_ok 1
    ${EndIf}
  ${EndIf}

  ; Remove the folders created by this check (they are empty).
  StrCpy $0 $check_path
  ${Do}
    ${If} $0 == $1
    ${OrIf} $0 == ""
      ${ExitDo}
    ${EndIf}
    RMDir "$0"
    ${GetParent} $0 $0
  ${Loop}
  ClearErrors

  Pop $2
  Pop $1
  Pop $0
FunctionEnd

; in: $check_path   out: $space_req, $space_free (MB, "" if unknown)
Function GetSpaceInfo
  Push $0
  SectionGetSize ${SecDflt} $0
  IntOp $space_req $0 / 1024
  IntOp $space_req $space_req + 1
  StrCpy $space_free ""
  ${GetRoot} $check_path $0
  ${If} $0 != ""
    ClearErrors
    ${DriveSpace} "$0\" "/D=F /S=M" $space_free
    ${If} ${Errors}
      StrCpy $space_free ""
    ${EndIf}
  ${EndIf}
  ClearErrors
  Pop $0
FunctionEnd

; Prepend $INSTDIR\bin to the system (HKLM) or user (HKCU) PATH.
Function AddToPath
  ClearErrors
  ${If} $install_mode == "all"
    ${EnvVarUpdate} $0 "PATH" "P" "HKLM" "$INSTDIR\bin"
  ${Else}
    ReadRegStr $1 HKCU "Environment" "PATH"
    ClearErrors
    ${If} $1 == ""
      ; EnvVarUpdate refuses to edit an empty PATH; new accounts often
      ; have no user PATH value yet.
      WriteRegExpandStr HKCU "Environment" "PATH" "$INSTDIR\bin"
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
    ${Else}
      ${EnvVarUpdate} $0 "PATH" "P" "HKCU" "$INSTDIR\bin"
    ${EndIf}
  ${EndIf}
  ${If} ${Errors}
    DetailPrint "Could not add $INSTDIR\bin to PATH."
    ${IfNot} ${Silent}
      MessageBox MB_OK|MB_ICONINFORMATION "Setup could not add the BLAST folder to PATH automatically (the existing PATH may be too long).$\r$\n$\r$\nTo run BLAST from any command prompt, add this folder to PATH manually:$\r$\n$\r$\n$INSTDIR\bin"
    ${EndIf}
  ${EndIf}
FunctionEnd

;--------------------------------
; Summary page

Function fnc_summary_page_Create
  nsDialogs::Create ${NSIS_CUSTOM_PAGE_LAYOUT}
  Pop $hCtl_summary_page
  ${If} $hCtl_summary_page == error
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "Installation Summary" "Review installation details before continuing"

  ${NSD_CreateLabel} 8u 8u 277u 12u "Installation type"
  Pop $0
  ${If} $install_mode == "all"
    ${NSD_CreateLabel} 8u 22u 277u 14u "All users of this computer"
  ${Else}
    ${NSD_CreateLabel} 8u 22u 277u 14u "Only for the current user"
  ${EndIf}
  Pop $0

  ${NSD_CreateLabel} 8u 42u 277u 12u "Where software will be installed"
  Pop $0
  ${NSD_CreateLabel} 8u 56u 277u 14u "$INSTDIR"
  Pop $hCtl_summary_install_path

  ${NSD_CreateLabel} 8u 76u 277u 12u "Usage data setting"
  Pop $0

  ${If} $share_data_opt_in == 1
    ${NSD_CreateLabel} 8u 90u 277u 14u "Will share usage data with NCBI"
    Pop $hCtl_summary_opt_text
  ${Else}
    ${NSD_CreateLabel} 8u 90u 277u 14u "No usage information will be shared with NCBI"
    Pop $hCtl_summary_opt_text
  ${EndIf}

  ${NSD_CreateLabel} 8u 112u 277u 24u "Press CONTINUE to proceed with installation or ABORT to cancel."
  Pop $0
FunctionEnd

Function fnc_summary_page_Show
  Call fnc_summary_page_Create

  ; Present only ABORT / CONTINUE on this page. (Back stays hidden: in the
  ; elevated copy the earlier pages belong to the first copy.)
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
; Uninstaller initialization: an all-users install needs elevation to remove.
Function un.onInit
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  Call un.DetectElevation

  StrCpy $install_mode "user"
  ReadRegStr $R0 HKLM "${BLAST_REGKEY}" ""
  ${If} $R0 != ""
  ${AndIf} $R0 == $INSTDIR
    StrCpy $install_mode "all"
  ${EndIf}
  ClearErrors

  ${If} $install_mode == "all"
  ${AndIf} $is_elevated != 1
    ${If} ${Silent}
      SetErrorLevel ${ERROR_ELEVATION_REQUIRED}
      Quit
    ${EndIf}
    ; Guard against a relaunch loop if elevation silently didn't happen.
    ${GetParameters} $R1
    ClearErrors
    ${GetOptions} $R1 "/ELEVATED" $R2
    ${IfNot} ${Errors}
      MessageBox MB_OK|MB_ICONEXCLAMATION "Administrator rights are required to remove NCBI BLAST, because it was installed for all users."
      Quit
    ${EndIf}
    ; Relaunch the real uninstaller (this copy usually runs from %TEMP%).
    ClearErrors
    ExecShell "runas" "$INSTDIR\${BLAST_UNINSTALLER}" "/ELEVATED"
    ${If} ${Errors}
      MessageBox MB_OK|MB_ICONEXCLAMATION "Administrator permission is required to remove NCBI BLAST, because it was installed for all users."
    ${EndIf}
    Quit
  ${EndIf}
FunctionEnd
