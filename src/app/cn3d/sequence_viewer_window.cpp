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

#include "taxonomy_tree.hpp"
#include "sequence_viewer_window.hpp"
#include "sequence_viewer.hpp"
#include "alignment_manager.hpp"
#include "sequence_set.hpp"
#include "show_hide_dialog.hpp"
#include "sequence_display.hpp"
#include "messenger.hpp"
#include "wx_tools.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_tools.hpp"
#include "cn3d_blast.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

static double prevPSSMWeight = -1.0;    // so scoring dialog remembers prev value

BEGIN_EVENT_TABLE(SequenceViewerWindow, wxFrame)
    INCLUDE_VIEWER_WINDOW_BASE_EVENTS
    EVT_CLOSE     (                                     SequenceViewerWindow::OnCloseWindow)
    EVT_MENU      (MID_SHOW_HIDE_ROWS,                  SequenceViewerWindow::OnShowHideRows)
    EVT_MENU      (MID_DELETE_ROW,                      SequenceViewerWindow::OnDeleteRow)
    EVT_MENU      (MID_MOVE_ROW,                        SequenceViewerWindow::OnMoveRow)
    EVT_MENU      (MID_SHOW_UPDATES,                    SequenceViewerWindow::OnShowUpdates)
    EVT_MENU_RANGE(MID_REALIGN_ROW, MID_REALIGN_ROWS,   SequenceViewerWindow::OnRealign)
    EVT_MENU_RANGE(MID_SORT_IDENT, MID_PROXIMITY_SORT,  SequenceViewerWindow::OnSort)
    EVT_MENU      (MID_SCORE_THREADER,                  SequenceViewerWindow::OnScoreThreader)
    EVT_MENU_RANGE(MID_MARK_BLOCK, MID_CLEAR_MARKS,     SequenceViewerWindow::OnMarkBlock)
    EVT_MENU_RANGE(MID_EXPORT_FASTA, MID_EXPORT_HTML,   SequenceViewerWindow::OnExport)
    EVT_MENU      (MID_SELF_HIT,                        SequenceViewerWindow::OnSelfHit)
    EVT_MENU_RANGE(MID_TAXONOMY_FULL, MID_TAXONOMY_ABBR,    SequenceViewerWindow::OnTaxonomy)
    EVT_MENU      (MID_HIGHLIGHT_BLOCKS,                SequenceViewerWindow::OnHighlightBlocks)
END_EVENT_TABLE()

SequenceViewerWindow::SequenceViewerWindow(SequenceViewer *parentSequenceViewer) :
    ViewerWindowBase(parentSequenceViewer, wxPoint(0,500), wxSize(1000,200)),
    sequenceViewer(parentSequenceViewer), taxonomyTree(NULL)
{
    SetWindowTitle();

    viewMenu->Append(MID_SHOW_HIDE_ROWS, "Show/Hide &Rows");
    viewMenu->Append(MID_SCORE_THREADER, "Show PSSM+Contact &Scores");
    viewMenu->Append(MID_SELF_HIT, "Show Se&lf-Hits");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_EXPORT_FASTA, "&FASTA");
    subMenu->Append(MID_EXPORT_A2M, "&A2M FASTA");
    subMenu->Append(MID_EXPORT_TEXT, "&Text");
    subMenu->Append(MID_EXPORT_HTML, "&HTML");
    viewMenu->Append(MID_EXPORT, "&Export...", subMenu);
    subMenu = new wxMenu;
    subMenu->Append(MID_TAXONOMY_FULL, "&Full");
    subMenu->Append(MID_TAXONOMY_ABBR, "&Abbreviated");
    viewMenu->Append(MID_TAXONOMY, "Show Ta&xonomy...", subMenu);
    viewMenu->Append(MID_HIGHLIGHT_BLOCKS, "&Highlight blocks");

    editMenu->AppendSeparator();
    subMenu = new wxMenu;
    subMenu->Append(MID_SORT_IDENT, "By &Identifier");
    subMenu->Append(MID_SORT_THREADER, "By &Score");
    subMenu->Append(MID_FLOAT_PDBS, "Float &PDBs");
    subMenu->Append(MID_FLOAT_HIGHLIGHTS, "Float Hi&ghlights");
    subMenu->Append(MID_FLOAT_G_V, "Float &Geometry Violations");
    subMenu->Append(MID_SORT_SELF_HIT, "By Self-&Hit");
    subMenu->Append(MID_PROXIMITY_SORT, "&Proximity Sort", "", true);
    editMenu->Append(MID_SORT_ROWS, "Sort &Rows...", subMenu);
    editMenu->Append(MID_DELETE_ROW, "De&lete Row", "", true);

    mouseModeMenu->Append(MID_MOVE_ROW, "&Move Row", "", true);

    updateMenu = new wxMenu;
    updateMenu->Append(MID_SHOW_UPDATES, "&Show Imports");
    updateMenu->AppendSeparator();
    updateMenu->Append(MID_REALIGN_ROW, "Realign &Individual Rows", "", true);
    updateMenu->Append(MID_REALIGN_ROWS, "Realign Rows from &List");
    updateMenu->AppendSeparator();
    updateMenu->Append(MID_MARK_BLOCK, "Mark &Block", "", true);
    updateMenu->Append(MID_CLEAR_MARKS, "&Clear Marks");
    menuBar->Append(updateMenu, "&Imports");

    EnableDerivedEditorMenuItems(false);

    SetMenuBar(menuBar);
}

