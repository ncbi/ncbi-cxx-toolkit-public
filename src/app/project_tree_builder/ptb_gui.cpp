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
#include <app/project_tree_builder/proj_builder_app.hpp>
#include "ptb_gui.h"

BEGIN_NCBI_SCOPE

#if defined(NCBI_OS_MSWIN)

#define COMPILE_MULTIMON_STUBS
#include <multimon.h>

/////////////////////////////////////////////////////////////////////////////

void CenterWindow(HWND hWnd);
BOOL UpdateData(HWND hDlg, CProjBulderApp* pApp, BOOL bGet);
BOOL CALLBACK PtbConfigDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/////////////////////////////////////////////////////////////////////////////

void CenterWindow(HWND hWnd)
{
    RECT rcWnd;
	::GetWindowRect(hWnd, &rcWnd);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
	GetMonitorInfo( MonitorFromWindow( hWnd, MONITOR_DEFAULTTOPRIMARY), &mi);
	int xLeft = (mi.rcMonitor.left + mi.rcMonitor.right  - rcWnd.right  + rcWnd.left)/2;
	int yTop  = (mi.rcMonitor.top  + mi.rcMonitor.bottom - rcWnd.bottom + rcWnd.top )/2;
	::SetWindowPos(hWnd, NULL, xLeft, yTop, -1, -1,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL UpdateData(HWND hDlg, CProjBulderApp* pApp, BOOL bGet)
{
    if (bGet) {
        char szBuf[MAX_PATH];

        GetDlgItemText( hDlg,IDC_EDIT_ROOT,    szBuf, sizeof(szBuf)); pApp->m_Root      = szBuf;
        GetDlgItemText( hDlg,IDC_EDIT_SUBTREE, szBuf, sizeof(szBuf)); pApp->m_Subtree   = szBuf;
        GetDlgItemText( hDlg,IDC_EDIT_SLN,     szBuf, sizeof(szBuf)); pApp->m_Solution  = szBuf;
        GetDlgItemText( hDlg,IDC_EDIT_EXTROOT, szBuf, sizeof(szBuf)); pApp->m_BuildRoot = szBuf;
        GetDlgItemText( hDlg,IDC_EDIT_PROJTAG, szBuf, sizeof(szBuf)); pApp->m_ProjTags  = szBuf;

        pApp->m_Dll            = IsDlgButtonChecked(hDlg,IDC_CHECK_DLL)   == BST_CHECKED;
        pApp->m_BuildPtb       = IsDlgButtonChecked(hDlg,IDC_CHECK_NOPTB) != BST_CHECKED;
        pApp->m_AddMissingLibs = IsDlgButtonChecked(hDlg,IDC_CHECK_EXT)   == BST_CHECKED;
        pApp->m_ScanWholeTree  = IsDlgButtonChecked(hDlg,IDC_CHECK_NWT)   != BST_CHECKED;
    } else {

        SetDlgItemText( hDlg,IDC_EDIT_ROOT,    pApp->m_Root.c_str());
        SetDlgItemText( hDlg,IDC_EDIT_SUBTREE, pApp->m_Subtree.c_str());
        SetDlgItemText( hDlg,IDC_EDIT_SLN,     pApp->m_Solution.c_str());
        SetDlgItemText( hDlg,IDC_EDIT_EXTROOT, pApp->m_BuildRoot.c_str());
        SetDlgItemText( hDlg,IDC_EDIT_PROJTAG, pApp->m_ProjTags.c_str());

        CheckDlgButton( hDlg,IDC_CHECK_DLL,    pApp->m_Dll);
        CheckDlgButton( hDlg,IDC_CHECK_NOPTB, !pApp->m_BuildPtb);
        CheckDlgButton( hDlg,IDC_CHECK_EXT,    pApp->m_AddMissingLibs);
        CheckDlgButton( hDlg,IDC_CHECK_NWT,   !pApp->m_ScanWholeTree);
    }
    return TRUE;
}

BOOL CALLBACK PtbConfigDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_INITDIALOG:
        SetWindowLong( hDlg, DWL_USER, lParam );
        UpdateData( hDlg,(CProjBulderApp*)lParam,FALSE );
        CenterWindow(hDlg);
        SetFocus( GetDlgItem(hDlg,IDOK));
        break;
    case WM_COMMAND:
        switch (wParam) {
        case IDOK:
            if (UpdateData( hDlg,(CProjBulderApp*)GetWindowLong(hDlg,DWL_USER),TRUE )) {
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
    default:
        break;
    }
    return FALSE;
}

#endif

bool  CProjBulderApp::ConfirmConfiguration(void)
{
#if defined(NCBI_OS_MSWIN)
    return ( DialogBoxParam( GetModuleHandle(NULL),
                             MAKEINTRESOURCE(IDD_PTB_GUI_DIALOG),
                             NULL, PtbConfigDialog,
                             (LPARAM)(LPVOID)this) == IDOK);
#else
    return true;
#endif
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/09 17:01:46  gouriano
 * Initial revision
 *
 *
 * ===========================================================================
 */


