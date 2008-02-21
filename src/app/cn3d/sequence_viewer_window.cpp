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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "remove_header_conflicts.hpp"

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
    EVT_MENU_RANGE(MID_REALIGN_ROW, MID_REALIGN_HLIT_ROWS,  SequenceViewerWindow::OnRealign)
    EVT_MENU_RANGE(MID_SORT_IDENT, MID_SORT_LOOPS,      SequenceViewerWindow::OnSort)
    EVT_MENU      (MID_SCORE_THREADER,                  SequenceViewerWindow::OnScoreThreader)
    EVT_MENU_RANGE(MID_MARK_BLOCK, MID_CLEAR_MARKS,     SequenceViewerWindow::OnMarkBlock)
    EVT_MENU_RANGE(MID_EXPORT_FASTA, MID_EXPORT_PSSM,   SequenceViewerWindow::OnExport)
    EVT_MENU      (MID_SELF_HIT,                        SequenceViewerWindow::OnSelfHit)
    EVT_MENU_RANGE(MID_TAXONOMY_FULL, MID_TAXONOMY_ABBR,    SequenceViewerWindow::OnTaxonomy)
    EVT_MENU_RANGE(MID_HIGHLIGHT_BLOCKS, MID_CLEAR_HIGHLIGHTS,  SequenceViewerWindow::OnHighlight)
    EVT_MENU_RANGE(MID_SQ_REFINER_OPTIONS, MID_SQ_REFINER_RUN,  SequenceViewerWindow::OnRefineAlignment)
END_EVENT_TABLE()

