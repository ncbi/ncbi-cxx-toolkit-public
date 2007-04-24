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
*      Classes to hold rows in a sequence/alignment display
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.h>

#include <algorithm>

#include "remove_header_conflicts.hpp"

#include "sequence_display.hpp"
#include "viewer_window_base.hpp"
#include "viewer_base.hpp"
#include "molecule.hpp"
#include "messenger.hpp"
#include "structure_set.hpp"
#include "style_manager.hpp"
#include "sequence_viewer_window.hpp"
#include "sequence_viewer.hpp"
#include "alignment_manager.hpp"
#include "cn3d_colors.hpp"
#include "update_viewer.hpp"
#include "update_viewer_window.hpp"
#include "cn3d_tools.hpp"
#include "cn3d_threader.hpp"
#include "conservation_colorer.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_blast.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// block marker string stuff
static const char
    blockLeftEdgeChar = '<',
    blockRightEdgeChar = '>',
    blockOneColumnChar = '^',
    blockInsideChar = '-';
static const string blockBoundaryStringTitle("(blocks)");

// color converter
static inline void Vector2wxColor(const Vector& colorVec, wxColor *colorWX)
{
    colorWX->Set(
        static_cast<unsigned char>((colorVec[0] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[1] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[2] + 0.000001) * 255)
    );
}


////////////////////////////////////////////////////////////////////////////////
// The sequence view is composed of rows which can be from sequence, alignment,
// or any string - anything derived from DisplayRow, implemented below.
////////////////////////////////////////////////////////////////////////////////

bool DisplayRowFromAlignment::GetCharacterTraitsAt(
    unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color,
    bool *drawBackground, Vector *cellBackgroundColor) const
{
    bool isHighlighted,
        result = alignment->GetCharacterTraitsAt(
            column, row, justification, character, color,
            &isHighlighted, drawBackground, cellBackgroundColor);

    // always apply highlight color, even if alignment has set its own background color
    if (isHighlighted) {
        *drawBackground = true;
        *cellBackgroundColor = GlobalColors()->Get(Colors::eHighlight);
    }

    return result;
}

DisplayRowFromSequence::DisplayRowFromSequence(const Sequence *s, unsigned int from, unsigned int to) :
    sequence(s), fromIndex(from), toIndex(to)
{
    if (from >= sequence->Length() || from > to || to >= sequence->Length())
        ERRORMSG("DisplayRowFromSequence::DisplayRowFromSequence() - from/to indexes out of range");
}

