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
*      Classes to interface with sequence viewer GUI
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.31  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.30  2000/12/21 23:42:16  thiessen
* load structures from cdd's
*
* Revision 1.29  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.28  2000/11/30 15:49:39  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.27  2000/11/17 19:48:14  thiessen
* working show/hide alignment row
*
* Revision 1.26  2000/11/12 04:02:59  thiessen
* working file save including alignment edits
*
* Revision 1.25  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.24  2000/11/03 01:12:44  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.23  2000/11/02 16:56:02  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.22  2000/10/19 12:40:54  thiessen
* avoid multiple sequence redraws with scroll set
*
* Revision 1.21  2000/10/17 14:35:06  thiessen
* added row shift - editor basically complete
*
* Revision 1.20  2000/10/16 20:03:07  thiessen
* working block creation
*
* Revision 1.19  2000/10/12 19:20:45  thiessen
* working block deletion
*
* Revision 1.18  2000/10/12 16:22:45  thiessen
* working block split
*
* Revision 1.17  2000/10/12 02:14:56  thiessen
* working block boundary editing
*
* Revision 1.16  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.15  2000/10/04 17:41:30  thiessen
* change highlight color (cell background) handling
*
* Revision 1.14  2000/10/03 18:59:23  thiessen
* added row/column selection
*
* Revision 1.13  2000/10/02 23:25:22  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.12  2000/09/20 22:22:27  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.11  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.10  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.9  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.8  2000/09/11 22:57:33  thiessen
* working highlighting
*
* Revision 1.7  2000/09/11 14:06:29  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:16  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.5  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.4  2000/09/07 17:37:35  thiessen
* minor fixes
*
* Revision 1.3  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.2  2000/08/30 23:46:28  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:48:42  thiessen
* working sequence window
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <wx/wx.h>

#include "cn3d/sequence_viewer.hpp"
#include "cn3d/sequence_viewer_widget.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/vector_math.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/show_hide_dialog.hpp"
#include "cn3d/alignment_manager.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

#include "cn3d/sequence_viewer_private.hpp"

////////////////////////////////////////////////////////////////////////////////
// SequenceViewerWindow is the top-level window that contains the
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SequenceViewerWindow, wxFrame)
    EVT_CLOSE     (                                     SequenceViewerWindow::OnCloseWindow)
    EVT_MENU_RANGE(MID_SHOW_TITLES, MID_HIDE_TITLES,    SequenceViewerWindow::OnTitleView)
    EVT_MENU      (MID_SHOW_HIDE_ROWS,                  SequenceViewerWindow::OnShowHideRows)
    EVT_MENU_RANGE(MID_ENABLE_EDIT, MID_SYNC_STRUCS_ON, SequenceViewerWindow::OnEditMenu)
    EVT_MENU_RANGE(MID_SELECT_RECT, MID_DRAG_HORIZ,     SequenceViewerWindow::OnMouseMode)
    EVT_MENU_RANGE(MID_LEFT,        MID_SPLIT,          SequenceViewerWindow::OnJustification)
END_EVENT_TABLE()

