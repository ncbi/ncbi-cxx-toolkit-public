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
*      implementation of GUI part of main sequence/alignment viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.2  2001/03/02 03:26:59  thiessen
* fix dangling pointer upon app close
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <wx/wx.h>

#include "cn3d/sequence_viewer_window.hpp"
#include "cn3d/sequence_Viewer.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/show_hide_dialog.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(SequenceViewerWindow, wxFrame)
    INCLUDE_VIEWER_WINDOW_BASE_EVENTS
    EVT_CLOSE     (                                     SequenceViewerWindow::OnCloseWindow)
    EVT_MENU      (MID_SHOW_HIDE_ROWS,                  SequenceViewerWindow::OnShowHideRows)
    EVT_MENU      (MID_DELETE_ROW,                      SequenceViewerWindow::OnDeleteRow)
    EVT_MENU      (MID_MOVE_ROW,                        SequenceViewerWindow::OnMoveRow)
END_EVENT_TABLE()

SequenceViewerWindow::SequenceViewerWindow(SequenceViewer *parentSequenceViewer) :
    ViewerWindowBase(parentSequenceViewer), sequenceViewer(parentSequenceViewer)
{
    viewMenu->Append(MID_SHOW_HIDE_ROWS, "Show/Hide &Rows");

    editMenu->AppendSeparator();
    editMenu->Append(MID_DELETE_ROW, "Delete &Row", "", true);

    mouseModeMenu->Append(MID_MOVE_ROW, "&Move Row", "", true);

    EnableDerivedEditorMenuItems(false);
}

SequenceViewerWindow::~SequenceViewerWindow(void)
{
}

void SequenceViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    if (viewer) {
        if (!SaveDialog(event.CanVeto())) {
            event.Veto();       // cancelled
            return;
        }
        viewer->GetCurrentDisplay()->RemoveBlockBoundaryRow();
        viewer->GUIDestroyed(); // make sure SequenceViewer knows the GUI is gone
        GlobalMessenger()->UnPostRedrawSequenceViewer(viewer);  // don't try to redraw after destroyed!
    }
    Destroy();
}

void SequenceViewerWindow::EnableDerivedEditorMenuItems(bool enabled)
{
    if (menuBar->FindItem(MID_SHOW_HIDE_ROWS)) {
        if (sequenceViewer->GetCurrentDisplay() &&
            sequenceViewer->GetCurrentDisplay()->IsEditable())
            menuBar->Enable(MID_SHOW_HIDE_ROWS, !enabled);  // can't show/hide when editor is on
        else
            menuBar->Enable(MID_SHOW_HIDE_ROWS, false);     // can't show/hide in non-alignment display
        menuBar->Enable(MID_DELETE_ROW, enabled);           // can only delete row when editor is on
        menuBar->Enable(MID_MOVE_ROW, enabled);             // can only move row when editor is on
    }
}

void SequenceViewerWindow::OnDeleteRow(wxCommandEvent& event)
{
    OnEditMenu(event);  // special case: handled by base class
}

void SequenceViewerWindow::OnMoveRow(wxCommandEvent& event)
{
    OnMouseMode(event); // set checks via base class
    viewerWidget->SetMouseMode(SequenceViewerWidget::eDragVertical);
}

bool SequenceViewerWindow::RequestEditorEnable(bool enable)
{
    // turn on editor
    if (enable) {
        return QueryShowAllRows();
    }

    // turn off editor
    else {
        return SaveDialog(true);
    }
}

bool SequenceViewerWindow::SaveDialog(bool canCancel)
{
    // if editor is checked on, then this save command was initiated outside the menu;
    // if so, then need to turn off editor pretending it was done from 'enable editor' menu item
    if (menuBar->IsChecked(MID_ENABLE_EDIT)) {
        Command(MID_ENABLE_EDIT);
        return true;
    }

    // quick & dirty check for whether save is necessary, by whether Undo is enabled
    if (!menuBar->IsEnabled(MID_UNDO)) {
        viewer->KeepOnlyStackTop();  // remove any unnecessary copy from stack
        return true;
    }

    int option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
    if (canCancel) option |= wxCANCEL;

    wxMessageDialog dialog(NULL, "Do you want to keep the changes to this alignment?", "", option);
    option = dialog.ShowModal();

    if (option == wxID_CANCEL) return false; // user cancelled this operation

    if (option == wxID_YES)
        sequenceViewer->SaveAlignment();    // save data
    else
        sequenceViewer->RevertAlignment();  // revert to original

    return true;
}

void SequenceViewerWindow::OnShowHideRows(wxCommandEvent& event)
{
    std::vector < const Sequence * > slaveSequences;
    sequenceViewer->alignmentManager->GetAlignmentSetSlaveSequences(&slaveSequences);
    wxString *titleStrs = new wxString[slaveSequences.size()];
    for (int i=0; i<slaveSequences.size(); i++)
        titleStrs[i] = slaveSequences[i]->GetTitle().c_str();

    std::vector < bool > visibilities;
    sequenceViewer->alignmentManager->GetAlignmentSetSlaveVisibilities(&visibilities);

    wxString title = "Show/Hide Slaves of ";
    title.Append(sequenceViewer->alignmentManager->GetCurrentMultipleAlignment()->GetMaster()->GetTitle().c_str());
    ShowHideDialog dialog(
        titleStrs,
        visibilities,
        sequenceViewer->alignmentManager,
        NULL, -1, title, wxPoint(400, 50), wxSize(200, 300));
    dialog.Activate();
//    delete titleStrs;    // apparently deleted by wxWindows
}

bool SequenceViewerWindow::QueryShowAllRows(void)
{
    std::vector < bool > visibilities;
    sequenceViewer->alignmentManager->GetAlignmentSetSlaveVisibilities(&visibilities);

    int i;
    for (i=0; i<visibilities.size(); i++) if (!visibilities[i]) break;
    if (i == visibilities.size()) return true;  // we're okay if all rows already visible

    // if some rows hidden, ask user whether to show all rows, or cancel
    wxMessageDialog query(NULL,
        "This operation requires all alignment rows to be visible. Do you wish to show all rows now?",
        "Query", wxOK | wxCANCEL | wxCENTRE | wxICON_QUESTION);

    if (query.ShowModal() == wxID_CANCEL) return false;   // user cancelled

    // show all rows
    for (i=0; i<visibilities.size(); i++) visibilities[i] = true;
    sequenceViewer->alignmentManager->SelectionCallback(visibilities);
    return true;
}

END_SCOPE(Cn3D)

