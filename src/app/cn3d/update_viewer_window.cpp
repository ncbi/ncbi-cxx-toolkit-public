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
* Revision 1.26  2001/11/27 16:26:10  thiessen
* major update to data management system
*
* Revision 1.25  2001/10/08 00:00:09  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.24  2001/09/27 15:38:00  thiessen
* decouple sequence import and BLAST
*
* Revision 1.23  2001/09/24 14:37:52  thiessen
* more wxPanel stuff - fix for new heirarchy in wx 2.3.2+
*
* Revision 1.22  2001/09/24 13:29:55  thiessen
* fix wxPanel issues
*
* Revision 1.21  2001/09/18 03:10:46  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.20  2001/08/24 18:53:43  thiessen
* add filename to sequence viewer window titles
*
* Revision 1.19  2001/08/06 20:22:01  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.18  2001/06/01 21:48:26  thiessen
* add terminal cutoff to threading
*
* Revision 1.17  2001/05/31 18:47:10  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.16  2001/05/24 21:38:41  thiessen
* fix threader options initial values
*
* Revision 1.15  2001/05/24 13:32:32  thiessen
* further tweaks for GTK
*
* Revision 1.14  2001/05/23 17:45:40  thiessen
* change dialog implementation to wxDesigner; interface changes
*
* Revision 1.13  2001/05/22 19:09:32  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.12  2001/05/17 18:34:26  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.11  2001/05/15 23:48:38  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.10  2001/05/15 14:57:56  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* Revision 1.9  2001/05/02 13:46:29  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.8  2001/04/19 12:58:32  thiessen
* allow merge and delete of individual updates
*
* Revision 1.7  2001/04/12 18:10:00  thiessen
* add block freezing
*
* Revision 1.6  2001/04/04 00:27:15  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.5  2001/03/28 23:02:17  thiessen
* first working full threading
*
* Revision 1.4  2001/03/22 00:33:18  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.3  2001/03/17 14:06:49  thiessen
* more workarounds for namespace/#define conflicts
*
* Revision 1.2  2001/03/13 01:25:06  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.1  2001/03/09 15:49:06  thiessen
* major changes to add initial update viewer
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>
#include <wx/stattext.h>

#include "cn3d/update_viewer_window.hpp"
#include "cn3d/update_viewer.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/cn3d_threader.hpp"
#include "cn3d/wx_tools.hpp"
#include "cn3d/cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(UpdateViewerWindow, wxFrame)
    INCLUDE_VIEWER_WINDOW_BASE_EVENTS
    EVT_CLOSE     (                                     UpdateViewerWindow::OnCloseWindow)
    EVT_MENU_RANGE(MID_THREAD_ONE, MID_THREAD_ALL,      UpdateViewerWindow::OnRunThreader)
    EVT_MENU_RANGE(MID_MERGE_ONE, MID_MERGE_ALL,        UpdateViewerWindow::OnMerge)
    EVT_MENU_RANGE(MID_DELETE_ONE, MID_DELETE_ALL,      UpdateViewerWindow::OnDelete)
    EVT_MENU      (MID_IMPORT,                          UpdateViewerWindow::OnImport)
    EVT_MENU      (MID_BLAST_ONE,                       UpdateViewerWindow::OnRunBlast)
END_EVENT_TABLE()

UpdateViewerWindow::UpdateViewerWindow(UpdateViewer *thisUpdateViewer) :
    ViewerWindowBase(thisUpdateViewer,
        wxString(GetWorkingFilename().c_str()) + " - Update Viewer",
        wxPoint(0,50), wxSize(1000,300)),
    updateViewer(thisUpdateViewer)
{
    // Edit menu
    editMenu->AppendSeparator();
    editMenu->Append(MID_IMPORT, "&Import Sequence");

    // Mouse mode menu
    menuBar->Enable(MID_SELECT_COLS, false);

    // Algorithms menu
    wxMenu *menu = new wxMenu;
    menu->Append(MID_THREAD_ONE, "Thread &Single", "", true);
    menu->Append(MID_THREAD_ALL, "Thread &All");
    menu->AppendSeparator();
    menu->Append(MID_BLAST_ONE, "&BLAST Single", "", true);
    menuBar->Append(menu, "Al&gorithms");

    // Alignments menu
    menu = new wxMenu;
    menu->Append(MID_MERGE_ONE, "&Merge Single", "", true);
    menu->Append(MID_MERGE_ALL, "Merge &All");
    menu->AppendSeparator();
    menu->Append(MID_DELETE_ONE, "&Delete Single", "", true);
    menu->Append(MID_DELETE_ALL, "Delete A&ll");
    menuBar->Append(menu, "&Alignments");

    // editor always on
    EnableBaseEditorMenuItems(true);
    menuBar->Check(MID_ENABLE_EDIT, true);
    menuBar->Enable(MID_ENABLE_EDIT, false);

    // set default mouse mode
    viewerWidget->SetMouseMode(SequenceViewerWidget::eDragHorizontal);
    menuBar->Check(MID_DRAG_HORIZ, true);
    menuBar->Check(MID_SELECT_RECT, false);
}