SequenceViewerWindow::SequenceViewerWindow(SequenceViewer *parent) :
    wxFrame(NULL, -1, "Cn3D++ Sequence Viewer", wxPoint(0,500), wxSize(1000,200)),
    viewerWidget(NULL), viewer(parent)
{
    SetSizeHints(200, 150);

    // status bar with a single field
    CreateStatusBar(2);
    int widths[2] = { 150, -1 };
    SetStatusWidths(2, widths);

    viewerWidget = new SequenceViewerWidget(this);
    wxString noHelp("");

    menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_SHOW_TITLES, "&Show Titles");
    //menu->Append(MID_HIDE_TITLES, "&Hide Titles");
    menu->Append(MID_SHOW_HIDE_ROWS, "Show/Hide &Rows");
    menuBar->Append(menu, "&View");

    menu = new wxMenu;
    menu->Append(MID_ENABLE_EDIT, "&Enable Editor", noHelp, true);
    menu->Append(MID_UNDO, "&Undo\tCtrl-Z");
    menu->AppendSeparator();
    menu->Append(MID_SPLIT_BLOCK, "&Split Block", noHelp, true);
    menu->Append(MID_MERGE_BLOCKS, "&Merge Blocks", noHelp, true);
    menu->Append(MID_CREATE_BLOCK, "&Create Block", noHelp, true);
    menu->Append(MID_DELETE_BLOCK, "&Delete Block", noHelp, true);
    menu->AppendSeparator();
    menu->Append(MID_SYNC_STRUCS, "Sync Structure &Colors");
    menu->Append(MID_SYNC_STRUCS_ON, "&Always Sync Structure Colors", noHelp, true);
    menuBar->Append(menu, "&Edit");

    menu = new wxMenu;
    menu->Append(MID_SELECT_RECT, "&Select Rectangle", noHelp, true);
    menu->Append(MID_SELECT_COLS, "Select &Columns", noHelp, true);
    menu->Append(MID_SELECT_ROWS, "Select &Rows", noHelp, true);
    menu->Append(MID_MOVE_ROW, "&Move Row", noHelp, true);
    menu->Append(MID_DRAG_HORIZ, "&Horizontal Drag", noHelp, true);
    menuBar->Append(menu, "&Mouse Mode");

    menu = new wxMenu;
    menu->Append(MID_LEFT, "&Left", noHelp, true);
    menu->Append(MID_RIGHT, "&Right", noHelp, true);
    menu->Append(MID_CENTER, "&Center", noHelp, true);
    menu->Append(MID_SPLIT, "&Split", noHelp, true);
    menuBar->Append(menu, "Unaligned &Justification");

    // set default initial modes
    viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRectangle);
    menuBar->Check(MID_SELECT_RECT, true);
    menuBar->Check(MID_SPLIT, true);
    currentJustification = BlockMultipleAlignment::eSplit;
    viewerWidget->TitleAreaOn();
    menuBar->Check(MID_SYNC_STRUCS_ON, true);
    EnableEditorMenuItems(false);

    SetMenuBar(menuBar);
}

SequenceViewerWindow::~SequenceViewerWindow(void)
{
}

void SequenceViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    viewer->viewerWindow = NULL; // make sure SequenceViewer knows the GUI is gone
    if (!SaveDialog(event.CanVeto())) {
        event.Veto();       // cancelled
        return;
    }
    viewer->RemoveBlockBoundaryRow();
    Destroy();
}

void SequenceViewerWindow::NewAlignment(ViewableAlignment *newAlignment)
{
    viewerWidget->AttachAlignment(newAlignment);
    if (viewer->IsEditableAlignment()) {
        menuBar->Enable(MID_SHOW_HIDE_ROWS, true);
        menuBar->Enable(MID_ENABLE_EDIT, true);
    } else {
        menuBar->Enable(MID_SHOW_HIDE_ROWS, false);
        menuBar->Enable(MID_ENABLE_EDIT, false);
    }
}

void SequenceViewerWindow::UpdateAlignment(ViewableAlignment *alignment)
{
    int vsX, vsY;   // to preserve scroll position
    viewerWidget->GetScroll(&vsX, &vsY);
    viewerWidget->AttachAlignment(alignment, vsX, vsY);
    GlobalMessenger()->PostRedrawSequenceViewers();
}

void SequenceViewerWindow::OnTitleView(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SHOW_TITLES:
            viewerWidget->TitleAreaOn(); break;
        case MID_HIDE_TITLES:
            viewerWidget->TitleAreaOff(); break;
    }
}

void SequenceViewerWindow::OnEditMenu(wxCommandEvent& event)
{
    bool turnEditorOn = menuBar->IsChecked(MID_ENABLE_EDIT);

    switch (event.GetId()) {
        case MID_ENABLE_EDIT:
            if (turnEditorOn) {
                if (!QueryShowAllRows()) { // make sure all rows are visible
                    menuBar->Check(MID_ENABLE_EDIT, false);
                    break;
                }
                TESTMSG("turning on editor");
                viewer->PushAlignment();    // keep copy of original at bottom of the stack
                viewer->AddBlockBoundaryRow();
                Command(MID_DRAG_HORIZ);    // switch to drag mode
            } else {
                if (!SaveDialog(true)) {   // cancelled
                    menuBar->Check(MID_ENABLE_EDIT, true);
                    break;
                }
                TESTMSG("turning off editor");
                viewer->RemoveBlockBoundaryRow();
                if (menuBar->IsChecked(MID_DRAG_HORIZ)) Command(MID_SELECT_RECT);
            }
            EnableEditorMenuItems(turnEditorOn);
            break;

        case MID_UNDO:
            TESTMSG("undoing...");
            viewer->PopAlignment();
            if (AlwaysSyncStructures()) SyncStructures();
            break;

        case MID_SPLIT_BLOCK:
            if (DoCreateBlock()) CreateBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (DoSplitBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                SplitBlockOff();
            break;

        case MID_MERGE_BLOCKS:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (DoMergeBlocks()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns);
            } else
                MergeBlocksOff();
            break;

        case MID_CREATE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (DoCreateBlock()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns);
            } else
                CreateBlockOff();
            break;

        case MID_DELETE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (DoDeleteBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                DeleteBlockOff();
            break;

        case MID_SYNC_STRUCS:
            viewer->RedrawAlignedMolecules();
            break;
    }
}