bool DisplayRowFromSequence::GetCharacterTraitsAt(
    unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const
{
    unsigned int index = column + fromIndex;
    if (index > toIndex)
        return false;

    *character = tolower((unsigned char) sequence->sequenceString[index]);
    if (sequence->molecule)
        *color = sequence->molecule->GetResidueColor(index);
    else
        color->Set(0,0,0);
    if (GlobalMessenger()->IsHighlighted(sequence, index)) {
        *drawBackground = true;
        *cellBackgroundColor = GlobalColors()->Get(Colors::eHighlight);
    } else {
        *drawBackground = false;
    }

    return true;
}

bool DisplayRowFromSequence::GetSequenceAndIndexAt(
    unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
    const Sequence **sequenceHandle, int *seqIndex) const
{
    unsigned int index = column + fromIndex;
    if (index > toIndex)
        return false;

    *sequenceHandle = sequence;
    *seqIndex = index;
    return true;
}

void DisplayRowFromSequence::SelectedRange(unsigned int from, unsigned int to,
    BlockMultipleAlignment::eUnalignedJustification justification, bool toggle) const
{
    from += fromIndex;
    to += fromIndex;

    // skip if selected outside range
    if (from < fromIndex && to < fromIndex) return;
    if (from > toIndex && to > toIndex) return;

    // trim to within range
    if (from < fromIndex) from = fromIndex;
    else if (from > toIndex) from = toIndex;
    if (to < fromIndex) to = fromIndex;
    else if (to > toIndex) to = toIndex;

    if (toggle)
        GlobalMessenger()->ToggleHighlights(sequence, from, to);
    else
        GlobalMessenger()->AddHighlights(sequence, from, to);
}

bool DisplayRowFromString::GetCharacterTraitsAt(unsigned int column,
	BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const
{
    if (column >= theString.size()) return false;

    *character = theString[column];
    *color = stringColor;

    if (hasBackgroundColor) {
        *drawBackground = true;
        *cellBackgroundColor = backgroundColor;
    } else {
        *drawBackground = false;
    }
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// The SequenceDisplay is the structure that holds the DisplayRows of the view.
// It's also derived from ViewableAlignment, in order to be plugged into a
// SequenceDisplayWidget.
////////////////////////////////////////////////////////////////////////////////

SequenceDisplay::SequenceDisplay(bool editable, ViewerWindowBase* const *parentViewerWindow) :
    isEditable(editable), viewerWindow(parentViewerWindow), startingColumn(0), maxRowWidth(0)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (unsigned int i=0; i<rows.size(); ++i) delete rows[i];
}

void SequenceDisplay::Empty(void)
{
    for (unsigned int i=0; i<rows.size(); ++i) delete rows[i];
    rows.clear();
    startingColumn = maxRowWidth = 0;
}


SequenceDisplay * SequenceDisplay::Clone(const Old2NewAlignmentMap& newAlignments) const
{
    SequenceDisplay *copy = new SequenceDisplay(isEditable, viewerWindow);
    for (unsigned int row=0; row<rows.size(); ++row)
        copy->rows.push_back(rows[row]->Clone(newAlignments));
    copy->startingColumn = startingColumn;
    copy->maxRowWidth = maxRowWidth;
    return copy;
}

void SequenceDisplay::AddRow(DisplayRow *row)
{
    rows.push_back(row);
    if (row->Width() > maxRowWidth) maxRowWidth = row->Width();
}

BlockMultipleAlignment * SequenceDisplay::GetAlignmentForRow(unsigned int row) const
{
    if (row >= rows.size()) return NULL;
    const DisplayRowFromAlignment *displayRow = dynamic_cast<const DisplayRowFromAlignment*>(rows[row]);
    if (displayRow) return displayRow->alignment;
    const DisplayRowFromString *stringRow = dynamic_cast<const DisplayRowFromString*>(rows[row]);
    if (stringRow) return stringRow->alignment;
    return NULL;
}

void SequenceDisplay::UpdateMaxRowWidth(void)
{
    RowVector::iterator r, re = rows.end();
    maxRowWidth = 0;
    for (r=rows.begin(); r!=re; ++r)
        if ((*r)->Width() > maxRowWidth) maxRowWidth = (*r)->Width();
}

void SequenceDisplay::AddRowFromAlignment(unsigned int row, BlockMultipleAlignment *fromAlignment)
{
    if (!fromAlignment || row >= fromAlignment->NRows()) {
        ERRORMSG("SequenceDisplay::AddRowFromAlignment() failed");
        return;
    }

    AddRow(new DisplayRowFromAlignment(row, fromAlignment));
}

void SequenceDisplay::AddRowFromSequence(const Sequence *sequence, unsigned int from, unsigned int to)
{
    if (!sequence) {
        ERRORMSG("SequenceDisplay::AddRowFromSequence() failed");
        return;
    }

    AddRow(new DisplayRowFromSequence(sequence, from, to));
}

void SequenceDisplay::AddRowFromString(const string& anyString)
{
    AddRow(new DisplayRowFromString(anyString));
}

bool SequenceDisplay::GetRowTitle(unsigned int row, wxString *title, wxColour *color) const
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
    *title = sequence->identifier->ToString().c_str();

    // set color - by object if there's an associated molecule
    const DisplayRowFromAlignment *alnRow = dynamic_cast<const DisplayRowFromAlignment*>(displayRow);
    const Molecule *molecule;
    if (alnRow && (molecule=alnRow->alignment->GetSequenceOfRow(alnRow->row)->molecule) != NULL)
        Vector2wxColor(molecule->parentSet->styleManager->GetObjectColor(molecule), color);
    else
        color->Set(0,0,0);      // ... black otherwise

    return true;
}

bool SequenceDisplay::GetCharacterTraitsAt(unsigned int column, unsigned int row,
        char *character, wxColour *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const
{
    if (row >= rows.size()) {
        WARNINGMSG("SequenceDisplay::GetCharacterTraitsAt() - row out of range");
        return false;
    }

    const DisplayRow *displayRow = rows[row];

    if (column >= displayRow->Width())
        return false;

    Vector colorVec, bgColorVec;
    if (!displayRow->GetCharacterTraitsAt(column,
            (*viewerWindow) ? (*viewerWindow)->GetCurrentJustification() : BlockMultipleAlignment::eLeft,
            character, &colorVec, drawBackground, &bgColorVec))
        return false;

    Vector2wxColor(colorVec, color);
    if (*drawBackground) Vector2wxColor(bgColorVec, cellBackgroundColor);
    return true;
}

void SequenceDisplay::MouseOver(int column, int row) const
{
    if (*viewerWindow) {
        wxString idLoc, status;

        if (row >= 0 && row < (int)rows.size()) {
            const DisplayRow *displayRow = rows[row];
            const Sequence *sequence = NULL;

            // show id if we're in sequence area
            if (column >= 0 && column < (int)displayRow->Width()) {

                // show title and seqloc
                int index = -1;
                if (displayRow->GetSequenceAndIndexAt(column,
                        (*viewerWindow)->GetCurrentJustification(), &sequence, &index)) {
                    if (index >= 0) {
                        wxString title;
                        wxColour color;
                        if (GetRowTitle(row, &title, &color))
                            idLoc.Printf("%s, loc %i", title.c_str(), index + 1); // show one-based numbering
                    }
                }

                // show PDB residue number if available (assume resID = index+1)
                if (sequence && index >= 0 && sequence->molecule) {
                    const Residue *residue = sequence->molecule->residues.find(index + 1)->second;
                    if (residue && residue->namePDB.size() > 0) {
                        wxString n = residue->namePDB.c_str();
                        n = n.Strip(wxString::both);
                        if (n.size() > 0)
                            idLoc = idLoc + " (PDB " + n + ")";
                    }
                }

                // show alignment block number and row-wise string
                const DisplayRowFromAlignment *alnRow =
                    dynamic_cast<const DisplayRowFromAlignment *>(displayRow);
                if (alnRow) {
                    int blockNum = alnRow->alignment->GetAlignedBlockNumber(column);
                    int rowNum = 0;
                    // get apparent row number (crude!)
                    for (int r=0; r<=row; ++r) {
                        const DisplayRowFromAlignment *aRow =
                            dynamic_cast<const DisplayRowFromAlignment *>(rows[r]);
                        if (aRow && aRow->alignment == alnRow->alignment) ++rowNum;
                    }
                    if (blockNum > 0)
                        status.Printf("Block %i, Row %i", blockNum, rowNum);
                    else
                        status.Printf("Row %i", rowNum);
                    if (alnRow->alignment->GetRowStatusLine(alnRow->row).size() > 0) {
                        status += " ; ";
                        status += alnRow->alignment->GetRowStatusLine(alnRow->row).c_str();
                    }
                }
            }

            // else show length and description if we're in the title area
            else if (column < 0) {
                sequence = displayRow->GetSequence();
                if (sequence) {
                    idLoc.Printf("length %i", sequence->Length());
                    status = sequence->GetDescription().c_str();
                }
            }
        }

        (*viewerWindow)->SetStatusText(idLoc, 0);
        (*viewerWindow)->SetStatusText(status, 1);
    }
}

void SequenceDisplay::UpdateAfterEdit(const BlockMultipleAlignment *forAlignment)
{
    UpdateBlockBoundaryRow(forAlignment);
    UpdateMaxRowWidth(); // in case alignment width has changed
    (*viewerWindow)->viewer->Save();    // make undoable
    (*viewerWindow)->UpdateDisplay(this);
    if ((*viewerWindow)->AlwaysSyncStructures())
        (*viewerWindow)->SyncStructures();
}

bool SequenceDisplay::MouseDown(int column, int row, unsigned int controls)
{
    TRACEMSG("got MouseDown at col:" << column << " row:" << row);

    // process events in title area (launch of browser for entrez page on a sequence)
    if (column < 0 && row >= 0 && row < (int)NRows()) {
        const Sequence *seq = rows[row]->GetSequence();
        if (seq) seq->LaunchWebBrowserWithInfo();
        return false;
    }

    // check special keys
    shiftDown = ((controls & ViewableAlignment::eShiftDown) > 0);
    controlDown = ((controls &
#ifdef __WXMAC__
        // on Mac, can't do ctrl-clicks, so use meta instead
        ViewableAlignment::eAltOrMetaDown
#else
        ViewableAlignment::eControlDown
#endif
            ) > 0);

    // clear highlights if ctrl-click is in whitespace below alignment
    if (controlDown && !shiftDown && column == -1)
        GlobalMessenger()->RemoveAllHighlights(true);

    // process events in sequence area
    BlockMultipleAlignment *alignment = GetAlignmentForRow(row);
    if (alignment && column >= 0) {

        // operations for any viewer window
        if ((*viewerWindow)->DoSplitBlock()) {
            if (alignment->SplitBlock(column)) {
                if (!controlDown) (*viewerWindow)->SplitBlockOff();
                UpdateAfterEdit(alignment);
            }
            return false;
        }

        if ((*viewerWindow)->DoDeleteBlock()) {
            if (alignment->DeleteBlock(column)) {
                if (!controlDown) (*viewerWindow)->DeleteBlockOff();
                UpdateAfterEdit(alignment);
            }
            return false;
        }

        // operations specific to the sequence window
        SequenceViewerWindow *sequenceWindow = dynamic_cast<SequenceViewerWindow*>(*viewerWindow);
        if (sequenceWindow && row >= 0) {

            if (sequenceWindow->DoRestrictHighlights()) {
                DisplayRowFromAlignment *selectedRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
                if (selectedRow) {
                    GlobalMessenger()->KeepHighlightsOnlyOnSequence(alignment->GetSequenceOfRow(selectedRow->row));
                    sequenceWindow->RestrictHighlightsOff();
                }
                return false;
            }

            if (sequenceWindow->DoMarkBlock()) {
                if (alignment->MarkBlock(column)) {
                    if (!controlDown) sequenceWindow->MarkBlockOff();
                    GlobalMessenger()->PostRedrawSequenceViewer(sequenceWindow->sequenceViewer);
                }
                return false;
            }

            if (sequenceWindow->DoProximitySort()) {
                if (ProximitySort(row)) {
                    sequenceWindow->ProximitySortOff();
                    GlobalMessenger()->PostRedrawSequenceViewer(sequenceWindow->sequenceViewer);
                }
                return false;
            }

            if (sequenceWindow->DoSortLoops()) {
                if (SortRowsByLoopLength(row, column)) {
                    sequenceWindow->SortLoopsOff();
                    GlobalMessenger()->PostRedrawSequenceViewer(sequenceWindow->sequenceViewer);
                }
                return false;
            }

            if (sequenceWindow->DoRealignRow() || sequenceWindow->DoDeleteRow()) {
                DisplayRowFromAlignment *selectedRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
                if (!selectedRow || selectedRow->row == 0 || !selectedRow->alignment) {
                    WARNINGMSG("Can't delete/realign that row...");
                    return false;
                }

                if (sequenceWindow->DoRealignRow()) {
                    vector < unsigned int > selectedDependents(1);
                    selectedDependents[0] = selectedRow->row;
                    sequenceWindow->sequenceViewer->alignmentManager->RealignDependentSequences(alignment, selectedDependents);
                    if (!controlDown) sequenceWindow->RealignRowOff();
                    return false;
                }

                if (sequenceWindow->DoDeleteRow()) {
                    if (alignment->NRows() <= 2) {
                        WARNINGMSG("Can't delete that row...");
                        return false;
                    }

                    // in case we need to redraw molecule associated with removed row
                    const Molecule *molecule = alignment->GetSequenceOfRow(selectedRow->row)->molecule;

                    // delete row based on alignment row # (not display row #); redraw molecule
                    if (alignment->DeleteRow(selectedRow->row)) {

                        // delete this row from the display, and update higher row #'s
                        RowVector::iterator r, re = rows.end(), toDelete;
                        for (r=rows.begin(); r!=re; ++r) {
                            DisplayRowFromAlignment
                                *currentARow = dynamic_cast<DisplayRowFromAlignment*>(*r);
                            if (!currentARow)
                                continue;
                            else if (currentARow->row > selectedRow->row)
                                (currentARow->row)--;
                            else if (currentARow == selectedRow)
                                toDelete = r;
                        }
                        delete *toDelete;
                        rows.erase(toDelete);

                        if (!controlDown) sequenceWindow->DeleteRowOff();
                        UpdateAfterEdit(alignment);
                        if (molecule && sequenceWindow->AlwaysSyncStructures())
                            GlobalMessenger()->PostRedrawMolecule(molecule);
                    }
                    return false;
                }
            }
        }

        // operations specific to the update window
        UpdateViewerWindow *updateWindow = dynamic_cast<UpdateViewerWindow*>(*viewerWindow);
        if (updateWindow && row >= 0) {

            // delete all blocks
            if (updateWindow->DoDeleteAllBlocks()) {
                if (alignment->DeleteAllBlocks()) {
                    if (!controlDown) updateWindow->DeleteAllBlocksOff();
                    UpdateAfterEdit(alignment);
                }
                return false;
            }

            // thread single
            if (updateWindow->DoThreadSingle()) {
                if (!updateWindow->updateViewer->alignmentManager->GetCurrentMultipleAlignment()) {
                    ERRORMSG("Can't run threader without existing core alignment");
                    return false;
                }
                // get threader options
                globalThreaderOptions.nRandomStarts = Threader::EstimateNRandomStarts(
                    updateWindow->updateViewer->alignmentManager->GetCurrentMultipleAlignment(),
                    alignment);
                ThreaderOptionsDialog optDialog(updateWindow, globalThreaderOptions);
                if (optDialog.ShowModal() == wxCANCEL) return false;  // user cancelled
                if (!optDialog.GetValues(&globalThreaderOptions)) {
                    ERRORMSG("Error retrieving options values from dialog");
                    return false;
                }
                updateWindow->updateViewer->alignmentManager->ThreadUpdate(globalThreaderOptions, alignment);
                updateWindow->ThreadSingleOff();
                return false;
            }

            // BLAST single
            if (updateWindow->DoBlastSingle()) {
                updateWindow->updateViewer->BlastUpdate(alignment, false);
                if (!controlDown) updateWindow->BlastSingleOff();
                return false;
            }

            // BLAST/PSSM single
            if (updateWindow->DoBlastPSSMSingle()) {
                updateWindow->updateViewer->BlastUpdate(alignment, true);
                if (!controlDown) updateWindow->BlastPSSMSingleOff();
                return false;
            }

            // BLAST neighbor single
            if (updateWindow->DoBlastNeighborSingle()) {
                updateWindow->updateViewer->BlastNeighbor(alignment);
                if (!controlDown) updateWindow->BlastNeighborSingleOff();
                return false;
            }

            // Block align single
            if (updateWindow->DoBlockAlignSingle()) {
                updateWindow->updateViewer->alignmentManager->BlockAlignUpdate(alignment);
                if (!controlDown) updateWindow->BlockAlignSingleOff();
                return false;
            }

            // Block extend single
            if (updateWindow->DoExtendSingle()) {
                updateWindow->updateViewer->alignmentManager->ExtendUpdate(alignment);
                if (!controlDown) updateWindow->ExtendSingleOff();
                return false;
            }

            // set region (on dependent sequence)
            if (updateWindow->DoSetRegion()) {
                // dialog uses 1-based sequence locations
                RegionDialog dialog(updateWindow,
                    alignment->GetSequenceOfRow(1), alignment->alignDependentFrom + 1, alignment->alignDependentTo + 1);
                if (dialog.ShowModal() == wxOK) {
                    int from, to;
                    if (!dialog.GetValues(&from, &to)) {
                        ERRORMSG("RegionDialog returned OK, but values invalid");
                    } else {
                        TRACEMSG("set region (dependent): " << from << " to " << to);
                        alignment->alignDependentFrom = from - 1;
                        alignment->alignDependentTo = to - 1;
                    }
                    if (!controlDown) updateWindow->SetRegionOff();
                }
                return false;
            }

            // merge single
            if (updateWindow->DoMergeSingle()) {
                AlignmentManager::UpdateMap single;
                single[alignment] = true;
                updateWindow->updateViewer->alignmentManager->MergeUpdates(single, false);
                if (!controlDown) updateWindow->MergeSingleOff();
                return false;
            }

            // merge neighbor
            if (updateWindow->DoMergeNeighbor()) {
                AlignmentManager::UpdateMap single;
                single[alignment] = true;
                updateWindow->updateViewer->alignmentManager->MergeUpdates(single, true);
                if (!controlDown) updateWindow->MergeNeighborOff();
                return false;
            }

            // delete single
            if (updateWindow->DoDeleteSingle()) {
                updateWindow->updateViewer->DeleteAlignment(alignment);
                if (!controlDown) updateWindow->DeleteSingleOff();
                return false;
            }
        }
    }

    return true;
}

void SequenceDisplay::SelectedRectangle(int columnLeft, int rowTop,
    int columnRight, int rowBottom)
{
    TRACEMSG("got SelectedRectangle " << columnLeft << ',' << rowTop << " to "
        << columnRight << ',' << rowBottom);

	BlockMultipleAlignment::eUnalignedJustification justification =
		(*viewerWindow)->GetCurrentJustification();

    BlockMultipleAlignment *alignment = GetAlignmentForRow(rowTop);
    bool singleAlignment = (alignment && alignment == GetAlignmentForRow(rowBottom));
    if (singleAlignment) {
        if ((*viewerWindow)->DoMergeBlocks()) {
            if (alignment->MergeBlocks(columnLeft, columnRight)) {
                if (!controlDown) (*viewerWindow)->MergeBlocksOff();
                UpdateAfterEdit(alignment);
            }
            return;
        }
        if ((*viewerWindow)->DoCreateBlock()) {
            if (alignment->CreateBlock(columnLeft, columnRight, justification)) {
                if (!controlDown) (*viewerWindow)->CreateBlockOff();
                UpdateAfterEdit(alignment);
            }
            return;
        }
    }

    if (!shiftDown && !controlDown)
        GlobalMessenger()->RemoveAllHighlights(true);

    if (singleAlignment && (*viewerWindow)->SelectBlocksIsOn()) {
        alignment->SelectBlocks(columnLeft, columnRight, controlDown);
    } else {
        for (int i=rowTop; i<=rowBottom; ++i)
            rows[i]->SelectedRange(columnLeft, columnRight, justification, controlDown); // toggle if control down
    }
}

void SequenceDisplay::DraggedCell(int columnFrom, int rowFrom,
    int columnTo, int rowTo)
{
    // special shift operations if shift/ctrl click w/ no drag
    if (columnFrom == columnTo && rowFrom == rowTo) {
        BlockMultipleAlignment *alignment = GetAlignmentForRow(rowFrom);
        if (alignment) {
            int alnBlockNum = alignment->GetAlignedBlockNumber(columnFrom);
            if (alnBlockNum > 0) {
                // operations on aligned blocks
                if (shiftDown && !controlDown && columnTo > 0)                  // shift left
                    --columnTo;
                else if (controlDown && !shiftDown && columnTo < (int)maxRowWidth-1) // shift right
                    ++columnTo;
                else if (shiftDown && controlDown) {                            // optimize block
                    DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[rowFrom]);
                    if (alnRow && alignment->OptimizeBlock(alnRow->row, columnFrom, (*viewerWindow)->GetCurrentJustification()))
                        UpdateAfterEdit(alignment);
                    return;
                }
            } else {
                // operations on unaligned blocks
                if ((shiftDown && !controlDown) || (controlDown && !shiftDown)) {
                    DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[rowFrom]);
                    if (alnRow && alignment->ZipAlignResidue(alnRow->row, columnFrom,
                            controlDown, (*viewerWindow)->GetCurrentJustification()))
                        UpdateAfterEdit(alignment);
                    return;
                }
            }
        }
    }

    TRACEMSG("got DraggedCell " << columnFrom << ',' << rowFrom << " to "
        << columnTo << ',' << rowTo);
    if (rowFrom == rowTo && columnFrom == columnTo) return;
    if (rowFrom != rowTo && columnFrom != columnTo) return;     // ignore diagonal drag

    if (columnFrom != columnTo) {
        BlockMultipleAlignment *alignment = GetAlignmentForRow(rowFrom);
        if (alignment) {
            // process horizontal drag on special block boundary row
            DisplayRowFromString *strRow = dynamic_cast<DisplayRowFromString*>(rows[rowFrom]);
            if (strRow) {
                wxString title;
                wxColour ignored;
                if (GetRowTitle(rowFrom, &title, &ignored) && title == blockBoundaryStringTitle.c_str()) {
                    char ch = strRow->theString[columnFrom];
                    if (ch == blockRightEdgeChar || ch == blockLeftEdgeChar || ch == blockOneColumnChar) {
                        if (alignment->MoveBlockBoundary(columnFrom, columnTo))
                            UpdateAfterEdit(alignment);
                    }
                }
                return;
            }

            // process drag on regular row - block row shift
            DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[rowFrom]);
            if (alnRow) {
                if (alignment->ShiftRow(alnRow->row, columnFrom, columnTo,
                        (*viewerWindow)->GetCurrentJustification()))
                    UpdateAfterEdit(alignment);
                return;
            }
        }
        return;
    }

    // use vertical drag to reorder row; move rowFrom so that it ends up just before the
    // initial rowTo row
    if (rowFrom == rowTo - 1) return;
    if (rowTo > rowFrom) --rowTo;
    RowVector newRows(rows);
    DisplayRow *row = newRows[rowFrom];
    RowVector::iterator r = newRows.begin();
    int i;
    for (i=0; i<rowFrom; ++i) ++r; // get iterator for position rowFrom
    newRows.erase(r);
    for (r=newRows.begin(), i=0; i<rowTo; ++i) ++r; // get iterator for position rowTo
    newRows.insert(r, row);

    // make sure that the master row of an alignment is still first
    bool masterOK = true;
    for (i=0; i<(int)newRows.size(); ++i) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(newRows[i]);
        if (alnRow) {
            if (alnRow->row != 0) {
                WARNINGMSG("The first alignment row must always be the master sequence");
                masterOK = false;
            }
            break;
        }
    }
    if (masterOK) {
        rows = newRows;
        (*viewerWindow)->UpdateDisplay(this);
        (*viewerWindow)->viewer->Save();   // make this an undoable operation
        GlobalMessenger()->PostRedrawAllSequenceViewers();
    }
}

