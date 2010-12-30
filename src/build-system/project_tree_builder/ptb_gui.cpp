/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Andrei Gourianov
 *
 */

#include <ncbi_pch.hpp>
#include "proj_builder_app.hpp"
#include "ptb_gui.h"

BEGIN_NCBI_SCOPE

#if defined(NCBI_OS_MSWIN)

#define COMPILE_MULTIMON_STUBS
#include <multimon.h>

/////////////////////////////////////////////////////////////////////////////

void CenterWindow(HWND hWnd);
BOOL UpdateData(HWND hDlg, CProjBulderApp* pApp, BOOL bGet);
INT_PTR CALLBACK PtbConfigDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/////////////////////////////////////////////////////////////////////////////

void CenterWindow(HWND hWnd)
{
    RECT rcWnd;
    GetWindowRect(hWnd, &rcWnd);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo( MonitorFromWindow( hWnd, MONITOR_DEFAULTTOPRIMARY), &mi);
    int xLeft = (mi.rcMonitor.left + mi.rcMonitor.right  - rcWnd.right  + rcWnd.left)/2;
    int yTop  = (mi.rcMonitor.top  + mi.rcMonitor.bottom - rcWnd.bottom + rcWnd.top )/2;
    SetWindowPos(hWnd, NULL, xLeft, yTop, -1, -1,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL UpdateData(HWND hDlg, CProjBulderApp* pApp, BOOL bGet)
{
    if (bGet) {
        char szBuf[MAX_PATH];

        GetDlgItemTextA( hDlg,IDC_EDIT_ROOT,    szBuf, sizeof(szBuf)); pApp->m_Root      = szBuf;
        GetDlgItemTextA( hDlg,IDC_EDIT_SUBTREE, szBuf, sizeof(szBuf)); pApp->m_Subtree   = szBuf;
        GetDlgItemTextA( hDlg,IDC_EDIT_SLN,     szBuf, sizeof(szBuf)); pApp->m_Solution  = szBuf;
        GetDlgItemTextA( hDlg,IDC_EDIT_EXTROOT, szBuf, sizeof(szBuf)); pApp->m_BuildRoot = szBuf;
        GetDlgItemTextA( hDlg,IDC_EDIT_PROJTAG, szBuf, sizeof(szBuf)); pApp->m_ProjTags  = szBuf;

        GetDlgItemTextA( hDlg,IDC_EDIT_3PARTY, szBuf, sizeof(szBuf));
        pApp->m_CustomConfiguration.AddDefinition("ThirdPartyBasePath", szBuf);
        GetDlgItemTextA( hDlg,IDC_EDIT_CNCBI,  szBuf, sizeof(szBuf));
        pApp->m_CustomConfiguration.AddDefinition("ThirdParty_C_ncbi", szBuf);

        pApp->m_Dll            = IsDlgButtonChecked(hDlg,IDC_CHECK_DLL)   == BST_CHECKED;
        pApp->m_BuildPtb       = IsDlgButtonChecked(hDlg,IDC_CHECK_NOPTB) != BST_CHECKED;
        pApp->m_AddMissingLibs = IsDlgButtonChecked(hDlg,IDC_CHECK_EXT)   == BST_CHECKED;
        pApp->m_ScanWholeTree  = IsDlgButtonChecked(hDlg,IDC_CHECK_NWT)   != BST_CHECKED;
        pApp->m_TweakVTuneR    = IsDlgButtonChecked(hDlg,IDC_CHECK_VTUNER) == BST_CHECKED;
        pApp->m_TweakVTuneD    = IsDlgButtonChecked(hDlg,IDC_CHECK_VTUNED) == BST_CHECKED;
    } else {

        SetDlgItemTextA( hDlg,IDC_EDIT_ROOT,    pApp->m_Root.c_str());
        SetDlgItemTextA( hDlg,IDC_EDIT_SUBTREE, pApp->m_Subtree.c_str());
        SetDlgItemTextA( hDlg,IDC_EDIT_SLN,     pApp->m_Solution.c_str());
        SetDlgItemTextA( hDlg,IDC_EDIT_EXTROOT, pApp->m_BuildRoot.c_str());
        SetDlgItemTextA( hDlg,IDC_EDIT_PROJTAG, pApp->m_ProjTags.c_str());

        SetDlgItemTextA( hDlg,IDC_EDIT_PTBINI2, pApp->m_CustomConfFile.c_str());
        string v;
        if (pApp->m_CustomConfiguration.GetValue("ThirdPartyBasePath", v)) {
            SetDlgItemTextA( hDlg,IDC_EDIT_3PARTY, v.c_str());
        }
        if (pApp->m_CustomConfiguration.GetValue("ThirdParty_C_ncbi", v)) {
            SetDlgItemTextA( hDlg,IDC_EDIT_CNCBI, v.c_str());
        }

        CheckDlgButton( hDlg,IDC_CHECK_DLL,    pApp->m_Dll);
        CheckDlgButton( hDlg,IDC_CHECK_NOPTB, !pApp->m_BuildPtb);
        CheckDlgButton( hDlg,IDC_CHECK_EXT,    pApp->m_AddMissingLibs);
        CheckDlgButton( hDlg,IDC_CHECK_NWT,   !pApp->m_ScanWholeTree);
        CheckDlgButton( hDlg,IDC_CHECK_VTUNER,  pApp->m_TweakVTuneR);
        CheckDlgButton( hDlg,IDC_CHECK_VTUNED,  pApp->m_TweakVTuneD);
    }
    return TRUE;
}

INT_PTR CALLBACK PtbConfigDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_INITDIALOG:
        SetWindowLongPtr( hDlg, DWLP_USER, lParam );
        UpdateData( hDlg,(CProjBulderApp*)lParam,FALSE );
        CenterWindow(hDlg);
        SetFocus( GetDlgItem(hDlg,IDOK));
        break;
    case WM_COMMAND:
        switch (wParam) {
        case IDOK:
            if (UpdateData( hDlg,(CProjBulderApp*)GetWindowLongPtr(hDlg,DWLP_USER),TRUE )) {
                EndDialog(hDlg,IDOK);
            }
            break;
        case IDCANCEL:
            EndDialog(hDlg,IDCANCEL);
            break;
        default:
            break;
        }
        break;
/*
    case WM_NCHITTEST:
        SetWindowLong(hDlg, DWL_MSGRESULT, HTCAPTION);
        return TRUE;
*/
    default:
        break;
    }
    return FALSE;
}