void SequenceViewerWindow::EnableEditorMenuItems(bool enabled)
{
    int i;
    for (i=MID_SPLIT_BLOCK; i<=MID_SYNC_STRUCS_ON; i++)
        menuBar->Enable(i, enabled);
    menuBar->Enable(MID_DRAG_HORIZ, enabled);
    if (!enabled) {
        if (DoSplitBlock()) SplitBlockOff();
        if (DoMergeBlocks()) MergeBlocksOff();
        if (DoCreateBlock()) CreateBlockOff();
        if (DoDeleteBlock()) DeleteBlockOff();
    }
    menuBar->Enable(MID_UNDO, false);
    menuBar->Enable(MID_SHOW_HIDE_ROWS, !enabled);  // can't show/hide when editor is on
    menuBar->Enable(MID_MOVE_ROW, enabled);         // can only move row when editor is on
}

void SequenceViewerWindow::OnMouseMode(wxCommandEvent& event)
{
    for (int i=MID_SELECT_RECT; i<=MID_DRAG_HORIZ; i++)
        menuBar->Check(i, (i == event.GetId()) ? true : false);

    switch (event.GetId()) {
        case MID_SELECT_RECT:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRectangle); break;
        case MID_SELECT_COLS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns); break;
        case MID_SELECT_ROWS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRows); break;
        case MID_MOVE_ROW:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragVertical); break;
        case MID_DRAG_HORIZ:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragHorizontal); break;
    }
}

void SequenceViewerWindow::OnJustification(wxCommandEvent& event)
{
    for (int i=MID_LEFT; i<=MID_SPLIT; i++)
        menuBar->Check(i, (i == event.GetId()) ? true : false);

    switch (event.GetId()) {
        case MID_LEFT:
            currentJustification = BlockMultipleAlignment::eLeft; break;
        case MID_RIGHT:
            currentJustification = BlockMultipleAlignment::eRight; break;
        case MID_CENTER:
            currentJustification = BlockMultipleAlignment::eCenter; break;
        case MID_SPLIT:
            currentJustification = BlockMultipleAlignment::eSplit; break;
    }
    GlobalMessenger()->PostRedrawSequenceViewers();
}

bool SequenceViewerWindow::SaveDialog(bool canCancel)
{
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
        viewer->SaveAlignment();    // save data
    else
        viewer->RevertAlignment();  // revert to original

    return true;
}

void SequenceViewerWindow::OnShowHideRows(wxCommandEvent& event)
{
    std::vector < const Sequence * > slaveSequences;
    viewer->alignmentManager->GetAlignmentSetSlaveSequences(&slaveSequences);
    wxString *titleStrs = new wxString[slaveSequences.size()];
    for (int i=0; i<slaveSequences.size(); i++)
        titleStrs[i] = slaveSequences[i]->GetTitle().c_str();

    std::vector < bool > visibilities;
    viewer->alignmentManager->GetAlignmentSetSlaveVisibilities(&visibilities);

    wxString title = "Show/Hide Slaves of ";
    title.Append(viewer->alignmentManager->GetCurrentMultipleAlignment()->GetMaster()->GetTitle().c_str());
    ShowHideDialog dialog(
        titleStrs,
        visibilities,
        viewer->alignmentManager,
        NULL, -1, title, wxPoint(400, 50), wxSize(200, 300));
    dialog.Activate();
//    delete titleStrs;    // apparently deleted by wxWindows
}