SequenceViewerWindow::~SequenceViewerWindow(void)
{
    if (taxonomyTree) delete taxonomyTree;
}

void SequenceViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    if (viewer) {
        if (event.CanVeto()) {
            Show(false);    // just hide the window if we can
            event.Veto();
            return;
        }
        SaveDialog(true, false);
        viewer->RemoveBlockBoundaryRows();
        viewer->GUIDestroyed(); // make sure SequenceViewer knows the GUI is gone
        GlobalMessenger()->UnPostRedrawSequenceViewer(viewer);  // don't try to redraw after destroyed!
    }
    Destroy();
}

void SequenceViewerWindow::SetWindowTitle(void)
{
    SetTitle(wxString(GetWorkingTitle().c_str()) + " - Sequence/Alignment Viewer");
}

void SequenceViewerWindow::EnableDerivedEditorMenuItems(bool enabled)
{
    if (menuBar->FindItem(MID_SHOW_HIDE_ROWS)) {
        bool editable = (sequenceViewer->GetCurrentDisplay() &&
            sequenceViewer->GetCurrentDisplay()->IsEditable());
        if (editable)
            menuBar->Enable(MID_SHOW_HIDE_ROWS, !enabled);  // can't show/hide when editor is on
        else
            menuBar->Enable(MID_SHOW_HIDE_ROWS, false);     // can't show/hide in non-alignment display
        menuBar->Enable(MID_DELETE_ROW, enabled);           // can only delete row when editor is on
        menuBar->Enable(MID_SORT_ROWS, enabled);
        menuBar->Enable(MID_MOVE_ROW, enabled);             // can only move row when editor is on
        menuBar->Enable(MID_REALIGN_ROW, enabled);          // can only realign rows when editor is on
        menuBar->Enable(MID_REALIGN_ROWS, enabled);         // can only realign rows when editor is on
        menuBar->Enable(MID_MARK_BLOCK, enabled);
        menuBar->Enable(MID_CLEAR_MARKS, enabled);
        menuBar->Enable(MID_SELF_HIT, editable);
        menuBar->Enable(MID_TAXONOMY, editable);
        menuBar->Enable(MID_SCORE_THREADER, editable);
        menuBar->Enable(MID_HIGHLIGHT_BLOCKS, editable);
        if (!enabled) CancelDerivedSpecialModesExcept(-1);
    }
}

void SequenceViewerWindow::OnDeleteRow(wxCommandEvent& event)
{
    if (event.GetId() == MID_DELETE_ROW) {
        CancelAllSpecialModesExcept(MID_DELETE_ROW);
        if (DoDeleteRow())
            SetCursor(*wxCROSS_CURSOR);
        else
            DeleteRowOff();
    }
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
        return SaveDialog(true, true);
    }
}

