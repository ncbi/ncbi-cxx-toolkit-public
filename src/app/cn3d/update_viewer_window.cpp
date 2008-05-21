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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "remove_header_conflicts.hpp"

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/stattext.h>

#include "update_viewer_window.hpp"
#include "update_viewer.hpp"
#include "messenger.hpp"
#include "sequence_display.hpp"
#include "alignment_manager.hpp"
#include "cn3d_threader.hpp"
#include <algo/structure/wx_tools/wx_tools.hpp>
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_ba_interface.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(UpdateViewerWindow, wxFrame)
    INCLUDE_VIEWER_WINDOW_BASE_EVENTS
    EVT_CLOSE     (                                                     UpdateViewerWindow::OnCloseWindow)
    EVT_MENU_RANGE(MID_DELETE_ALL_BLOCKS, MID_DELETE_BLOCKS_ALL_ROWS,   UpdateViewerWindow::OnDelete)
    EVT_MENU_RANGE(MID_SORT_UPDATES_IDENTIFIER, MID_SORT_UPDATES_PSSM,  UpdateViewerWindow::OnSortUpdates)
    EVT_MENU_RANGE(MID_THREAD_ONE, MID_THREAD_ALL,                      UpdateViewerWindow::OnRunThreader)
    EVT_MENU_RANGE(MID_MERGE_ONE, MID_MERGE_ALL,                        UpdateViewerWindow::OnMerge)
    EVT_MENU_RANGE(MID_DELETE_ONE, MID_DELETE_ALL,                      UpdateViewerWindow::OnDelete)
    EVT_MENU_RANGE(MID_IMPORT_SEQUENCES, MID_IMPORT_STRUCTURE,          UpdateViewerWindow::OnImport)
    EVT_MENU_RANGE(MID_BLAST_ONE, MID_BLAST_NEIGHBOR,                   UpdateViewerWindow::OnRunBlast)
    EVT_MENU_RANGE(MID_SET_REGION, MID_RESET_REGIONS,                   UpdateViewerWindow::OnSetRegion)
    EVT_MENU_RANGE(MID_BLOCKALIGN_ONE, MID_BLOCKALIGN_ALL,              UpdateViewerWindow::OnBlockAlign)
    EVT_MENU_RANGE(MID_EXTEND_ONE, MID_EXTEND_ALL,                      UpdateViewerWindow::OnExtend)
END_EVENT_TABLE()

UpdateViewerWindow::UpdateViewerWindow(UpdateViewer *thisUpdateViewer) :
    ViewerWindowBase(thisUpdateViewer, wxPoint(0,50), wxSize(1000,300)),
    updateViewer(thisUpdateViewer)
{
    SetWindowTitle();

    // Edit menu
    editMenu->AppendSeparator();
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_SORT_UPDATES_IDENTIFIER, "By &Identifier");
    subMenu->Append(MID_SORT_UPDATES_PSSM, "By &PSSM Score");
    editMenu->Append(MID_SORT_UPDATES, "So&rt Imports...", subMenu);
    editMenu->AppendSeparator();
    editMenu->Append(MID_IMPORT_SEQUENCES, "&Import Sequences");
    editMenu->Append(MID_IMPORT_STRUCTURE, "Import S&tructure");
    // insert at hard-coded location, since FindItem doesn't seem to work...
    editMenu->Insert(8, MID_DELETE_ALL_BLOCKS, "Delete A&ll Blocks", "", true);
    editMenu->Insert(9, MID_DELETE_BLOCKS_ALL_ROWS, "Delete All Blocks in All Ali&gnments");

    // Mouse mode menu
    menuBar->Enable(MID_SELECT_COLS, false);
    menuBar->Enable(MID_SELECT_BLOCKS, false);

    // Algorithms menu
    wxMenu *menu = new wxMenu;
    menu->Append(MID_BLOCKALIGN_ONE, "B&lock Align Single", "", true);
    menu->Append(MID_BLOCKALIGN_ALL, "Bloc&k Align N");
    menu->AppendSeparator();
    menu->Append(MID_BLAST_ONE, "&BLAST Single", "", true);
    menu->Append(MID_BLAST_PSSM_ONE, "BLAST/&PSSM Single", "", true);
    menu->Append(MID_BLAST_NEIGHBOR, "BLAST &Neighbor", "", true);
    menu->AppendSeparator();
    menu->Append(MID_THREAD_ONE, "Thread &Single", "", true);
    menu->Append(MID_THREAD_ALL, "Thread &All");
    menu->AppendSeparator();
    menu->Append(MID_EXTEND_ONE, "&Extend Single", "", true);
    menu->Append(MID_EXTEND_ALL, "E&xtend All");
    menu->AppendSeparator();
    menu->Append(MID_SET_REGION, "Set &Region", "", true);
    menu->Append(MID_RESET_REGIONS, "Reset All Re&gions");
    menuBar->Append(menu, "Al&gorithms");

    // Alignments menu
    menu = new wxMenu;
    menu->Append(MID_MERGE_ONE, "&Merge Single", "", true);
    menu->Append(MID_MERGE_NEIGHBOR, "Merge to &Neighbor", "", true);
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

    SetMenuBar(menuBar);
}