void SequenceDisplay::RedrawAlignedMolecules(void) const
{
    for (unsigned int i=0; i<rows.size(); ++i) {
        const Sequence *sequence = rows[i]->GetSequence();
        if (sequence && sequence->molecule)
            GlobalMessenger()->PostRedrawMolecule(sequence->molecule);
    }
}

DisplayRowFromString * SequenceDisplay::FindBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const
{
    DisplayRowFromString *blockBoundaryRow = NULL;
    for (unsigned int row=0; row<rows.size(); ++row) {
        if ((blockBoundaryRow = dynamic_cast<DisplayRowFromString*>(rows[row])) != NULL) {
            if (blockBoundaryRow->alignment == forAlignment &&
                blockBoundaryRow->title == blockBoundaryStringTitle)
                break;
            else
                blockBoundaryRow = NULL;
        }
    }
    return blockBoundaryRow;
}

static inline DisplayRowFromString * CreateBlockBoundaryRow(BlockMultipleAlignment *forAlignment)
{
    return new DisplayRowFromString("", Vector(0,0,0),
        blockBoundaryStringTitle, true, Vector(0.8,0.8,1), forAlignment);
}

void SequenceDisplay::AddBlockBoundaryRows(void)
{
    if (!IsEditable()) return;

    // find alignment master rows
    unsigned int i = 0;
    map < const BlockMultipleAlignment * , bool > doneForAlignment;
    do {
        RowVector::iterator r;
        for (r=rows.begin(), i=0; i<rows.size(); ++r, ++i) {
            DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(*r);
            if (!alnRow || alnRow->row != 0 || !alnRow->alignment ||
                doneForAlignment.find(alnRow->alignment) != doneForAlignment.end()) continue;

            // insert block row before each master row
            DisplayRowFromString *blockBoundaryRow = CreateBlockBoundaryRow(alnRow->alignment);
            UpdateBlockBoundaryRow(blockBoundaryRow);
            rows.insert(r, blockBoundaryRow);
            doneForAlignment[alnRow->alignment] = true;
            break;  // insert on vector can cause invalidation of iterators, so start over
        }
    } while (i < rows.size());

    if (*viewerWindow) (*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::AddBlockBoundaryRow(BlockMultipleAlignment *forAlignment)
{
    DisplayRowFromString *blockBoundaryRow = CreateBlockBoundaryRow(forAlignment);
    AddRow(blockBoundaryRow);
    UpdateBlockBoundaryRow(blockBoundaryRow);
}

void SequenceDisplay::UpdateBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const
{
    DisplayRowFromString *blockBoundaryRow;
    if (!IsEditable() || (blockBoundaryRow = FindBlockBoundaryRow(forAlignment)) == NULL ||
        !blockBoundaryRow->alignment) return;
    UpdateBlockBoundaryRow(blockBoundaryRow);
}

void SequenceDisplay::UpdateBlockBoundaryRow(DisplayRowFromString *blockBoundaryRow) const
{
    if (!IsEditable() || !blockBoundaryRow || !blockBoundaryRow->alignment) return;

    unsigned int alignmentWidth = blockBoundaryRow->alignment->AlignmentWidth();
    blockBoundaryRow->theString.resize(alignmentWidth);

    // fill out block boundary marker string
    int blockColumn, blockWidth;
    for (unsigned int i=0; i<alignmentWidth; ++i) {
        blockBoundaryRow->alignment->GetAlignedBlockPosition(i, &blockColumn, &blockWidth);
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
    if (*viewerWindow) GlobalMessenger()->PostRedrawSequenceViewer((*viewerWindow)->viewer);
}

void SequenceDisplay::RemoveBlockBoundaryRows(void)
{
    vector < bool > toRemove(rows.size(), false);
    unsigned int nToRemove = 0;
    for (unsigned int row=0; row<rows.size(); ++row) {
        DisplayRowFromString *blockBoundaryRow = dynamic_cast<DisplayRowFromString*>(rows[row]);
        if (blockBoundaryRow && blockBoundaryRow->title == blockBoundaryStringTitle) {
            delete blockBoundaryRow;
            toRemove[row] = true;
            ++nToRemove;
        }
    }
    VectorRemoveElements(rows, toRemove, nToRemove);
    UpdateMaxRowWidth();
    if (*viewerWindow) (*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::GetProteinSequences(SequenceList *seqs) const
{
    seqs->clear();
    for (unsigned int row=0; row<rows.size(); ++row) {
        const Sequence *seq = rows[row]->GetSequence();
        if (seq && seq->isProtein)
            seqs->push_back(seq);
    }
}

void SequenceDisplay::GetSequences(const BlockMultipleAlignment *forAlignment, SequenceList *seqs) const
{
    seqs->clear();
    for (unsigned int row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->alignment == forAlignment)
            seqs->push_back(alnRow->alignment->GetSequenceOfRow(alnRow->row));
    }
}

void SequenceDisplay::GetRowOrder(
    const BlockMultipleAlignment *forAlignment, vector < unsigned int > *dependentRowOrder) const
{
    dependentRowOrder->clear();
    for (unsigned int row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->alignment == forAlignment)
            dependentRowOrder->push_back(alnRow->row);
    }
    if (dependentRowOrder->size() != forAlignment->NRows() || dependentRowOrder->front() != 0)
        ERRORMSG("SequenceDisplay::GetRowOrder() - row order vector problem");
}

// comparison function: if CompareRows(a, b) == true, then row a moves up
typedef bool (*CompareRows)(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b);

static bool CompareRowsByIdentifier(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return MoleculeIdentifier::CompareIdentifiers(
        a->alignment->GetSequenceOfRow(a->row)->identifier,
        b->alignment->GetSequenceOfRow(b->row)->identifier);
}

static bool CompareRowsByScore(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return (a->alignment->GetRowDouble(a->row) > b->alignment->GetRowDouble(b->row));
}

static bool CompareRowsByEValue(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return ((a->alignment->GetRowDouble(a->row) >= 0.0 &&
             a->alignment->GetRowDouble(a->row) < b->alignment->GetRowDouble(b->row)) ||
            b->alignment->GetRowDouble(b->row) < 0.0);
}

static bool CompareRowsFloatPDB(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return (a->alignment->GetSequenceOfRow(a->row)->identifier->pdbID.size() > 0 &&
            b->alignment->GetSequenceOfRow(b->row)->identifier->pdbID.size() == 0);
}

static bool CompareRowsFloatHighlights(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return (GlobalMessenger()->IsHighlightedAnywhere(a->alignment->GetSequenceOfRow(a->row)->identifier) &&
            !GlobalMessenger()->IsHighlightedAnywhere(b->alignment->GetSequenceOfRow(b->row)->identifier));
}

static Threader::GeometryViolationsForRow violations;
static bool CompareRowsFloatGV(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return (violations[a->row].size() > 0 && violations[b->row].size() == 0);
}

static CompareRows rowComparisonFunction = NULL;

void SequenceDisplay::SortRowsByIdentifier(void)
{
    rowComparisonFunction = CompareRowsByIdentifier;
    SortRows();
}

void SequenceDisplay::SortRowsByThreadingScore(double weightPSSM)
{
    if (!CalculateRowScoresWithThreader(weightPSSM)) return;
    rowComparisonFunction = CompareRowsByScore;
    SortRows();
}

void SequenceDisplay::FloatPDBRowsToTop(void)
{
    rowComparisonFunction = CompareRowsFloatPDB;
    SortRows();
}

void SequenceDisplay::FloatHighlightsToTop(void)
{
    rowComparisonFunction = CompareRowsFloatHighlights;
    SortRows();
}

void SequenceDisplay::FloatGVToTop(void)
{
    DisplayRowFromAlignment *alnRow = NULL;
    for (unsigned int row=0; row<rows.size(); ++row) {
        alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow)
            break;
    }
    if (!alnRow) {
        ERRORMSG("SequenceDisplay::FloatGVToTop() - can't get alignment");
        return;
    }

    if ((*viewerWindow)->viewer->alignmentManager->threader->
            GetGeometryViolations(alnRow->alignment, &violations) == 0)
        return;
    if (!(*viewerWindow)->menuBar->IsChecked(ViewerWindowBase::MID_SHOW_GEOM_VLTNS))
        (*viewerWindow)->ProcessCommand(ViewerWindowBase::MID_SHOW_GEOM_VLTNS);

    rowComparisonFunction = CompareRowsFloatGV;
    SortRows();
}

void SequenceDisplay::SortRowsBySelfHit(void)
{
    // get alignment
    for (unsigned int row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow) {
            // do self-hit calculation
            (*viewerWindow)->viewer->CalculateSelfHitScores(alnRow->alignment);

            // then sort by score
            rowComparisonFunction = CompareRowsByEValue;
            SortRows();
            return;
        }
    }
}

bool SequenceDisplay::SortRowsByLoopLength(unsigned int row, unsigned int alnIndex)
{
    DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
    if (!alnRow)
        return false;

    for (unsigned int r=0; r<alnRow->alignment->NRows(); ++r) {
        int loop = alnRow->alignment->GetLoopLength(r, alnIndex);
        if (loop < 0)   // means this column is aligned
            return false;
        alnRow->alignment->SetRowDouble(r, loop);
    }

    rowComparisonFunction = CompareRowsByScore;
    SortRows();
    return true;
}

void SequenceDisplay::SortRows(void)
{
    if (!rowComparisonFunction) {
        ERRORMSG("SequenceDisplay::SortRows() - must first set comparison function");
        return;
    }

    // to simplify sorting, construct list of dependent rows only
    vector < DisplayRowFromAlignment * > dependents;
    unsigned int row;
    for (row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->row > 0)
            dependents.push_back(alnRow);
    }

    // do the sort
    stable_sort(dependents.begin(), dependents.end(), rowComparisonFunction);
    rowComparisonFunction = NULL;

    // recreate the row list with new order
    RowVector newRows(rows.size());
    unsigned int nDependents = 0;
    for (row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->row > 0)
            newRows[row] = dependents[nDependents++];   // put sorted dependents in place
        else
            newRows[row] = rows[row];           // leave other rows in original order
    }
    if (nDependents == dependents.size())   // sanity check
        rows = newRows;
    else
        ERRORMSG("SequenceDisplay::SortRows() - internal inconsistency");

    (*viewerWindow)->viewer->Save();   // make this an undoable operation
    (*viewerWindow)->UpdateDisplay(this);
}