UpdateViewerWindow::~UpdateViewerWindow(void)
{
}

void UpdateViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    if (viewer) {
        if (!SaveDialog(event.CanVeto())) {
            event.Veto();       // cancelled
            return;
        }
        viewer->GUIDestroyed(); // make sure UpdateViewer knows the GUI is gone
        GlobalMessenger()->UnPostRedrawSequenceViewer(viewer);  // don't try to redraw after destroyed!
    }
    Destroy();
}

void UpdateViewerWindow::EnableDerivedEditorMenuItems(bool enabled)
{
}

void UpdateViewerWindow::OnRunThreader(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_THREAD_ONE:
            if (DoBlastSingle()) BlastSingleOff();
            if (DoMergeSingle()) MergeSingleOff();
            if (DoDeleteSingle()) DeleteSingleOff();
            CancelBaseSpecialModes();
            if (DoThreadSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                ThreadSingleOff();
            break;
        case MID_THREAD_ALL: {
            if (updateViewer->GetCurrentAlignments()->size() == 0) return;

            ThreaderOptions options;
            // base nRS estimate on first update...
            options.nRandomStarts = Threader::EstimateNRandomStarts(
                updateViewer->alignmentManager->GetCurrentMultipleAlignment(),
                updateViewer->GetCurrentAlignments()->front());
            ThreaderOptionsDialog optDialog(this, options);
            if (optDialog.ShowModal() == wxCANCEL) return;  // user cancelled

            if (!optDialog.GetValues(&options)) {
                ERR_POST(Error << "Error retrieving options values from dialog");
                return;
            }

            RaiseLogWindow();
            SetCursor(*wxHOURGLASS_CURSOR);
            updateViewer->alignmentManager->ThreadAllUpdates(options);
            SetCursor(wxNullCursor);
            break;
        }
    }
}

void UpdateViewerWindow::OnMerge(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_MERGE_ONE:
            if (DoThreadSingle()) ThreadSingleOff();
            if (DoBlastSingle()) BlastSingleOff();
            if (DoDeleteSingle()) DeleteSingleOff();
            CancelBaseSpecialModes();
            if (DoMergeSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                MergeSingleOff();
            break;
        case MID_MERGE_ALL:
        {
            AlignmentManager::UpdateMap all;    // construct map/list of all updates
            const ViewerBase::AlignmentList *currentUpdates = updateViewer->GetCurrentAlignments();
            ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates->end();
            for (u=currentUpdates->begin(); u!=ue; u++) all[*u] = true;
            UpdateViewerWindow::updateViewer->alignmentManager->MergeUpdates(all);
            break;
        }
    }
}

void UpdateViewerWindow::OnDelete(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_DELETE_ONE:
            if (DoThreadSingle()) ThreadSingleOff();
            if (DoBlastSingle()) BlastSingleOff();
            if (DoMergeSingle()) MergeSingleOff();
            CancelBaseSpecialModes();
            if (DoDeleteSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                DeleteSingleOff();
            break;
        case MID_DELETE_ALL: {
            ViewerBase::AlignmentList empty;
            updateViewer->ReplaceAlignments(empty);
            break;
        }
    }
}

bool UpdateViewerWindow::SaveDialog(bool canCancel)
{
    // quick & dirty check for whether save is necessary, by whether Undo is enabled
    if (!menuBar->IsEnabled(MID_UNDO)) return true;

    int option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
    if (canCancel) option |= wxCANCEL;

    wxMessageDialog dialog(NULL, "Do you want to keep the changes to these updates?", "", option);
    option = dialog.ShowModal();

    if (option == wxID_CANCEL) return false; // user cancelled this operation

    if (option == wxID_YES)
        updateViewer->SaveAlignments();     // save data
    else {
        updateViewer->RevertAlignment();    // revert to original
        updateViewer->PushAlignment();      // but keep undo stack active
    }

    return true;
}

void UpdateViewerWindow::OnImport(wxCommandEvent& event)
{
    if (event.GetId() == MID_IMPORT) updateViewer->ImportSequence();
}

void UpdateViewerWindow::OnRunBlast(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_BLAST_ONE:
            if (DoThreadSingle()) ThreadSingleOff();
            if (DoMergeSingle()) MergeSingleOff();
            if (DoDeleteSingle()) DeleteSingleOff();
            CancelBaseSpecialModes();
            if (DoBlastSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                BlastSingleOff();
            break;
    }
}