#endif

bool  CProjBulderApp::ConfirmConfiguration(void)
{
#if defined(NCBI_OS_MSWIN)
    bool result = ( DialogBoxParam( GetModuleHandle(NULL),
                             MAKEINTRESOURCE(IDD_PTB_GUI_DIALOG),
                             NULL, PtbConfigDialog,
                             (LPARAM)(LPVOID)this) == IDOK);
    if (result) {
        m_CustomConfiguration.AddDefinition("__TweakVTuneR", m_TweakVTuneR ? "yes" : "no");
        m_CustomConfiguration.AddDefinition("__TweakVTuneD", m_TweakVTuneD ? "yes" : "no");
        m_CustomConfiguration.Save(m_CustomConfFile);
    }
    return result;
#else
    return true;
#endif
}

bool  CProjBulderApp::Gui_ConfirmConfiguration(void)
{
    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
        return true;
    }

    cout << "*PTBGUI{* custom" << endl;
    list<string> skip;
    skip.push_back("__AllowedProjects");
    m_CustomConfiguration.Dump(cout, &skip);
    cout << "*PTBGUI}* custom" << endl;
    
    bool started = false;
    char buf[512];
    for (;;) {
        cin.getline(buf, sizeof(buf));
        string s(buf);
        if (NStr::StartsWith(s, "*PTBGUI{*")) {
            started = true;
            continue;
        }
        if (NStr::StartsWith(s, "*PTBGUIabort*")) {
            return false;
        }
        if (NStr::StartsWith(s, "*PTBGUI}*")) {
            started = false;
            break;;
        }
        if (started) {
            string s1, s2;
            if (NStr::SplitInTwo(s,"=", s1, s2)) {
                NStr::TruncateSpacesInPlace(s1);
                NStr::TruncateSpacesInPlace(s2);
                m_CustomConfiguration.AddDefinition(s1, s2);
            }
        }
    }
//    m_CustomConfiguration.Save(m_CustomConfFile);
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        string v;
        if (m_CustomConfiguration.GetValue("__TweakVTuneR", v)) {
            m_TweakVTuneR = NStr::StringToBool(v);
        }
        if (m_CustomConfiguration.GetValue("__TweakVTuneD", v)) {
            m_TweakVTuneD = NStr::StringToBool(v);
        }
        m_AddUnicode = GetSite().IsProvided("Ncbi-Unicode", false);
        if ( m_MsvcRegSettings.get() ) {
            GetBuildConfigs(&m_MsvcRegSettings->m_ConfigInfo);
        }
    }
    return true;
}

bool CProjBulderApp::Gui_ConfirmProjects(CProjectItemsTree& projects_tree)
{
    string prjid;
    cout << "*PTBGUI{* projects" << endl;
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
/*
        if (p->second.m_MakeType == eMakeType_Excluded ||
            p->second.m_MakeType == eMakeType_ExcludedByReq) {
            continue;
        }
*/
        if (p->first.Type() == CProjKey::eDll ||
            p->first.Type() == CProjKey::eDataSpec) {
            continue;
        }
        prjid = CreateProjectName(p->first);
        cout << prjid << ",";
        if (p->first.Type() == CProjKey::eLib ||
            p->first.Type() == CProjKey::eDll) {
            cout << "lib";
        } else if (p->first.Type() == CProjKey::eApp) {
            cout << "app";
        } else {
            cout << "other";
        }
        cout << ",";

        if (m_CustomConfiguration.DoesValueContain( "__AllowedProjects", prjid)) {
            cout << "select";
        } else {
            cout << "unselect";
        }
        cout << "," << NStr::Replace(p->second.m_SourcesBaseDir,"\\","/");
        cout << "," << NStr::Join(p->second.m_ProjTags,"/");
        cout << endl;
    }
    cout << "*PTBGUI}* projects" << endl;

    bool started = false;
    char buf[512];
    list<string> prj;
    for (;;) {
        cin.getline(buf, sizeof(buf));
        string s(buf);
        if (NStr::StartsWith(s, "*PTBGUI{*")) {
            started = true;
            continue;
        }
        if (NStr::StartsWith(s, "*PTBGUIabort*")) {
            return false;
        }
        if (NStr::StartsWith(s, "*PTBGUI}*")) {
            started = false;
            break;;
        }
        if (started) {
            NStr::TruncateSpacesInPlace(s);
            prj.push_back(s);
        }
    }
    m_CustomConfiguration.AddDefinition("__AllowedProjects", NStr::Join(prj," "));
    m_CustomConfiguration.Save(m_CustomConfFile);
    return true;
}


END_NCBI_SCOPE