bool SequenceDisplay::ProximitySort(unsigned int displayRow)
{
    DisplayRowFromAlignment *keyRow = dynamic_cast<DisplayRowFromAlignment*>(rows[displayRow]);
    if (!keyRow || keyRow->row == 0) return false;
    if (keyRow->alignment->NRows() < 3) return true;

    TRACEMSG("doing Proximity Sort on alignment row " << keyRow->row);
    unsigned int row;
    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    keyRow->alignment->GetUngappedAlignedBlocks(&blocks);
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
    const Sequence *seq1 = keyRow->alignment->GetSequenceOfRow(keyRow->row);
    vector < DisplayRowFromAlignment * > sortedByScore;

    // calculate scores for each row based on simple Blosum62 sum of all aligned pairs
    for (row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (!alnRow) continue;
        sortedByScore.push_back(alnRow);

        if (alnRow == keyRow) {
            keyRow->alignment->SetRowDouble(keyRow->row, kMax_Double);
            keyRow->alignment->SetRowStatusLine(keyRow->row, "(key row)");
        } else {
            const Sequence *seq2 = alnRow->alignment->GetSequenceOfRow(alnRow->row);
            double score = 0.0;
            for (b=blocks.begin(); b!=be; ++b) {
                const Block::Range
                    *r1 = (*b)->GetRangeOfRow(keyRow->row),
                    *r2 = (*b)->GetRangeOfRow(alnRow->row);
                for (unsigned int i=0; i<(*b)->width; ++i)
                    score += GetBLOSUM62Score(
                        seq1->sequenceString[r1->from + i], seq2->sequenceString[r2->from + i]);
            }
            alnRow->alignment->SetRowDouble(alnRow->row, score);
            wxString str;
            str.Printf("Score vs. key row: %i", (int) score);
            alnRow->alignment->SetRowStatusLine(alnRow->row, str.c_str());
        }
    }
    if (sortedByScore.size() != keyRow->alignment->NRows()) {
        ERRORMSG("SequenceDisplay::ProximitySort() - wrong # rows in sort list");
        return false;
    }

    // sort by these scores
    stable_sort(sortedByScore.begin(), sortedByScore.end(), CompareRowsByScore);

    // find where the master row is in sorted list
    unsigned int M;
    for (M=0; M<sortedByScore.size(); ++M) if (sortedByScore[M]->row == 0) break;

    // arrange by proximity to key row
    vector < DisplayRowFromAlignment * > arrangedByProximity(sortedByScore.size(), NULL);

    // add master and key row
    arrangedByProximity[0] = sortedByScore[M];  // move master back to top
    arrangedByProximity[M] = sortedByScore[0];  // move key row to M
    // remove these from sorted list
    vector < bool > toRemove(sortedByScore.size(), false);
    toRemove[M] = toRemove[0] = true;
    VectorRemoveElements(sortedByScore, toRemove, 2);

    // add the rest of the sequences in the sorted list to the arranged list
    int i = 1, j = 1, N;
    unsigned int R = 0;
    while (R < sortedByScore.size()) {
        N = M + i*j;    // iterate N = M+1, M-1, M+2, M-2, ...
        j = -j;
        if (j > 0) ++i;
        if (N > 0 && N < (int)arrangedByProximity.size())
            arrangedByProximity[N] = sortedByScore[R++];
    }

    // recreate the row list with new order
    RowVector newRows(rows.size());
    unsigned int nNewRows = 0;
    for (row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow)
            newRows[row] = arrangedByProximity[nNewRows++];   // put arranged rows in place
        else
            newRows[row] = rows[row];           // leave other rows in original order
    }
    if (nNewRows == arrangedByProximity.size())   // sanity check
        rows = newRows;
    else
        ERRORMSG("SequenceDisplay::ProximitySort() - internal inconsistency");

    // finally, highlight the key row and scroll approximately there
    GlobalMessenger()->RemoveAllHighlights(true);
    GlobalMessenger()->AddHighlights(seq1, 0, seq1->Length() - 1);
    (*viewerWindow)->ScrollToRow((M - 3) > 0 ? (M - 3) : 0);
    (*viewerWindow)->viewer->Save();   // make this an undoable operation
	(*viewerWindow)->UpdateDisplay(this);
    return true;
}