/////////////////////////////////////////////////////////////////////////////////
// ThreaderOptionsDialog implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(ThreaderOptionsDialog, wxDialog)
    EVT_BUTTON(-1,  ThreaderOptionsDialog::OnButton)
    EVT_CLOSE (     ThreaderOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()

ThreaderOptionsDialog::ThreaderOptionsDialog(wxWindow* parent, const ThreaderOptions& initialOptions) :
    wxDialog(parent, -1, "Set Threader Options", wxDefaultPosition, wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU) // not resizable
{
    // the following code is modified (heavily) from wxDesigner C++ output from threader_dialog.wdr
    wxPanel *panel = new wxPanel(this, -1);
    wxBoxSizer *item0 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *item1 = new wxBoxSizer(wxVERTICAL);
    wxStaticBox *item3 = new wxStaticBox(panel, -1, "Threader Options");
    wxStaticBoxSizer *item2 = new wxStaticBoxSizer(item3, wxHORIZONTAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(3, 5, 0);

    // PSSM weight
    wxStaticText *item5 = new wxStaticText(panel, -1, "Weighting of PSSM/Contact score? [0 .. 1], 1 = PSSM only", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item5, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    fpWeight = new FloatingPointSpinCtrl(panel,
        0.0, 1.0, 0.05, initialOptions.weightPSSM,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(fpWeight->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(fpWeight->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // loop lengths
    wxStaticText *item8 = new wxStaticText(panel, -1, "Loop length multiplier? [0.1 .. 10]", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item8, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    fpLoops = new FloatingPointSpinCtrl(panel,
        0.1, 10.0, 0.25, initialOptions.loopLengthMultiplier,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(fpLoops->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(fpLoops->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // random starts
    wxStaticText *item11 = new wxStaticText(panel, -1, "Number of random starts? [1 .. 100]", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item11, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    iStarts = new IntegerSpinCtrl(panel,
        1, 100, 5, initialOptions.nRandomStarts,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(iStarts->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(iStarts->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // # results
    wxStaticText *item14 = new wxStaticText(panel, -1, "Number of result alignments per row? [1 .. 20]", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item14, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    iResults = new IntegerSpinCtrl(panel,
        1, 20, 1, initialOptions.nResultAlignments,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(iResults->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(iResults->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // terminal residue cutoff
    wxStaticText *item16 = new wxStaticText(panel, -1, "Terminal residue cutoff? [-1..N], -1 = unrestricted", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item16, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    iCutoff = new IntegerSpinCtrl(panel,
        -1, 100000, 1, initialOptions.terminalResidueCutoff,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(iCutoff->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(iCutoff->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // merge results
    wxStaticText *item17 = new wxStaticText(panel, -1, "Merge results after each row is threaded?", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item17, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    bMerge = new wxCheckBox(panel, -1, "", wxDefaultPosition, wxDefaultSize, 0);
    bMerge->SetValue(initialOptions.mergeAfterEachSequence);
    grid->Add(bMerge, 0, wxALIGN_CENTRE|wxALL, 5);
    grid->Add(20, SPIN_CTRL_HEIGHT, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // freeze blocks
    wxStaticText *item19 = new wxStaticText(panel, -1, "Freeze isolated blocks?", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item19, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    bFreeze = new wxCheckBox(panel, -1, "", wxDefaultPosition, wxDefaultSize, 0);
    bFreeze->SetValue(initialOptions.freezeIsolatedBlocks);
    grid->Add(bFreeze, 0, wxALIGN_CENTRE|wxALL, 5);
    grid->Add(20, SPIN_CTRL_HEIGHT, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item2->Add(grid, 0, wxALIGN_CENTRE|wxALL, 5);
    item1->Add(item2, 0, wxALIGN_CENTRE|wxALL, 5);
    wxBoxSizer *item21 = new wxBoxSizer(wxHORIZONTAL);

    bOK = new wxButton(panel, -1, "OK", wxDefaultPosition, wxDefaultSize, 0);
    bOK->SetDefault();
    item21->Add(bOK, 0, wxALIGN_CENTRE|wxALL, 5);

    bCancel = new wxButton(panel, -1, "Cancel", wxDefaultPosition, wxDefaultSize, 0);
    item21->Add(bCancel, 0, wxALIGN_CENTRE|wxALL, 5);

    item1->Add(item21, 0, wxALIGN_CENTRE|wxALL, 5);
    item0->Add(item1, 0, wxALIGN_CENTRE|wxALL, 5);
    panel->SetAutoLayout(true);
    panel->SetSizer(item0);
    item0->Fit(this);
    item0->Fit(panel);
    item0->SetSizeHints(this);
}

bool ThreaderOptionsDialog::GetValues(ThreaderOptions *options)
{
    options->mergeAfterEachSequence = bMerge->GetValue();
    options->freezeIsolatedBlocks = bFreeze->GetValue();
    return (
        fpWeight->GetDouble(&options->weightPSSM) &&
        fpLoops->GetDouble(&options->loopLengthMultiplier) &&
        iStarts->GetInteger(&options->nRandomStarts) &&
        iResults->GetInteger(&options->nResultAlignments) &&
        iCutoff->GetInteger(&options->terminalResidueCutoff)
    );
}

void ThreaderOptionsDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void ThreaderOptionsDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetEventObject() == bOK) {
        ThreaderOptions dummy;
        if (GetValues(&dummy))  // can't successfully quit if values aren't valid
            EndModal(wxOK);
        else
            wxBell();
    } else if (event.GetEventObject() == bCancel) {
        EndModal(wxCANCEL);
    } else {
        event.Skip();
    }
}

END_SCOPE(Cn3D)