bool SequenceViewerWindow::SaveDialog(bool prompt, bool canCancel)
{
    static bool overrideCanCancel = false, prevPrompt, prevCanCancel;
    if (overrideCanCancel) {
        prompt = prevPrompt;
        canCancel = prevCanCancel;
        overrideCanCancel = false;
    }

    // if editor is checked on, then this save command was initiated outside the menu;
    // if so, then need to turn off editor pretending it was done from 'enable editor' menu item
    if (menuBar->IsChecked(MID_ENABLE_EDIT)) {
        overrideCanCancel = true;
        prevPrompt = prompt;
        prevCanCancel = canCancel;
        Command(MID_ENABLE_EDIT);
        return true;
    }

    // quick & dirty check for whether save is necessary, by whether Undo is enabled
    if (!menuBar->IsEnabled(MID_UNDO)) {
        viewer->KeepCurrent();  // remove any unnecessary copy from stack
        return true;
    }

    int option = wxID_YES;

    if (prompt) {
        option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
        if (canCancel) option |= wxCANCEL;

        wxMessageDialog dialog(NULL, "Do you want to keep the changes to this alignment?", "", option);
        option = dialog.ShowModal();

        if (option == wxID_CANCEL) return false; // user cancelled this operation
    }

    if (option == wxID_YES) {
        sequenceViewer->SaveAlignment();    // save data
    } else {
        sequenceViewer->Revert();  // revert to original
		UpdateDisplay(sequenceViewer->GetCurrentDisplay());
	}

    return true;
}

void SequenceViewerWindow::OnShowHideRows(wxCommandEvent& event)
{
    vector < const Sequence * > slaveSequences;
    sequenceViewer->alignmentManager->GetAlignmentSetSlaveSequences(&slaveSequences);
    wxString *titleStrs = new wxString[slaveSequences.size()];
    for (int i=0; i<slaveSequences.size(); ++i)
        titleStrs[i] = slaveSequences[i]->identifier->ToString().c_str();

    vector < bool > visibilities;
    sequenceViewer->alignmentManager->GetAlignmentSetSlaveVisibilities(&visibilities);

    wxString title = "Show/Hide Slaves of ";
    title.Append(sequenceViewer->alignmentManager->GetCurrentMultipleAlignment()->GetMaster()->identifier->ToString().c_str());
    ShowHideDialog dialog(
        titleStrs, &visibilities, sequenceViewer->alignmentManager, true,
        this, -1, title, wxPoint(250, 50));
    dialog.ShowModal();
    //delete titleStrs;    // apparently deleted by wxWindows
}

bool SequenceViewerWindow::QueryShowAllRows(void)
{
    vector < bool > visibilities;
    sequenceViewer->alignmentManager->GetAlignmentSetSlaveVisibilities(&visibilities);

    int i;
    for (i=0; i<visibilities.size(); ++i) if (!visibilities[i]) break;
    if (i == visibilities.size()) return true;  // we're okay if all rows already visible

    // if some rows hidden, ask user whether to show all rows, or cancel
    wxMessageDialog query(NULL,
        "This operation requires all alignment rows to be visible. Do you wish to show all rows now?",
        "Query", wxOK | wxCANCEL | wxCENTRE | wxICON_QUESTION);

    if (query.ShowModal() == wxID_CANCEL) return false;   // user cancelled

    // show all rows
    for (i=0; i<visibilities.size(); ++i) visibilities[i] = true;
    sequenceViewer->alignmentManager->ShowHideCallbackFunction(visibilities);
    return true;
}

// process events from the realign menu
void SequenceViewerWindow::OnShowUpdates(wxCommandEvent& event)
{
    sequenceViewer->alignmentManager->ShowUpdateWindow();
}