bool SequenceDisplay::CalculateRowScoresWithThreader(double weightPSSM)
{
    if (isEditable) {
        SequenceViewer *seqViewer = dynamic_cast<SequenceViewer*>((*viewerWindow)->viewer);
        if (seqViewer) {
            seqViewer->alignmentManager->CalculateRowScoresWithThreader(weightPSSM);
            TRACEMSG("calculated row scores");
            return true;
        }
    }
    return false;
}

void SequenceDisplay::RowsAdded(unsigned int nRowsAddedToMultiple, BlockMultipleAlignment *multiple, int alnWhere)
{
    if (nRowsAddedToMultiple == 0) return;

    // find the last row that's from this multiple
    unsigned int r, nRows = 0, lastAlnRowIndex = 0;
    int displayWhere = -1;
    DisplayRowFromAlignment *lastAlnRow = NULL;
    for (r=0; r<rows.size(); ++r) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[r]);
        if (alnRow && alnRow->alignment == multiple) {
            lastAlnRow = alnRow;
            lastAlnRowIndex = r;
            ++nRows;
            if (alnWhere >= 0 && (int)alnRow->row == alnWhere)
                displayWhere = r;  // convert 'where' from alignment row to display row
        }
    }
    if (!lastAlnRow || multiple->NRows() != nRows + nRowsAddedToMultiple) {
        ERRORMSG("SequenceDisplay::RowsAdded() - inconsistent parameters");
        return;
    }
    int rowToMergeAfter = (displayWhere >= 0) ? displayWhere : lastAlnRowIndex;
    INFOMSG("adding new row after display row #" << (rowToMergeAfter+1));

    // move higher rows up to leave space for new rows
    unsigned int nRowsToMove = rows.size() - 1 - rowToMergeAfter;
    rows.resize(rows.size() + nRowsAddedToMultiple);
    for (r=0; r<nRowsToMove; ++r)
        rows[rows.size() - 1 - r] = rows[rows.size() - 1 - r - nRowsAddedToMultiple];

    // add new rows, assuming new rows to add to the display are from the last rows of the multiple
    for (r=0; r<nRowsAddedToMultiple; ++r)
        rows[rowToMergeAfter + 1 + r] = new DisplayRowFromAlignment(
            multiple->NRows() + r - nRowsAddedToMultiple, multiple);

    UpdateAfterEdit(multiple);
}