SequenceViewerWindow::SequenceViewerWindow(SequenceViewer *parentSequenceViewer) :
    ViewerWindowBase(parentSequenceViewer, wxPoint(0,500), wxSize(1000,200)),
    sequenceViewer(parentSequenceViewer), taxonomyTree(NULL)
{
    SetWindowTitle();

    viewMenu->AppendSeparator();
    viewMenu->Append(MID_SHOW_HIDE_ROWS, "Show/Hide &Rows");
    viewMenu->Append(MID_SCORE_THREADER, "Show PSSM+Contact &Scores");
    viewMenu->Append(MID_SELF_HIT, "Show Se&lf-Hits");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_EXPORT_FASTA, "&FASTA");
    subMenu->Append(MID_EXPORT_A2M, "&A2M FASTA");
    subMenu->Append(MID_EXPORT_TEXT, "&Text");
    subMenu->Append(MID_EXPORT_HTML, "&HTML");
    subMenu->Append(MID_EXPORT_PSSM, "&PSSM");
    viewMenu->Append(MID_EXPORT, "&Export...", subMenu);
    subMenu = new wxMenu;
    subMenu->Append(MID_TAXONOMY_FULL, "&Full");
    subMenu->Append(MID_TAXONOMY_ABBR, "&Abbreviated");
    viewMenu->Append(MID_TAXONOMY, "Show Ta&xonomy...", subMenu);
    viewMenu->AppendSeparator();
    viewMenu->Append(MID_HIGHLIGHT_BLOCKS, "Highlight &blocks");
    viewMenu->Append(MID_EXPAND_HIGHLIGHTS, "Exp&and Highlights to Aligned Columns");
    viewMenu->Append(MID_RESTRICT_HIGHLIGHTS, "Restr&ict Highlights to Row", "", true);
    viewMenu->Append(MID_CLEAR_HIGHLIGHTS, "Clear &Highlights");

    editMenu->AppendSeparator();
    subMenu = new wxMenu;
    subMenu->Append(MID_SORT_IDENT, "By &Identifier");
    subMenu->Append(MID_SORT_THREADER, "By &Score");
    subMenu->Append(MID_FLOAT_PDBS, "Float &PDBs");
    subMenu->Append(MID_FLOAT_HIGHLIGHTS, "Float Hi&ghlights");
    subMenu->Append(MID_FLOAT_G_V, "Float Geometry &Violations");
    subMenu->Append(MID_SORT_SELF_HIT, "By Self-&Hit");
    subMenu->Append(MID_PROXIMITY_SORT, "Pro&ximity Sort", "", true);
    subMenu->Append(MID_SORT_LOOPS, "By &Loop Length", "", true);
    editMenu->Append(MID_SORT_ROWS, "Sort &Rows...", subMenu);
    editMenu->Append(MID_DELETE_ROW, "De&lete Row", "", true);
    editMenu->AppendSeparator();
    editMenu->Append(MID_SQ_REFINER_OPTIONS, "Set &up Alignment Refiner");
    editMenu->Append(MID_SQ_REFINER_RUN, "Run Alignment Re&finer");

    mouseModeMenu->Append(MID_MOVE_ROW, "&Move Row", "", true);

    updateMenu = new wxMenu;
    updateMenu->Append(MID_SHOW_UPDATES, "&Show Imports");
    updateMenu->AppendSeparator();
    updateMenu->Append(MID_REALIGN_ROW, "Realign &Individual Rows", "", true);
    updateMenu->Append(MID_REALIGN_ROWS, "Realign Rows from &List");
    updateMenu->Append(MID_REALIGN_HLIT_ROWS, "Realign &Highlighted Rows");
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
        menuBar->Enable(MID_DELETE_ROW, enabled);
        menuBar->Enable(MID_SORT_ROWS, enabled);
        menuBar->Enable(MID_MOVE_ROW, enabled);
        menuBar->Enable(MID_REALIGN_ROW, enabled);
        menuBar->Enable(MID_REALIGN_ROWS, enabled);
        menuBar->Enable(MID_REALIGN_HLIT_ROWS, enabled);
        menuBar->Enable(MID_MARK_BLOCK, enabled);
        menuBar->Enable(MID_CLEAR_MARKS, enabled);
        menuBar->Enable(MID_SELF_HIT, editable);
        menuBar->Enable(MID_TAXONOMY, editable);
        menuBar->Enable(MID_SCORE_THREADER, editable);
        menuBar->Enable(MID_HIGHLIGHT_BLOCKS, editable);
        menuBar->Enable(MID_EXPORT, editable);
        menuBar->Enable(MID_EXPAND_HIGHLIGHTS, editable);
        menuBar->Enable(MID_RESTRICT_HIGHLIGHTS, editable);
        menuBar->Enable(MID_SQ_REFINER_OPTIONS, enabled);
        menuBar->Enable(MID_SQ_REFINER_RUN, enabled);
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
        ProcessCommand(MID_ENABLE_EDIT);
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
    vector < const Sequence * > dependentSequences;
    sequenceViewer->alignmentManager->GetAlignmentSetDependentSequences(&dependentSequences);
    wxString *titleStrs = new wxString[dependentSequences.size()];
    for (unsigned int i=0; i<dependentSequences.size(); ++i)
        titleStrs[i] = dependentSequences[i]->identifier->ToString().c_str();

    vector < bool > visibilities;
    sequenceViewer->alignmentManager->GetAlignmentSetDependentVisibilities(&visibilities);

    wxString title = "Show/Hide Dependents of ";
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
    sequenceViewer->alignmentManager->GetAlignmentSetDependentVisibilities(&visibilities);

    unsigned int i;
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

    // get dependent rows to realign (in display order)
    SequenceDisplay::SequenceList sequences;
    sequenceViewer->GetCurrentDisplay()->GetSequences(alignment, &sequences);
    vector < bool > selectedDependents(sequences.size() - 1, false);
    unsigned int i;

    // selection dialog
    if (event.GetId() == MID_REALIGN_ROWS) {
        // get titles of current dependent display rows (*not* rows from the AlignmentSet!)
        wxString *titleStrs = new wxString[sequences.size() - 1];
        for (i=1; i<sequences.size(); ++i)  // assuming master is first sequence
            titleStrs[i - 1] = sequences[i]->identifier->ToString().c_str();

        wxString title = "Realign Dependents of ";
        title.Append(alignment->GetMaster()->identifier->ToString().c_str());
        ShowHideDialog dialog(
            titleStrs, &selectedDependents,
            NULL,   // no "apply" button or callback
            true, this, -1, title, wxPoint(200, 100));
        dialog.ShowModal();
    }

    // select rows w/ any highlights
    else if (event.GetId() == MID_REALIGN_HLIT_ROWS) {
        for (i=1; i<sequences.size(); ++i)  // assuming master is first sequence
            if (GlobalMessenger()->IsHighlightedAnywhere(sequences[i]->identifier))
                selectedDependents[i - 1] = true;
    }

    // make list of dependent rows to be realigned
    vector < unsigned int > rowOrder, realignDependents;
    sequenceViewer->GetCurrentDisplay()->GetRowOrder(alignment, &rowOrder);
    for (i=0; i<selectedDependents.size(); ++i)
        if (selectedDependents[i])
            realignDependents.push_back(rowOrder[i + 1]);

    // do the realignment
    if (realignDependents.size() > 0)
        sequenceViewer->alignmentManager->RealignDependentSequences(alignment, realignDependents);
}