UpdateViewerWindow::~UpdateViewerWindow(void)
{
}

void UpdateViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    if (viewer) {
        if (event.CanVeto()) {
            Show(false);    // just hide the window if we can
            event.Veto();
            return;
        }
        SaveDialog(true, false);
        viewer->GUIDestroyed(); // make sure UpdateViewer knows the GUI is gone
        GlobalMessenger()->UnPostRedrawSequenceViewer(viewer);  // don't try to redraw after destroyed!
    }
    Destroy();
}

void UpdateViewerWindow::SetWindowTitle(void)
{
    SetTitle(wxString(GetWorkingTitle().c_str()) + " - Import Viewer");
}

void UpdateViewerWindow::EnableDerivedEditorMenuItems(bool enabled)
{
}

void UpdateViewerWindow::OnRunThreader(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_THREAD_ONE:
            CancelAllSpecialModesExcept(MID_THREAD_ONE);
            if (DoThreadSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                ThreadSingleOff();
            break;
        case MID_THREAD_ALL: {
            if (!updateViewer->alignmentManager->GetCurrentMultipleAlignment()) {
                ERRORMSG("Can't run threader without existing core alignment");
                return;
            }
            if (updateViewer->GetCurrentAlignments().size() == 0) return;

            // base nRS estimate on first update...
            globalThreaderOptions.nRandomStarts = Threader::EstimateNRandomStarts(
                updateViewer->alignmentManager->GetCurrentMultipleAlignment(),
                updateViewer->GetCurrentAlignments().front());
            ThreaderOptionsDialog optDialog(this, globalThreaderOptions);
            if (optDialog.ShowModal() == wxCANCEL) return;  // user cancelled

            if (!optDialog.GetValues(&globalThreaderOptions)) {
                ERRORMSG("Error retrieving options values from dialog");
                return;
            }

            RaiseLogWindow();
            SetCursor(*wxHOURGLASS_CURSOR);
            updateViewer->alignmentManager->ThreadAllUpdates(globalThreaderOptions);
            SetCursor(wxNullCursor);
            break;
        }
    }
}

void UpdateViewerWindow::OnMerge(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_MERGE_ONE:
            CancelAllSpecialModesExcept(MID_MERGE_ONE);
            if (DoMergeSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                MergeSingleOff();
            break;
        case MID_MERGE_NEIGHBOR:
            CancelAllSpecialModesExcept(MID_MERGE_NEIGHBOR);
            if (DoMergeNeighbor())
                SetCursor(*wxCROSS_CURSOR);
            else
                MergeNeighborOff();
            break;
        case MID_MERGE_ALL:
        {
            AlignmentManager::UpdateMap all;    // construct map/list of all updates
            const ViewerBase::AlignmentList& currentUpdates = updateViewer->GetCurrentAlignments();
            if (currentUpdates.size() > 0) {
                ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates.end();
                for (u=currentUpdates.begin(); u!=ue; ++u) all[*u] = true;
                updateViewer->alignmentManager->MergeUpdates(all, false);
            }
            break;
        }
    }
}