void SequenceViewerWindow::OnRealign(wxCommandEvent& event)
{
    // setup one-at-a-time row realignment
    if (event.GetId() == MID_REALIGN_ROW) {
        CancelAllSpecialModesExcept(MID_REALIGN_ROW);
        if (DoRealignRow())
            SetCursor(*wxCROSS_CURSOR);
        else
            RealignRowOff();
        return;
    }

    // bring up selection dialog for realigning multiple rows
    if (sequenceViewer->GetCurrentAlignments().size() == 0) {
        ERRORMSG("SequenceViewerWindow::OnRealign() - no alignment!");
        return;
    }
    BlockMultipleAlignment *alignment = sequenceViewer->GetCurrentAlignments().front();

    // get titles of current slave display rows (*not* rows from the AlignmentSet!)
    SequenceDisplay::SequenceList sequences;
    sequenceViewer->GetCurrentDisplay()->GetSequences(alignment, &sequences);
    wxString *titleStrs = new wxString[sequences.size() - 1];
    int i;
    for (i=1; i<sequences.size(); ++i)  // assuming master is first sequence
        titleStrs[i - 1] = sequences[i]->identifier->ToString().c_str();

    vector < bool > selectedSlaves(sequences.size() - 1, false);

    wxString title = "Realign Slaves of ";
    title.Append(alignment->GetMaster()->identifier->ToString().c_str());
    ShowHideDialog dialog(
        titleStrs, &selectedSlaves,
        NULL,   // no "apply" button or callback
        true, this, -1, title, wxPoint(200, 100));
    dialog.ShowModal();

    // make list of slave rows to be realigned
    vector < int > rowOrder, realignSlaves;
    sequenceViewer->GetCurrentDisplay()->GetRowOrder(alignment, &rowOrder);
    for (i=0; i<selectedSlaves.size(); ++i)
        if (selectedSlaves[i])
            realignSlaves.push_back(rowOrder[i + 1]);

    // do the realignment
    if (realignSlaves.size() >= 0)
        sequenceViewer->alignmentManager->RealignSlaveSequences(alignment, realignSlaves);
}

#define MASTER_HAS_STRUCTURE \
    (sequenceViewer->alignmentManager != NULL && \
     sequenceViewer->alignmentManager->GetCurrentMultipleAlignment() != NULL && \
     sequenceViewer->alignmentManager->GetCurrentMultipleAlignment()->GetSequenceOfRow(0)->molecule != NULL)

void SequenceViewerWindow::OnSort(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SORT_IDENT:
            if (DoProximitySort()) ProximitySortOff();
            sequenceViewer->GetCurrentDisplay()->SortRowsByIdentifier();
            break;
        case MID_SORT_THREADER:
        {
            if (DoProximitySort()) ProximitySortOff();
            GetFloatingPointDialog fpDialog(NULL,
                "Weighting of PSSM/Contact score? ([0..1], 1 = PSSM only)", "Enter PSSM Weight",
                0.0, 1.0, 0.05, (MASTER_HAS_STRUCTURE ?
                    ((prevPSSMWeight >= 0.0) ? prevPSSMWeight : 0.5) : 1.0));
            if (fpDialog.ShowModal() == wxOK) {
                double weightPSSM = prevPSSMWeight = fpDialog.GetValue();
                SetCursor(*wxHOURGLASS_CURSOR);
                sequenceViewer->GetCurrentDisplay()->SortRowsByThreadingScore(weightPSSM);
                SetCursor(wxNullCursor);
            }
            break;
        }
        case MID_FLOAT_HIGHLIGHTS:
            if (DoProximitySort()) ProximitySortOff();
            sequenceViewer->GetCurrentDisplay()->FloatHighlightsToTop();
            break;
        case MID_FLOAT_PDBS:
            if (DoProximitySort()) ProximitySortOff();
            sequenceViewer->GetCurrentDisplay()->FloatPDBRowsToTop();
            break;
        case MID_FLOAT_G_V:
            if (DoProximitySort()) ProximitySortOff();
            sequenceViewer->GetCurrentDisplay()->FloatGVToTop();
            break;
        case MID_SORT_SELF_HIT:
            if (DoProximitySort()) ProximitySortOff();
            sequenceViewer->GetCurrentDisplay()->SortRowsBySelfHit();
            break;
        case MID_PROXIMITY_SORT:
            CancelAllSpecialModesExcept(MID_PROXIMITY_SORT);
            if (DoProximitySort())
                SetCursor(*wxCROSS_CURSOR);
            else
                ProximitySortOff();
            break;
    }
}

