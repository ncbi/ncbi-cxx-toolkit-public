/*  $Id$
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
* Authors:  Paul Thiessen
*
* File Description:
*      implementation of GUI part of update viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/03/09 15:49:06  thiessen
* major changes to add initial update viewer
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <wx/wx.h>

#include "cn3d/update_viewer_window.hpp"
#include "cn3d/update_viewer.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/sequence_display.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(UpdateViewerWindow, wxFrame)
    INCLUDE_VIEWER_WINDOW_BASE_EVENTS
    EVT_CLOSE     (                                     UpdateViewerWindow::OnCloseWindow)
END_EVENT_TABLE()

UpdateViewerWindow::UpdateViewerWindow(UpdateViewer *parentUpdateViewer) :
    ViewerWindowBase(parentUpdateViewer, "Cn3D++ Update Viewer", wxPoint(0,200), wxSize(1000,300)),
    updateViewer(parentUpdateViewer)
{
    menuBar->Enable(MID_SELECT_COLS, false);
    EnableDerivedEditorMenuItems(false);
}

UpdateViewerWindow::~UpdateViewerWindow(void)
{
}

void UpdateViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    if (viewer) {
        viewer->GetCurrentDisplay()->RemoveBlockBoundaryRows();
        viewer->GUIDestroyed(); // make sure UpdateViewer knows the GUI is gone
        GlobalMessenger()->UnPostRedrawSequenceViewer(viewer);  // don't try to redraw after destroyed!
    }
    Destroy();
}

void UpdateViewerWindow::EnableDerivedEditorMenuItems(bool enabled)
{
}

bool UpdateViewerWindow::RequestEditorEnable(bool enable)
{
    return true;
}

END_SCOPE(Cn3D)