void SequenceDisplay::RowsRemoved(const vector < unsigned int >& rowsRemoved,
    const BlockMultipleAlignment *multiple)
{
    if (rowsRemoved.size() == 0) return;

    // first, construct a map of old alignment row numbers -> new row numbers; also do sanity checks
    unsigned int i;
    vector < unsigned int > alnRowNumbers(multiple->NRows() + rowsRemoved.size());
    vector < bool > removedAlnRows(alnRowNumbers.size(), false);
    for (i=0; i<alnRowNumbers.size(); ++i) alnRowNumbers[i] = i;
    for (i=0; i<rowsRemoved.size(); ++i) {
        if (rowsRemoved[i] < 1 || rowsRemoved[i] >= alnRowNumbers.size()) {
            ERRORMSG("SequenceDisplay::RowsRemoved() - can't remove row " << removedAlnRows[i]);
            return;
        } else
            removedAlnRows[rowsRemoved[i]] = true;
    }
    VectorRemoveElements(alnRowNumbers, removedAlnRows, rowsRemoved.size());
    map < unsigned int, unsigned int > oldRowToNewRow;
    for (i=0; i<alnRowNumbers.size(); ++i) oldRowToNewRow[alnRowNumbers[i]] = i;

    // then tag rows to remove from display, and update row numbers for rows not removed
    vector < bool > removeDisplayRows(rows.size(), false);
    for (i=0; i<rows.size(); ++i) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[i]);
        if (alnRow && alnRow->alignment == multiple) {
            if (removedAlnRows[alnRow->row]) {
                delete rows[i];
                removeDisplayRows[i] = true;
            } else
                alnRow->row = oldRowToNewRow[alnRow->row];
        }
    }
    VectorRemoveElements(rows, removeDisplayRows, rowsRemoved.size());

    UpdateAfterEdit(multiple);
}

bool SequenceDisplay::GetDisplayCoordinates(const Molecule *molecule, unsigned int seqIndex,
    BlockMultipleAlignment::eUnalignedJustification justification, unsigned int *column, unsigned int *row)
{
    if (!molecule) return false;

    unsigned int displayRow;
    const Sequence *seq;

    // search by Molecule*
    for (displayRow=0; displayRow<NRows(); ++displayRow) {
        seq = rows[displayRow]->GetSequence();
        if (seq && seq->molecule == molecule) {
            *row = displayRow;

            const DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[displayRow]);
            if (alnRow) {
                int c = alnRow->alignment->GetAlignmentIndex(alnRow->row, seqIndex, justification);
                *column = c;
                return (c >= 0);
            }

            const DisplayRowFromSequence *seqRow = dynamic_cast<DisplayRowFromSequence*>(rows[displayRow]);
            if (seqRow && seqIndex >= seqRow->fromIndex && seqIndex <= seqRow->toIndex) {
                *column = seqIndex - seqRow->fromIndex;
                return true;
            }
        }
    }
    return false;
}

END_SCOPE(Cn3D)