bool SequenceViewerWindow::QueryShowAllRows(void)
{
    std::vector < bool > visibilities;
    viewer->alignmentManager->GetAlignmentSetSlaveVisibilities(&visibilities);

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
    viewer->alignmentManager->SelectionCallback(visibilities);
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// The sequence view is composed of rows which can be from sequence, alignment,
// or any string - anything derived from DisplayRow, implemented below.
////////////////////////////////////////////////////////////////////////////////

bool DisplayRowFromAlignment::GetCharacterTraitsAt(
    int column, BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color,
    bool *drawBackground, wxColour *cellBackgroundColor) const
{
    bool isHighlighted,
        result = alignment->GetCharacterTraitsAt(column, row, justification, character, color, &isHighlighted);

    if (isHighlighted) {
        *drawBackground = true;
        *cellBackgroundColor = highlightColor;
    } else
        *drawBackground = false;

    return result;
}

bool DisplayRowFromSequence::GetCharacterTraitsAt(
	int column, BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const
{
    if (column >= sequence->sequenceString.size())
        return false;

    *character = tolower(sequence->sequenceString[column]);
    if (sequence->molecule)
        *color = sequence->molecule->GetResidueColor(column);
    else
        color->Set(0,0,0);
    if (GlobalMessenger()->IsHighlighted(sequence, column)) {
        *drawBackground = true;
        *cellBackgroundColor = highlightColor;
    } else {
        *drawBackground = false;
    }

    return true;
}

bool DisplayRowFromSequence::GetSequenceAndIndexAt(
    int column, BlockMultipleAlignment::eUnalignedJustification justification,
    const Sequence **sequenceHandle, int *index) const
{
    if (column >= sequence->sequenceString.size())
        return false;

    *sequenceHandle = sequence;
    *index = column;
    return true;
}

void DisplayRowFromSequence::SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const
{
    int len = sequence->sequenceString.size();

    // skip if selected outside range
    if (from < 0 && to < 0) return;
    if (from >= len && to >= len) return;

    // trim to within range
    if (from < 0) from = 0;
    else if (from >= len) from = len - 1;
    if (to < 0) to = 0;
    else if (to >= len) to = len - 1;

    GlobalMessenger()->AddHighlights(sequence, from, to);
}

bool DisplayRowFromString::GetCharacterTraitsAt(int column,
	BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const
{
    if (column >= theString.size()) return false;

    *character = theString[column];
    *color = stringColor;

    if (hasBackgroundColor) {
        *drawBackground = true;
        // convert vector color to wxColour
        cellBackgroundColor->Set(
            static_cast<unsigned char>((backgroundColor[0] + 0.000001) * 255),
            static_cast<unsigned char>((backgroundColor[1] + 0.000001) * 255),
            static_cast<unsigned char>((backgroundColor[2] + 0.000001) * 255)
        );
    } else {
        *drawBackground = false;
    }
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// The SequenceDisplay is the structure that holds the DisplayRows of the view.
// It's also derived from ViewableAlignment, in order to be plugged into a
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

SequenceDisplay::SequenceDisplay(SequenceViewerWindow * const *parentViewerWindow) :
    maxRowWidth(0), viewerWindow(parentViewerWindow), startingColumn(0)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (int i=0; i<rows.size(); i++) delete rows[i];
}

SequenceDisplay * SequenceDisplay::Clone(BlockMultipleAlignment *newAlignment) const
{
    SequenceDisplay *copy = new SequenceDisplay(viewerWindow);
    for (int row=0; row<rows.size(); row++)
        copy->rows.push_back(rows[row]->Clone(newAlignment));
    copy->startingColumn = startingColumn;
    copy->maxRowWidth = maxRowWidth;
    return copy;
}

void SequenceDisplay::AddRow(DisplayRow *row)
{
    rows.push_back(row);
    if (row->Width() > maxRowWidth) maxRowWidth = row->Width();
}

void SequenceDisplay::PrependRow(DisplayRow *row)
{
    rows.insert(rows.begin(), row);
    if (row->Width() > maxRowWidth) maxRowWidth = row->Width();
}

void SequenceDisplay::RemoveRow(DisplayRow *row)
{
    RowVector::iterator r, re = rows.end();
    for (r=rows.begin(); r!=re; r++) {
        if (*r == row) {
            rows.erase(r);
            delete row;
            break;
        }
    }
    UpdateMaxRowWidth();
}

void SequenceDisplay::UpdateMaxRowWidth(void)
{
    RowVector::iterator r, re = rows.end();
    maxRowWidth = 0;
    for (r=rows.begin(); r!=re; r++)
        if ((*r)->Width() > maxRowWidth) maxRowWidth = (*r)->Width();
}

void SequenceDisplay::AddRowFromAlignment(int row, BlockMultipleAlignment *fromAlignment)
{
    if (!fromAlignment || row < 0 || row >= fromAlignment->NRows()) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromAlignment() failed");
        return;
    }

    AddRow(new DisplayRowFromAlignment(row, fromAlignment));
}

void SequenceDisplay::AddRowFromSequence(const Sequence *sequence)
{
    if (!sequence) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromSequence() failed");
        return;
    }

    AddRow(new DisplayRowFromSequence(sequence));
}

void SequenceDisplay::AddRowFromString(const std::string& anyString)
{
    AddRow(new DisplayRowFromString(anyString));
}

bool SequenceDisplay::GetRowTitle(int row, wxString *title, wxColour *color) const
{
    const DisplayRow *displayRow = rows[row];

    const DisplayRowFromString *strRow = dynamic_cast<const DisplayRowFromString*>(displayRow);
    if (strRow && !strRow->title.empty()) {
        *title = strRow->title.c_str();
        color->Set(0,0,0);      // black
        return true;
    }

    const Sequence *sequence = displayRow->GetSequence();
    if (!sequence) return false;

    // set title
    *title = sequence->GetTitle().c_str();

    // set color
    const DisplayRowFromAlignment *alnRow = dynamic_cast<const DisplayRowFromAlignment*>(displayRow);
    if (alnRow && alnRow->alignment->IsMaster(sequence))
        color->Set(255,0,0);    // red
    else
        color->Set(0,0,0);      // black

    return true;
}

bool SequenceDisplay::GetCharacterTraitsAt(int column, int row,
        char *character, wxColour *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const
{
    if (row < 0 || row > rows.size()) {
        ERR_POST(Warning << "SequenceDisplay::GetCharacterTraitsAt() - row out of range");
        return false;
    }

    const DisplayRow *displayRow = rows[row];

    if (column >= displayRow->Width())
        return false;

    Vector colorVec;
    if (!displayRow->GetCharacterTraitsAt(column,
            (*viewerWindow) ? (*viewerWindow)->GetCurrentJustification() : BlockMultipleAlignment::eLeft,
            character, &colorVec, drawBackground, cellBackgroundColor))
        return false;

    // convert vector color to wxColour
    color->Set(
        static_cast<unsigned char>((colorVec[0] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[1] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[2] + 0.000001) * 255)
    );

    return true;
}

void SequenceDisplay::MouseOver(int column, int row) const
{
    if (*viewerWindow) {
        wxString message;
        if (column >= 0 && row >= 0) {
            const DisplayRow *displayRow = rows[row];
            if (column < displayRow->Width()) {
                const Sequence *sequence;
                int index;
                if (displayRow->GetSequenceAndIndexAt(column,
                        (*viewerWindow)->GetCurrentJustification(), &sequence, &index)) {
                    if (index >= 0) {
                        wxString title;
                        wxColour color;
                        if (GetRowTitle(row, &title, &color)) {
                            message.Printf(", loc %i", index);
                            message = title + message;
                        }
                    }
                }
            }
        }
        (*viewerWindow)->SetStatusText(message, 0);
    }
}

void SequenceDisplay::UpdateAfterEdit(void)
{
    (*viewerWindow)->viewer->UpdateBlockBoundaryRow();
    UpdateMaxRowWidth(); // in case alignment width has changed
    (*viewerWindow)->viewer->PushAlignment();
    if ((*viewerWindow)->AlwaysSyncStructures())
        (*viewerWindow)->SyncStructures();
}

bool SequenceDisplay::MouseDown(int column, int row, unsigned int controls)
{
    TESTMSG("got MouseDown");
    controlDown = ((controls & ViewableAlignment::eControlDown) > 0);

    if (!controlDown && column == -1)
        GlobalMessenger()->RemoveAllHighlights(true);

    if ((*viewerWindow)->viewer->GetCurrentAlignment() && column >= 0) {
        if ((*viewerWindow)->DoSplitBlock()) {
            if ((*viewerWindow)->viewer->GetCurrentAlignment()->SplitBlock(column)) {
                (*viewerWindow)->SplitBlockOff();
                UpdateAfterEdit();
            }
            return false;
        }
        if ((*viewerWindow)->DoDeleteBlock()) {
            if ((*viewerWindow)->viewer->GetCurrentAlignment()->DeleteBlock(column)) {
                (*viewerWindow)->DeleteBlockOff();
                UpdateAfterEdit();
            }
            return false;
        }
    }

    return true;
}

void SequenceDisplay::SelectedRectangle(int columnLeft, int rowTop,
    int columnRight, int rowBottom)
{
    TESTMSG("got SelectedRectangle " << columnLeft << ',' << rowTop << " to "
        << columnRight << ',' << rowBottom);

	BlockMultipleAlignment::eUnalignedJustification justification =
		(*viewerWindow)->GetCurrentJustification();

	if ((*viewerWindow)->viewer->GetCurrentAlignment()) {
        if ((*viewerWindow)->DoMergeBlocks()) {
            if ((*viewerWindow)->viewer->GetCurrentAlignment()->MergeBlocks(columnLeft, columnRight)) {
                (*viewerWindow)->MergeBlocksOff();
                UpdateAfterEdit();
            }
            return;
        }
        if ((*viewerWindow)->DoCreateBlock()) {
            if ((*viewerWindow)->viewer->GetCurrentAlignment()->CreateBlock(columnLeft, columnRight, justification)) {
                (*viewerWindow)->CreateBlockOff();
                UpdateAfterEdit();
            }
            return;
        }
    }

    if (!controlDown)
        GlobalMessenger()->RemoveAllHighlights(true);

    for (int i=rowTop; i<=rowBottom; i++)
        rows[i]->SelectedRange(columnLeft, columnRight, justification);
}

void SequenceDisplay::DraggedCell(int columnFrom, int rowFrom,
    int columnTo, int rowTo)
{
    TESTMSG("got DraggedCell " << columnFrom << ',' << rowFrom << " to "
        << columnTo << ',' << rowTo);
    if (rowFrom == rowTo && columnFrom == columnTo) return;
    if (rowFrom != rowTo && columnFrom != columnTo) return;     // ignore diagonal drag

    if (columnFrom != columnTo) {
        if ((*viewerWindow)->viewer->GetCurrentAlignment()) {
            // process horizontal drag on special block boundary row
            DisplayRowFromString *strRow = dynamic_cast<DisplayRowFromString*>(rows[rowFrom]);
            if (strRow) {
                wxString title;
                wxColour ignored;
                if (GetRowTitle(rowFrom, &title, &ignored) && title == blockBoundaryStringTitle.c_str()) {
                    char ch = strRow->theString[columnFrom];
                    if (ch == blockRightEdgeChar || ch == blockLeftEdgeChar || ch == blockOneColumnChar) {
                        if ((*viewerWindow)->viewer->GetCurrentAlignment()->MoveBlockBoundary(columnFrom, columnTo))
                            UpdateAfterEdit();
                    }
                }
                return;
            }

            // process drag on regular row - block row shift
            DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[rowFrom]);
            if (alnRow) {
                if ((*viewerWindow)->viewer->GetCurrentAlignment()->ShiftRow(alnRow->row, columnFrom, columnTo))
                    UpdateAfterEdit();
                return;
            }
        }
        return;
    }

    // use vertical drag to reorder row; move rowFrom so that it ends up in the 'rowTo' row
    RowVector newRows(rows);
    DisplayRow *row = newRows[rowFrom];
    RowVector::iterator r = newRows.begin();
    int i;
    for (i=0; i<rowFrom; i++) r++; // get iterator for position rowFrom
    newRows.erase(r);
    for (r=newRows.begin(), i=0; i<rowTo; i++) r++; // get iterator for position rowTo
    newRows.insert(r, row);

    // make sure that the master row of an alignment is still first
    bool masterOK = true;
    for (i=0; i<newRows.size(); i++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(newRows[i]);
        if (alnRow) {
            if (alnRow->row != 0) {
                ERR_POST(Warning << "The first alignment row must always be the master sequence");
                masterOK = false;
            }
            break;
        }
    }
    if (masterOK) {
        rows = newRows;
        (*viewerWindow)->viewer->PushAlignment();   // make this an undoable operation
        GlobalMessenger()->PostRedrawSequenceViewers();
    }
}


////////////////////////////////////////////////////////////////////////////////
// the implementation of the SequenceViewer, the interface to the viewer GUI
////////////////////////////////////////////////////////////////////////////////

SequenceViewer::SequenceViewer(AlignmentManager *alnMgr) :
    viewerWindow(NULL), alignmentManager(alnMgr), isEditableAlignment(false)
{
}

SequenceViewer::~SequenceViewer(void)
{
    DestroyGUI();
    ClearStacks();
}

void SequenceViewer::DestroyGUI(void)
{
    if (viewerWindow) {
        viewerWindow->Destroy();
        viewerWindow = NULL;
    }
}

void SequenceViewer::Refresh(void)
{
    if (viewerWindow)
        viewerWindow->Refresh();
    else
        NewDisplay(GetCurrentDisplay());
}

void SequenceViewer::InitStacks(BlockMultipleAlignment *alignment, SequenceDisplay *display)
{
    ClearStacks();
    if (alignment) alignmentStack.push_back(alignment);
    displayStack.push_back(display);
}

// this works like the OpenGL transform stack: when data is to be saved by pushing,
// the current data (display + alignment) is copied and put on the top of the stack,
// becoming the current viewed/edited alignment.
void SequenceViewer::PushAlignment(void)
{
    TESTMSG("SequenceViewer::PushAlignment() - stack size before push: " << displayStack.size());
    if (alignmentStack.size() == 0) {
        ERR_POST(Error << "SequenceViewer::PushAlignment() - can't be called with empty alignment stack");
        return;
    }

    alignmentStack.push_back(alignmentStack.back()->Clone());
    displayStack.push_back(displayStack.back()->Clone(alignmentStack.back()));
    viewerWindow->UpdateAlignment(displayStack.back());
    if (displayStack.size() > 2) viewerWindow->EnableUndo(true);
}

// there can be a miminum of two items on the stack - the bottom is always the original
// before editing; the top, when the editor is on, is always the current data, and just beneath
// the top is a copy of the current data
void SequenceViewer::PopAlignment(void)
{
    if (alignmentStack.size() < 3 || displayStack.size() < 3) {
        ERR_POST(Error << "SequenceViewer::PopAlignment() - no more data to pop off the stack");
        return;
    }

    // top two stack items are identical; delete both...
    for (int i=0; i<2; i++) {
        delete alignmentStack.back();
        alignmentStack.pop_back();
        delete displayStack.back();
        displayStack.pop_back();
    }

    // ... then add a copy of what's underneath, making that copy the current data
    PushAlignment();
    if (!FindBlockBoundaryRow()) AddBlockBoundaryRow();
    if (displayStack.size() == 2) viewerWindow->EnableUndo(false);
}

void SequenceViewer::ClearStacks(void)
{
    while (alignmentStack.size() > 0) {
        delete alignmentStack.back();
        alignmentStack.pop_back();
    }
    while (displayStack.size() > 0) {
        delete displayStack.back();
        displayStack.pop_back();
    }
}

void SequenceViewer::RevertAlignment(void)
{
    // revert to the bottom of the stack
    while (alignmentStack.size() > 1) {
        delete alignmentStack.back();
        alignmentStack.pop_back();
        delete displayStack.back();
        displayStack.pop_back();
    }

    if (viewerWindow) {
        viewerWindow->UpdateAlignment(displayStack.back());
        if (viewerWindow->AlwaysSyncStructures()) viewerWindow->SyncStructures();
    }
}

void SequenceViewer::SaveDialog(void)
{
    if (viewerWindow) viewerWindow->SaveDialog(false);
}

void SequenceViewer::KeepOnlyStackTop(void)
{
    // keep only the top of the stack
    while (alignmentStack.size() > 1) {
        delete alignmentStack.front();
        alignmentStack.pop_front();
        delete displayStack.front();
        displayStack.pop_front();
    }
}

void SequenceViewer::SaveAlignment(void)
{
    KeepOnlyStackTop();

    // go back into the original pairwise alignment data and save according to the
    // current edited BlockMultipleAlignment and display row order
    std::vector < int > rowOrder;
    for (int i=0; i<GetCurrentDisplay()->rows.size(); i++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(GetCurrentDisplay()->rows[i]);
        if (alnRow) rowOrder.push_back(alnRow->row);
    }
    alignmentManager->SavePairwiseFromMultiple(alignmentStack.back(), rowOrder);
}

void SequenceViewer::NewDisplay(SequenceDisplay *display)
{
    if (display) {
        if (!viewerWindow) viewerWindow = new SequenceViewerWindow(this);
        viewerWindow->NewAlignment(display);
		viewerWindow->ScrollToColumn(display->GetStartingColumn());
        viewerWindow->Show(true);
        // ScrollTo causes immediate redraw, so don't need a second one
        GlobalMessenger()->UnPostRedrawSequenceViewers();
    }
}

void SequenceViewer::DisplayAlignment(BlockMultipleAlignment *alignment)
{
    ClearStacks();

    SequenceDisplay *display = new SequenceDisplay(&viewerWindow);
    for (int row=0; row<alignment->NRows(); row++)
        display->AddRowFromAlignment(row, alignment);
    isEditableAlignment = true;

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(alignment->GetFirstAlignedBlockPosition() - 5);

    InitStacks(alignment, display);
    NewDisplay(display);
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
    ClearStacks();

    SequenceDisplay *display = new SequenceDisplay(&viewerWindow);
    // populate each line of the display with one sequence, with blank lines inbetween
    SequenceList::const_iterator s, se = sequenceList->end();
    for (s=sequenceList->begin(); s!=se; s++) {
        if (s != sequenceList->begin()) display->AddRowFromString("");
        display->AddRowFromSequence(*s);
    }
    isEditableAlignment = false;

    InitStacks(NULL, display);
    NewDisplay(display);
}

DisplayRowFromString * SequenceViewer::FindBlockBoundaryRow(void)
{
    SequenceDisplay *display = GetCurrentDisplay();
    if (!display) return NULL;

    DisplayRowFromString *blockBoundaryRow = NULL;
    for (int row=0; row<display->rows.size(); row++) {
        if ((blockBoundaryRow = dynamic_cast<DisplayRowFromString*>(display->rows[row])) != NULL) {
            if (blockBoundaryRow->title == blockBoundaryStringTitle)
                break;
            else
                blockBoundaryRow = NULL;
        }
    }
    return blockBoundaryRow;
}

void SequenceViewer::AddBlockBoundaryRow(void)
{
    SequenceDisplay *display = GetCurrentDisplay();
    if (!display || !GetCurrentAlignment() || !IsEditableAlignment() || FindBlockBoundaryRow()) return;

    DisplayRowFromString *blockBoundaryRow =
        new DisplayRowFromString("", Vector(0,0,0), blockBoundaryStringTitle, true, Vector(0.8,0.8,1));
    display->PrependRow(blockBoundaryRow);
    UpdateBlockBoundaryRow();
    viewerWindow->UpdateAlignment(display);
}

void SequenceViewer::UpdateBlockBoundaryRow(void)
{
    SequenceDisplay *display = GetCurrentDisplay();
    DisplayRowFromString *blockBoundaryRow;
    if (!display || !GetCurrentAlignment() || !IsEditableAlignment() ||
        !(blockBoundaryRow = FindBlockBoundaryRow())) return;

    blockBoundaryRow->theString.resize(GetCurrentAlignment()->AlignmentWidth());

    // fill out block boundary marker string
    int blockColumn, blockWidth;
    for (int i=0; i<GetCurrentAlignment()->AlignmentWidth(); i++) {
        GetCurrentAlignment()->GetAlignedBlockPosition(i, &blockColumn, &blockWidth);
        if (blockColumn >= 0 && blockWidth > 0) {
            if (blockWidth == 1)
                blockBoundaryRow->theString[i] = blockOneColumnChar;
            else if (blockColumn == 0)
                blockBoundaryRow->theString[i] = blockLeftEdgeChar;
            else if (blockColumn == blockWidth - 1)
                blockBoundaryRow->theString[i] = blockRightEdgeChar;
            else
                blockBoundaryRow->theString[i] = blockInsideChar;
        } else
            blockBoundaryRow->theString[i] = ' ';
    }
    GlobalMessenger()->PostRedrawSequenceViewers();
}

void SequenceViewer::RemoveBlockBoundaryRow(void)
{
    SequenceDisplay *display = GetCurrentDisplay();
    DisplayRowFromString *blockBoundaryRow = FindBlockBoundaryRow();
    if (blockBoundaryRow) {
        display->RemoveRow(blockBoundaryRow);
        if (viewerWindow) viewerWindow->UpdateAlignment(display);
    }
}

void SequenceViewer::RedrawAlignedMolecules(void) const
{
    SequenceDisplay *display = GetCurrentDisplay();
    for (int i=0; i<display->rows.size(); i++) {
        const Sequence *sequence = display->rows[i]->GetSequence();
        if (sequence && sequence->molecule)
            GlobalMessenger()->PostRedrawMolecule(sequence->molecule);
    }
}

END_SCOPE(Cn3D)