#define MASTER_HAS_STRUCTURE \
    (sequenceViewer->alignmentManager != NULL && \
     sequenceViewer->alignmentManager->GetCurrentMultipleAlignment() != NULL && \
     sequenceViewer->alignmentManager->GetCurrentMultipleAlignment()->GetSequenceOfRow(0)->molecule != NULL)

void SequenceViewerWindow::OnSort(wxCommandEvent& event)
{
    if (event.GetId() != MID_PROXIMITY_SORT && DoProximitySort())
        ProximitySortOff();
    if (event.GetId() != MID_SORT_LOOPS && DoSortLoops())
        SortLoopsOff();

    switch (event.GetId()) {
        case MID_SORT_IDENT:
            sequenceViewer->GetCurrentDisplay()->SortRowsByIdentifier();
            break;
        case MID_SORT_THREADER:
        {
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
            sequenceViewer->GetCurrentDisplay()->FloatHighlightsToTop();
            break;
        case MID_FLOAT_PDBS:
            sequenceViewer->GetCurrentDisplay()->FloatPDBRowsToTop();
            break;
        case MID_FLOAT_G_V:
            sequenceViewer->GetCurrentDisplay()->FloatGVToTop();
            break;
        case MID_SORT_SELF_HIT:
            sequenceViewer->GetCurrentDisplay()->SortRowsBySelfHit();
            break;
        case MID_PROXIMITY_SORT:
            CancelAllSpecialModesExcept(MID_PROXIMITY_SORT);
            if (DoProximitySort())
                SetCursor(*wxCROSS_CURSOR);
            else
                ProximitySortOff();
            break;
        case MID_SORT_LOOPS:
            CancelAllSpecialModesExcept(MID_SORT_LOOPS);
            if (DoSortLoops())
                SetCursor(*wxCROSS_CURSOR);
            else
                SortLoopsOff();
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
        ProcessCommand(SequenceViewerWindow::MID_ENABLE_EDIT);
}

void SequenceViewerWindow::OnExport(wxCommandEvent& event)
{
    SequenceViewer::eExportType type;
    if (event.GetId() == MID_EXPORT_FASTA) type = SequenceViewer::asFASTA;
    else if (event.GetId() == MID_EXPORT_A2M) type = SequenceViewer::asFASTAa2m;
    else if (event.GetId() == MID_EXPORT_TEXT) type = SequenceViewer::asText;
    else if (event.GetId() == MID_EXPORT_HTML) type = SequenceViewer::asHTML;
    else if (event.GetId() == MID_EXPORT_PSSM) type = SequenceViewer::asPSSM;
    else {
        ERRORMSG("SequenceViewerWindow::OnExport() - unrecognized export type");
        return;
    }
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

void SequenceViewerWindow::OnHighlight(wxCommandEvent& event)
{
    if (event.GetId() == MID_CLEAR_HIGHLIGHTS) {
        GlobalMessenger()->RemoveAllHighlights(true);
        return;
    }

    if (sequenceViewer->GetCurrentAlignments().size() == 0) return;

    if (event.GetId() == MID_HIGHLIGHT_BLOCKS) {
        GlobalMessenger()->RemoveAllHighlights(true);

        const BlockMultipleAlignment *multiple = sequenceViewer->GetCurrentAlignments().front();
        BlockMultipleAlignment::UngappedAlignedBlockList blocks;
        multiple->GetUngappedAlignedBlocks(&blocks);
        if (blocks.size() == 0) return;

        BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
        const Sequence *seq;
        const Block::Range *range;
        for (unsigned int row=0; row<multiple->NRows(); ++row) {
            seq = multiple->GetSequenceOfRow(row);
            for (b = blocks.begin(); b!=be; ++b) {
                range = (*b)->GetRangeOfRow(row);
                GlobalMessenger()->AddHighlights(seq, range->from, range->to);
            }
        }
    }

    else if (event.GetId() == MID_EXPAND_HIGHLIGHTS) {
        if (!GlobalMessenger()->IsAnythingHighlighted())
            return;
        const BlockMultipleAlignment *multiple = sequenceViewer->GetCurrentAlignments().front();
        BlockMultipleAlignment::UngappedAlignedBlockList blocks;
        multiple->GetUngappedAlignedBlocks(&blocks);
        if (blocks.size() == 0)
            return;

        // first find all alignment indexes (columns) that have highlights in any row
        BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
        unsigned int row, blockColumn, seqIndex=0;
        typedef list < pair < int, int > > IntPairList;     // pair is < start, len >
        IntPairList alignmentIndexesWithHighlights;
        bool inHighlightedRange;
        for (b = blocks.begin(); b!=be; ++b) {
            inHighlightedRange = false;
            for (blockColumn=0; blockColumn<(*b)->width; ++blockColumn) {
                for (row=0; row<multiple->NRows(); ++row) {
                    seqIndex = (*b)->GetRangeOfRow(row)->from + blockColumn;
                    if (GlobalMessenger()->IsHighlighted(multiple->GetSequenceOfRow(row), seqIndex))
                        break;
                }
                if (row < multiple->NRows()) {  // found highlight in some row
                    if (inHighlightedRange) {
                        ++(alignmentIndexesWithHighlights.back().second);
                    } else {
                        alignmentIndexesWithHighlights.push_back(make_pair(
                            multiple->GetAlignmentIndex(row, seqIndex,
                                BlockMultipleAlignment::eLeft),     // justification irrelevant since block is aligned
                            1));
                        inHighlightedRange = true;
                    }
                } else
                    inHighlightedRange = false;
            }
        }

        // now highlight all contiguous columns
        IntPairList::const_iterator h, he = alignmentIndexesWithHighlights.end();
        for (h=alignmentIndexesWithHighlights.begin(); h!=he; ++h) {
            TRACEMSG("highlighting from alignment index " << h->first << " to " << (h->first + h->second - 1));
            for (row=0; row<multiple->NRows(); ++row) {
                multiple->SelectedRange(row, h->first, h->first + h->second - 1,
                    BlockMultipleAlignment::eLeft, false);      // justification irrelevant since block is aligned
            }
        }
    }

    else if (event.GetId() == MID_RESTRICT_HIGHLIGHTS) {
        if (!GlobalMessenger()->IsAnythingHighlighted())
            return;
        CancelAllSpecialModesExcept(MID_RESTRICT_HIGHLIGHTS);
        if (DoRestrictHighlights())
            SetCursor(*wxCROSS_CURSOR);
        else
            RestrictHighlightsOff();
    }
}

void SequenceViewerWindow::OnRefineAlignment(wxCommandEvent& event)
{
    if (sequenceViewer->GetCurrentAlignments().size() == 0) {
        ERRORMSG("SequenceViewerWindow::OnRefineAlignment() - no alignment!");
        return;
    }
    if (event.GetId() == MID_SQ_REFINER_RUN)
        ProcessCommand(MID_HIGHLIGHT_BLOCKS);
    sequenceViewer->alignmentManager->RefineAlignment(event.GetId() == MID_SQ_REFINER_OPTIONS);
}

END_SCOPE(Cn3D)