void SequenceViewerWindow::OnScoreThreader(wxCommandEvent& event)
{
    GetFloatingPointDialog fpDialog(NULL,
        "Weighting of PSSM/Contact score? ([0..1], 1 = PSSM only)", "Enter PSSM Weight",
        0.0, 1.0, 0.05, (MASTER_HAS_STRUCTURE ?
            ((prevPSSMWeight >= 0.0) ? prevPSSMWeight : 0.5) : 1.0));
    if (fpDialog.ShowModal() == wxOK) {
        double weightPSSM = prevPSSMWeight = fpDialog.GetValue();
        SetCursor(*wxHOURGLASS_CURSOR);
        if (sequenceViewer->GetCurrentDisplay()->IsEditable())
            sequenceViewer->GetCurrentDisplay()->CalculateRowScoresWithThreader(weightPSSM);
        SetCursor(wxNullCursor);
    }
}

void SequenceViewerWindow::OnMarkBlock(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_MARK_BLOCK:
            CancelAllSpecialModesExcept(MID_MARK_BLOCK);
            if (DoMarkBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                MarkBlockOff();
            break;
        case MID_CLEAR_MARKS:
            if (sequenceViewer->GetCurrentAlignments().size() > 0 &&
                    sequenceViewer->GetCurrentAlignments().front()->ClearMarks())
                GlobalMessenger()->PostRedrawSequenceViewer(sequenceViewer);
            break;
    }
}

void SequenceViewerWindow::TurnOnEditor(void)
{
    if (!menuBar->IsChecked(MID_ENABLE_EDIT))
        Command(SequenceViewerWindow::MID_ENABLE_EDIT);
}

void SequenceViewerWindow::OnExport(wxCommandEvent& event)
{
    SequenceViewer::eExportType type;
    if (event.GetId() == MID_EXPORT_FASTA) type = SequenceViewer::asFASTA;
    else if (event.GetId() == MID_EXPORT_A2M) type = SequenceViewer::asFASTAa2m;
    else if (event.GetId() == MID_EXPORT_TEXT) type = SequenceViewer::asText;
    else if (event.GetId() == MID_EXPORT_HTML) type = SequenceViewer::asHTML;
    sequenceViewer->ExportAlignment(type);
}

void SequenceViewerWindow::OnSelfHit(wxCommandEvent& event)
{
    if (sequenceViewer->GetCurrentAlignments().size() > 0) {
        const BlockMultipleAlignment *multiple = sequenceViewer->GetCurrentAlignments().front();
        sequenceViewer->alignmentManager->blaster->CalculateSelfHitScores(multiple);
    }
}

void SequenceViewerWindow::OnTaxonomy(wxCommandEvent& event)
{
    if (!taxonomyTree) taxonomyTree = new TaxonomyTree();
    if (sequenceViewer->GetCurrentAlignments().size() > 0)
        taxonomyTree->ShowTreeForAlignment(this, sequenceViewer->GetCurrentAlignments().front(),
            (event.GetId() == MID_TAXONOMY_ABBR));
}

