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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

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
#include "wx_tools.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_ba_interface.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(UpdateViewerWindow, wxFrame)
    INCLUDE_VIEWER_WINDOW_BASE_EVENTS
    EVT_CLOSE     (                                                     UpdateViewerWindow::OnCloseWindow)
    EVT_MENU_RANGE(MID_DELETE_ALL_BLOCKS, MID_DELETE_BLOCKS_ALL_ROWS,   UpdateViewerWindow::OnDelete)
    EVT_MENU      (MID_SORT_UPDATES_IDENTIFIER,                         UpdateViewerWindow::OnSortUpdates)
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
    menu->Append(MID_BLOCKALIGN_ALL, "Bloc&k Align All");
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
            (*a)->alignSlaveFrom = -1;
            (*a)->alignSlaveTo = -1;
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
    wxDialog(parentFrame, -1, "Set Alignment Region", wxDefaultPosition, wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU) // not resizable
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.64  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.63  2004/10/04 17:00:54  thiessen
* add expand/restrict highlights, delete all blocks/all rows in updates
*
* Revision 1.62  2004/09/23 10:31:14  thiessen
* add block extension algorithm
*
* Revision 1.61  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.60  2004/03/15 18:38:52  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.59  2004/02/19 17:05:21  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.58  2003/09/25 15:14:09  thiessen
* add Reset All Regions command
*
* Revision 1.57  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.56  2003/01/31 17:18:59  thiessen
* many small additions and changes...
*
* Revision 1.55  2003/01/29 01:41:06  thiessen
* add merge neighbor instead of merge near highlight
*
* Revision 1.54  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.53  2002/12/19 14:15:37  thiessen
* mac fixes to menus, add icon
*
* Revision 1.52  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.51  2002/09/19 12:51:08  thiessen
* fix block aligner / update bug; add distance select for other molecules only
*
* Revision 1.50  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.49  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.48  2002/08/15 22:13:18  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.47  2002/08/13 20:46:38  thiessen
* add global block aligner
*
* Revision 1.46  2002/08/01 01:55:16  thiessen
* add block aligner options dialog
*
* Revision 1.45  2002/07/26 15:28:48  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.44  2002/07/03 13:39:40  thiessen
* fix for redundant sequence removal
*
* Revision 1.43  2002/06/05 17:25:47  thiessen
* change 'update' to 'import' in GUI
*
* Revision 1.42  2002/06/05 14:28:41  thiessen
* reorganize handling of window titles
*
* Revision 1.41  2002/05/22 17:17:10  thiessen
* progress on BLAST interface ; change custom spin ctrl implementation
*
* Revision 1.40  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.39  2002/05/07 20:22:47  thiessen
* fix for BLAST/PSSM
*
* Revision 1.38  2002/05/02 18:40:25  thiessen
* do BLAST/PSSM for debug builds only, for testing
*
* Revision 1.37  2002/04/27 16:32:15  thiessen
* fix small leaks/bugs found by BoundsChecker
*
* Revision 1.36  2002/04/26 13:46:42  thiessen
* comment out all blast/pssm methods
*
* Revision 1.35  2002/04/15 15:34:55  thiessen
* remove BLAST/PSSM for now
*
* Revision 1.34  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.33  2002/03/04 15:52:15  thiessen
* hide sequence windows instead of destroying ; add perspective/orthographic projection choice
*
* Revision 1.32  2002/02/21 22:01:49  thiessen
* remember alignment range on demotion
*
* Revision 1.31  2002/02/21 12:26:30  thiessen
* fix row delete bug ; remember threader options
*
* Revision 1.30  2002/02/13 14:53:30  thiessen
* add update sort
*
* Revision 1.29  2002/02/12 17:19:23  thiessen
* first working structure import
*
* Revision 1.28  2001/12/06 23:13:47  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.27  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
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
*/