void UpdateViewerWindow::OnDelete(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_DELETE_ALL_BLOCKS:
            CancelAllSpecialModesExcept(MID_DELETE_ALL_BLOCKS);
            if (DoDeleteAllBlocks())
                SetCursor(*wxCROSS_CURSOR);
            else
                DeleteAllBlocksOff();
            break;
        case MID_DELETE_BLOCKS_ALL_ROWS: {
            const ViewerBase::AlignmentList& currentUpdates = updateViewer->GetCurrentAlignments();
            if (currentUpdates.size() > 0) {
                UpdateViewer::AlignmentList newAlignments;
                ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates.end();
                for (u=currentUpdates.begin(); u!=ue; ++u) {
                    BlockMultipleAlignment *newAlignment = (*u)->Clone();
                    newAlignment->DeleteAllBlocks();
                    newAlignments.push_back(newAlignment);
                }
                updateViewer->ReplaceAlignments(newAlignments);
            }
            break;
        }
        case MID_DELETE_ONE:
            CancelAllSpecialModesExcept(MID_DELETE_ONE);
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

bool UpdateViewerWindow::SaveDialog(bool prompt, bool canCancel)
{
    // quick & dirty check for whether save is necessary, by whether Undo is enabled
    if (!menuBar->IsEnabled(MID_UNDO)) return true;

    int option = wxID_YES;

    if (prompt) {
        option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
        if (canCancel) option |= wxCANCEL;

        wxMessageDialog dialog(NULL, "Do you want to keep the changes to these updates?", "", option);
        option = dialog.ShowModal();

        if (option == wxID_CANCEL) return false; // user cancelled this operation
    }

    if (option == wxID_YES)
        updateViewer->SaveAlignments();     // save data
    else {
        updateViewer->Revert();				// revert to original
        updateViewer->EnableStacks();       // but keep undo stack active
    }

    return true;
}

void UpdateViewerWindow::OnImport(wxCommandEvent& event)
{
    if (event.GetId() == MID_IMPORT_SEQUENCES) updateViewer->ImportSequences();
    else if (event.GetId() == MID_IMPORT_STRUCTURE) updateViewer->ImportStructure();
}

void UpdateViewerWindow::OnRunBlast(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_BLAST_ONE:
            CancelAllSpecialModesExcept(MID_BLAST_ONE);
            if (DoBlastSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                BlastSingleOff();
            break;
        case MID_BLAST_PSSM_ONE:
            CancelAllSpecialModesExcept(MID_BLAST_PSSM_ONE);
            if (DoBlastPSSMSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                BlastPSSMSingleOff();
            break;
        case MID_BLAST_NEIGHBOR:
            CancelAllSpecialModesExcept(MID_BLAST_NEIGHBOR);
            if (DoBlastNeighborSingle())
                SetCursor(*wxCROSS_CURSOR);
            else
                BlastNeighborSingleOff();
            break;
    }
}

void UpdateViewerWindow::OnSortUpdates(wxCommandEvent& event)
{
    if (event.GetId() == MID_SORT_UPDATES_IDENTIFIER)
        updateViewer->SortByIdentifier();
    else if (event.GetId() == MID_SORT_UPDATES_PSSM)
        updateViewer->SortByPSSM();
}

void UpdateViewerWindow::OnSetRegion(wxCommandEvent& event)
{
    if (event.GetId() == MID_SET_REGION) {
        CancelAllSpecialModesExcept(MID_SET_REGION);
        if (DoSetRegion())
            SetCursor(*wxCROSS_CURSOR);
        else
            SetRegionOff();
    }

    else if (event.GetId() == MID_RESET_REGIONS) {
		UpdateViewer::AlignmentList::const_iterator a, ae = updateViewer->GetCurrentAlignments().end();
        for (a=updateViewer->GetCurrentAlignments().begin(); a!=ae; ++a) {
            (*a)->alignDependentFrom = -1;
            (*a)->alignDependentTo = -1;
        }
    }
}

void UpdateViewerWindow::OnBlockAlign(wxCommandEvent& event)
{
    if (event.GetId() == MID_BLOCKALIGN_ONE) {
        CancelAllSpecialModesExcept(MID_BLOCKALIGN_ONE);
        if (DoBlockAlignSingle())
            SetCursor(*wxCROSS_CURSOR);
        else
            BlockAlignSingleOff();
    }

    else if (event.GetId() == MID_BLOCKALIGN_ALL) {
        updateViewer->alignmentManager->BlockAlignAllUpdates();
    }
}

void UpdateViewerWindow::OnExtend(wxCommandEvent& event)
{
    if (event.GetId() == MID_EXTEND_ONE) {
        CancelAllSpecialModesExcept(MID_EXTEND_ONE);
        if (DoExtendSingle())
            SetCursor(*wxCROSS_CURSOR);
        else
            ExtendSingleOff();
    }

    else if (event.GetId() == MID_EXTEND_ALL) {
        updateViewer->alignmentManager->ExtendAllUpdates();
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
    wxDialog(parent, -1, "Set Threader Options", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
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
    wxStaticText *item11 = new wxStaticText(panel, -1, "Number of random starts? [1 .. 1000]", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(item11, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    iStarts = new IntegerSpinCtrl(panel,
        1, 1000, 5, initialOptions.nRandomStarts,
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

ThreaderOptionsDialog::~ThreaderOptionsDialog(void)
{
    delete fpWeight;
    delete fpLoops;
    delete iStarts;
    delete iResults;
    delete iCutoff;
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


///// RegionDialog stuff; window setup taken from region_dialog.wdr

#define ID_TEXT 10000
#define ID_T_TITLE 10001
#define ID_TEXTCTRL 10002
#define ID_SPINBUTTON 10003
#define ID_B_OK 10004
#define ID_B_CANCEL 10005

BEGIN_EVENT_TABLE(RegionDialog, wxDialog)
    EVT_BUTTON(-1,  RegionDialog::OnButton)
    EVT_CLOSE (     RegionDialog::OnCloseWindow)
END_EVENT_TABLE()

RegionDialog::RegionDialog(wxWindow* parentFrame,
        const Sequence* sequence, int initialFrom, int initialTo) :
    wxDialog(parentFrame, -1, "Set Alignment Region", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    wxPanel *parent = new wxPanel(this, -1);
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Region" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item3->AddGrowableCol( 1 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, "Sequence:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString title;
    title.Printf("%s, length %i", sequence->identifier->ToString().c_str(), sequence->Length());
    wxStaticText *item5 = new wxStaticText( parent, ID_T_TITLE, title, wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item5, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item6 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item6->AddGrowableCol( 1 );
    item6->AddGrowableRow( 4 );

    wxStaticText *item7 = new wxStaticText( parent, ID_TEXT, "From:", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    iFrom = new IntegerSpinCtrl(parent, 0, sequence->Length(), 1, initialFrom,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item6->Add( iFrom->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );
    item6->Add( iFrom->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item10 = new wxStaticText( parent, ID_TEXT, "To:", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    iTo = new IntegerSpinCtrl(parent, 0, sequence->Length(), 1, initialTo,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item6->Add( iTo->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );
    item6->Add( iTo->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    item1->Add( item6, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item13 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item14 = new wxButton( parent, ID_B_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item14->SetDefault();
    item13->Add( item14, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item15 = new wxButton( parent, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item15, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item13, 0, wxALIGN_CENTRE|wxALL, 5 );

    parent->SetAutoLayout( TRUE );
    parent->SetSizer( item0 );
    item0->Fit( this );
    item0->Fit( parent );
    item0->SetSizeHints( this );
}

RegionDialog::~RegionDialog(void)
{
    delete iFrom;
    delete iTo;
}

void RegionDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void RegionDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetId() == ID_B_OK) {
        int dummy1, dummy2;
        if (GetValues(&dummy1, &dummy2))  // can't successfully quit if values aren't valid
            EndModal(wxOK);
        else
            wxBell();
    } else if (event.GetId() == ID_B_CANCEL) {
        EndModal(wxCANCEL);
    } else {
        event.Skip();
    }
}

bool RegionDialog::GetValues(int *from, int *to)
{
    return (iFrom->GetInteger(from) && iTo->GetInteger(to));
}

END_SCOPE(Cn3D)