void SequenceViewerWindow::OnHighlightBlocks(wxCommandEvent& event)
{
    if (sequenceViewer->GetCurrentAlignments().size() == 0) return;
    GlobalMessenger()->RemoveAllHighlights(true);

    const BlockMultipleAlignment *multiple = sequenceViewer->GetCurrentAlignments().front();
    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);
    if (blocks.size() == 0) return;

    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
    const Sequence *seq;
    const Block::Range *range;
    for (int row=0; row<multiple->NRows(); ++row) {
        seq = multiple->GetSequenceOfRow(row);
        for (b = blocks.begin(); b!=be; ++b) {
            range = (*b)->GetRangeOfRow(row);
            GlobalMessenger()->AddHighlights(seq, range->from, range->to);
        }
    }
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.54  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.53  2004/03/15 18:32:03  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.52  2004/02/19 17:05:10  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.51  2003/10/20 13:17:15  thiessen
* add float geometry violations sorting
*
* Revision 1.50  2003/08/23 22:42:17  thiessen
* add highlight blocks command
*
* Revision 1.49  2003/03/06 19:23:56  thiessen
* fix for compilation problem (seqfeat macros)
*
* Revision 1.48  2003/02/03 19:20:06  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.47  2003/01/14 14:15:07  thiessen
* default PSSM score = 1.0 when no structure
*
* Revision 1.46  2002/12/19 14:15:37  thiessen
* mac fixes to menus, add icon
*
* Revision 1.45  2002/12/06 17:07:15  thiessen
* remove seqrow export format; add choice of repeat handling for FASTA export; export rows in display order
*
* Revision 1.44  2002/12/02 13:37:09  thiessen
* add seqrow format export
*
* Revision 1.43  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.42  2002/10/07 18:51:53  thiessen
* add abbreviated taxonomy tree
*
* Revision 1.41  2002/10/04 18:45:28  thiessen
* updates to taxonomy viewer
*
* Revision 1.40  2002/09/09 22:51:19  thiessen
* add basic taxonomy tree viewer
*
* Revision 1.39  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.38  2002/09/06 13:06:31  thiessen
* fix menu accelerator conflicts
*
* Revision 1.37  2002/09/05 18:38:57  thiessen
* add sort by highlights
*
* Revision 1.36  2002/09/03 13:15:58  thiessen
* add A2M export
*
* Revision 1.35  2002/08/15 22:13:17  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.34  2002/06/13 14:54:07  thiessen
* add sort by self-hit
*
* Revision 1.33  2002/06/13 13:32:39  thiessen
* add self-hit calculation
*
* Revision 1.32  2002/06/05 17:25:47  thiessen
* change 'update' to 'import' in GUI
*
* Revision 1.31  2002/06/05 14:28:40  thiessen
* reorganize handling of window titles
*
* Revision 1.30  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.29  2002/04/22 14:27:29  thiessen
* add alignment export
*
* Revision 1.28  2002/03/19 18:48:00  thiessen
* small bug fixes; remember PSSM weight
*
* Revision 1.27  2002/03/04 15:52:14  thiessen
* hide sequence windows instead of destroying ; add perspective/orthographic projection choice
*
* Revision 1.26  2002/02/13 14:53:30  thiessen
* add update sort
*
* Revision 1.25  2001/12/06 23:13:45  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.24  2001/10/20 20:16:32  thiessen
* don't use wxDefaultPosition for dialogs (win2000 problem)
*
* Revision 1.23  2001/10/08 14:18:33  thiessen
* fix show/hide dialog under wxGTK
*
* Revision 1.22  2001/10/01 16:04:24  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.21  2001/09/06 13:10:10  thiessen
* tweak show hide dialog layout
*
* Revision 1.20  2001/08/24 18:53:43  thiessen
* add filename to sequence viewer window titles
*
* Revision 1.19  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.18  2001/06/04 14:58:00  thiessen
* add proximity sort; highlight sequence on browser launch
*
* Revision 1.17  2001/06/01 14:05:13  thiessen
* add float PDB sort
*
* Revision 1.16  2001/05/17 18:34:26  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.15  2001/05/15 23:48:37  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.14  2001/05/11 02:10:42  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.13  2001/05/09 17:15:07  thiessen
* add automatic block removal upon demotion
*
* Revision 1.12  2001/05/08 21:15:44  thiessen
* add PSSM weight dialog for sorting
*
* Revision 1.11  2001/04/04 00:27:15  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.10  2001/03/30 14:43:41  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.9  2001/03/30 03:07:34  thiessen
* add threader score calculation & sorting
*
* Revision 1.8  2001/03/19 15:50:40  thiessen
* add sort rows by identifier
*
* Revision 1.7  2001/03/17 14:06:49  thiessen
* more workarounds for namespace/#define conflicts
*
* Revision 1.6  2001/03/13 01:25:06  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.5  2001/03/09 15:49:05  thiessen
* major changes to add initial update viewer
*
* Revision 1.4  2001/03/06 20:20:51  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.3  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.2  2001/03/02 03:26:59  thiessen
* fix dangling pointer upon app close
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
*/
