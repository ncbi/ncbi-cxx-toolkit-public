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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.h>

#include <algorithm>

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
    int column, BlockMultipleAlignment::eUnalignedJustification justification,
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

DisplayRowFromSequence::DisplayRowFromSequence(const Sequence *s, int from, int to) :
    sequence(s), fromIndex(from), toIndex(to)
{
    if (from < 0 || from >= sequence->Length() || from > to || to < 0 || to >= sequence->Length())
        ERRORMSG("DisplayRowFromSequence::DisplayRowFromSequence() - from/to indexes out of range");
}

bool DisplayRowFromSequence::GetCharacterTraitsAt(
	int column, BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const
{
    int index = column + fromIndex;
    if (index > toIndex)
        return false;

    *character = tolower(sequence->sequenceString[index]);
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
    int column, BlockMultipleAlignment::eUnalignedJustification justification,
    const Sequence **sequenceHandle, int *seqIndex) const
{
    int index = column + fromIndex;
    if (index > toIndex)
        return false;

    *sequenceHandle = sequence;
    *seqIndex = index;
    return true;
}

void DisplayRowFromSequence::SelectedRange(int from, int to,
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

bool DisplayRowFromString::GetCharacterTraitsAt(int column,
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
    isEditable(editable), maxRowWidth(0), viewerWindow(parentViewerWindow), startingColumn(0)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (int i=0; i<rows.size(); ++i) delete rows[i];
}

void SequenceDisplay::Empty(void)
{
    for (int i=0; i<rows.size(); ++i) delete rows[i];
    rows.clear();
    startingColumn = maxRowWidth = 0;
}


SequenceDisplay * SequenceDisplay::Clone(const Old2NewAlignmentMap& newAlignments) const
{
    SequenceDisplay *copy = new SequenceDisplay(isEditable, viewerWindow);
    for (int row=0; row<rows.size(); ++row)
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

BlockMultipleAlignment * SequenceDisplay::GetAlignmentForRow(int row) const
{
    if (row < 0 || row >= rows.size()) return NULL;
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

void SequenceDisplay::AddRowFromAlignment(int row, BlockMultipleAlignment *fromAlignment)
{
    if (!fromAlignment || row < 0 || row >= fromAlignment->NRows()) {
        ERRORMSG("SequenceDisplay::AddRowFromAlignment() failed");
        return;
    }

    AddRow(new DisplayRowFromAlignment(row, fromAlignment));
}

void SequenceDisplay::AddRowFromSequence(const Sequence *sequence, int from, int to)
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

bool SequenceDisplay::GetCharacterTraitsAt(int column, int row,
        char *character, wxColour *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const
{
    if (row < 0 || row > rows.size()) {
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

        if (row >= 0 && row < rows.size()) {
            const DisplayRow *displayRow = rows[row];
            const Sequence *sequence = NULL;

            // show id if we're in sequence area
            if (column >= 0 && column < displayRow->Width()) {

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
                    status = sequence->description.c_str();
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
    if (column < 0 && row >= 0 && row < NRows()) {
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

            if (sequenceWindow->DoRealignRow() || sequenceWindow->DoDeleteRow()) {
                DisplayRowFromAlignment *selectedRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
                if (!selectedRow || selectedRow->row == 0 || !selectedRow->alignment) {
                    WARNINGMSG("Can't delete/realign that row...");
                    return false;
                }

                if (sequenceWindow->DoRealignRow()) {
                    vector < int > selectedSlaves(1);
                    selectedSlaves[0] = selectedRow->row;
                    sequenceWindow->sequenceViewer->alignmentManager->RealignSlaveSequences(alignment, selectedSlaves);
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

            // set region (on slave sequence)
            if (updateWindow->DoSetRegion()) {
                // dialog uses 1-based sequence locations
                RegionDialog dialog(updateWindow,
                    alignment->GetSequenceOfRow(1), alignment->alignSlaveFrom + 1, alignment->alignSlaveTo + 1);
                if (dialog.ShowModal() == wxOK) {
                    int from, to;
                    if (!dialog.GetValues(&from, &to)) {
                        ERRORMSG("RegionDialog returned OK, but values invalid");
                    } else {
                        TRACEMSG("set region (slave): " << from << " to " << to);
                        alignment->alignSlaveFrom = from - 1;
                        alignment->alignSlaveTo = to - 1;
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
                else if (controlDown && !shiftDown && columnTo < maxRowWidth-1) // shift right
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
    for (i=0; i<newRows.size(); ++i) {
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
    for (int i=0; i<rows.size(); ++i) {
        const Sequence *sequence = rows[i]->GetSequence();
        if (sequence && sequence->molecule)
            GlobalMessenger()->PostRedrawMolecule(sequence->molecule);
    }
}

DisplayRowFromString * SequenceDisplay::FindBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const
{
    DisplayRowFromString *blockBoundaryRow = NULL;
    for (int row=0; row<rows.size(); ++row) {
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
    int i = 0;
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

    int alignmentWidth = blockBoundaryRow->alignment->AlignmentWidth();
    blockBoundaryRow->theString.resize(alignmentWidth);

    // fill out block boundary marker string
    int blockColumn, blockWidth;
    for (int i=0; i<alignmentWidth; ++i) {
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
    int nToRemove = 0;
    for (int row=0; row<rows.size(); ++row) {
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
    for (int row=0; row<rows.size(); ++row) {
        const Sequence *seq = rows[row]->GetSequence();
        if (seq && seq->isProtein)
            seqs->push_back(seq);
    }
}

void SequenceDisplay::GetSequences(const BlockMultipleAlignment *forAlignment, SequenceList *seqs) const
{
    seqs->clear();
    for (int row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->alignment == forAlignment)
            seqs->push_back(alnRow->alignment->GetSequenceOfRow(alnRow->row));
    }
}

void SequenceDisplay::GetRowOrder(
    const BlockMultipleAlignment *forAlignment, vector < int > *slaveRowOrder) const
{
    slaveRowOrder->clear();
    for (int row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->alignment == forAlignment)
            slaveRowOrder->push_back(alnRow->row);
    }
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
    (*viewerWindow)->viewer->Save();   // make this an undoable operation
	(*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::SortRowsByThreadingScore(double weightPSSM)
{
    if (!CalculateRowScoresWithThreader(weightPSSM)) return;
    rowComparisonFunction = CompareRowsByScore;
    SortRows();
    TRACEMSG("sorted rows");
    (*viewerWindow)->viewer->Save();   // make this an undoable operation
	(*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::FloatPDBRowsToTop(void)
{
    rowComparisonFunction = CompareRowsFloatPDB;
    SortRows();
    (*viewerWindow)->viewer->Save();   // make this an undoable operation
	(*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::FloatHighlightsToTop(void)
{
    rowComparisonFunction = CompareRowsFloatHighlights;
    SortRows();
    (*viewerWindow)->viewer->Save();   // make this an undoable operation
	(*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::FloatGVToTop(void)
{
    DisplayRowFromAlignment *alnRow = NULL;
    for (int row=0; row<rows.size(); ++row) {
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
        (*viewerWindow)->Command(ViewerWindowBase::MID_SHOW_GEOM_VLTNS);

    rowComparisonFunction = CompareRowsFloatGV;
    SortRows();
    (*viewerWindow)->viewer->Save();   // make this an undoable operation
    (*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::SortRowsBySelfHit(void)
{
    // get alignment
    for (int row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow) {
            // do self-hit calculation
            (*viewerWindow)->viewer->CalculateSelfHitScores(alnRow->alignment);

            // then sort by score
            rowComparisonFunction = CompareRowsByEValue;
            SortRows();
			(*viewerWindow)->viewer->Save();   // make this an undoable operation
			(*viewerWindow)->UpdateDisplay(this);
            break;
        }
    }
}

void SequenceDisplay::SortRows(void)
{
    if (!rowComparisonFunction) {
        ERRORMSG("SequenceDisplay::SortRows() - must first set comparison function");
        return;
    }

    // to simplify sorting, construct list of slave rows only
    vector < DisplayRowFromAlignment * > slaves;
    int row;
    for (row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->row > 0)
            slaves.push_back(alnRow);
    }

    // do the sort
    stable_sort(slaves.begin(), slaves.end(), rowComparisonFunction);
    rowComparisonFunction = NULL;

    // recreate the row list with new order
    RowVector newRows(rows.size());
    int nSlaves = 0;
    for (row=0; row<rows.size(); ++row) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->row > 0)
            newRows[row] = slaves[nSlaves++];   // put sorted slaves in place
        else
            newRows[row] = rows[row];           // leave other rows in original order
    }
    if (nSlaves == slaves.size())   // sanity check
        rows = newRows;
    else
        ERRORMSG("SequenceDisplay::SortRows() - internal inconsistency");
}

bool SequenceDisplay::ProximitySort(int displayRow)
{
    DisplayRowFromAlignment *keyRow = dynamic_cast<DisplayRowFromAlignment*>(rows[displayRow]);
    if (!keyRow || keyRow->row == 0) return false;
    if (keyRow->alignment->NRows() < 3) return true;

    TRACEMSG("doing Proximity Sort on alignment row " << keyRow->row);
    int row;
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
                for (int i=0; i<(*b)->width; ++i)
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
    int M;
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
    int i = 1, j = 1, N, R = 0;
    while (R < sortedByScore.size()) {
        N = M + i*j;    // iterate N = M+1, M-1, M+2, M-2, ...
        j = -j;
        if (j > 0) ++i;
        if (N > 0 && N < arrangedByProximity.size())
            arrangedByProximity[N] = sortedByScore[R++];
    }

    // recreate the row list with new order
    RowVector newRows(rows.size());
    int nNewRows = 0;
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

void SequenceDisplay::RowsAdded(int nRowsAddedToMultiple, BlockMultipleAlignment *multiple, int alnWhere)
{
    if (nRowsAddedToMultiple <= 0) return;

    // find the last row that's from this multiple
    int r, nRows = 0, lastAlnRowIndex, displayWhere = -1;
    DisplayRowFromAlignment *lastAlnRow = NULL;
    for (r=0; r<rows.size(); ++r) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[r]);
        if (alnRow && alnRow->alignment == multiple) {
            lastAlnRow = alnRow;
            lastAlnRowIndex = r;
            ++nRows;
            if (alnWhere >= 0 && alnRow->row == alnWhere)
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
    int nRowsToMove = rows.size() - 1 - rowToMergeAfter;
    rows.resize(rows.size() + nRowsAddedToMultiple);
    for (r=0; r<nRowsToMove; ++r)
        rows[rows.size() - 1 - r] = rows[rows.size() - 1 - r - nRowsAddedToMultiple];

    // add new rows, assuming new rows to add to the display are from the last rows of the multiple
    for (r=0; r<nRowsAddedToMultiple; ++r)
        rows[rowToMergeAfter + 1 + r] = new DisplayRowFromAlignment(
            multiple->NRows() + r - nRowsAddedToMultiple, multiple);

    UpdateAfterEdit(multiple);
}

void SequenceDisplay::RowsRemoved(const vector < int >& rowsRemoved,
    const BlockMultipleAlignment *multiple)
{
    if (rowsRemoved.size() == 0) return;

    // first, construct a map of old alignment row numbers -> new row numbers; also do sanity checks
    int i;
    vector < int > alnRowNumbers(multiple->NRows() + rowsRemoved.size());
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
    map < int, int > oldRowToNewRow;
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

bool SequenceDisplay::GetDisplayCoordinates(const Molecule *molecule, int seqIndex,
    BlockMultipleAlignment::eUnalignedJustification justification, int *column, int *row)
{
    if (!molecule || seqIndex < 0) return false;

    int displayRow;
    const Sequence *seq;

    // search by Molecule*
    for (displayRow=0; displayRow<NRows(); ++displayRow) {
        seq = rows[displayRow]->GetSequence();
        if (seq && seq->molecule == molecule) {
            *row = displayRow;

            const DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[displayRow]);
            if (alnRow) {
                *column = alnRow->alignment->GetAlignmentIndex(alnRow->row, seqIndex, justification);
                return (*column >= 0);
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


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.79  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.78  2004/10/04 17:00:54  thiessen
* add expand/restrict highlights, delete all blocks/all rows in updates
*
* Revision 1.77  2004/10/01 13:07:44  thiessen
* add ZipAlignResidue
*
* Revision 1.76  2004/09/27 21:40:46  thiessen
* add highlight cache
*
* Revision 1.75  2004/09/27 15:33:04  thiessen
* add block info content optimization on ctrl+shift+click
*
* Revision 1.74  2004/09/23 10:31:14  thiessen
* add block extension algorithm
*
* Revision 1.73  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.72  2004/03/15 17:55:06  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.71  2004/02/19 17:05:05  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.70  2003/11/06 19:07:19  thiessen
* leave show gv's on if on already
*
* Revision 1.69  2003/10/20 13:17:15  thiessen
* add float geometry violations sorting
*
* Revision 1.68  2003/07/14 18:37:07  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.67  2003/02/03 19:20:05  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.66  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.65  2003/01/29 01:41:05  thiessen
* add merge neighbor instead of merge near highlight
*
* Revision 1.64  2003/01/28 21:07:56  thiessen
* add block fit coloring algorithm; tweak row dragging; fix style bug
*
* Revision 1.63  2003/01/27 16:42:24  thiessen
* remove debugging messages
*
* Revision 1.62  2003/01/27 15:52:22  thiessen
* merge after highlighted row; show rejects; trim rejects from new reject list
*
* Revision 1.61  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.60  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.59  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.58  2002/09/05 18:38:57  thiessen
* add sort by highlights
*
* Revision 1.57  2002/08/28 20:30:33  thiessen
* fix proximity sort bug
*
* Revision 1.56  2002/08/15 22:13:16  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.55  2002/08/15 12:57:07  thiessen
* fix highlight redraw bug
*
* Revision 1.54  2002/08/13 20:46:37  thiessen
* add global block aligner
*
* Revision 1.53  2002/07/26 15:28:48  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.52  2002/07/23 15:46:50  thiessen
* print out more BLAST info; tweak save file name
*
* Revision 1.51  2002/06/13 14:54:07  thiessen
* add sort by self-hit
*
* Revision 1.50  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.49  2002/05/07 20:22:47  thiessen
* fix for BLAST/PSSM
*
* Revision 1.48  2002/05/02 18:40:25  thiessen
* do BLAST/PSSM for debug builds only, for testing
*
* Revision 1.47  2002/04/26 19:01:00  thiessen
* fix display delete bug
*
* Revision 1.46  2002/04/26 13:46:36  thiessen
* comment out all blast/pssm methods
*
* Revision 1.45  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.44  2002/02/28 19:11:52  thiessen
* wrap sequences in single-structure mode
*
* Revision 1.43  2002/02/22 14:24:01  thiessen
* sort sequences in reject dialog ; general identifier comparison
*
* Revision 1.42  2002/02/21 12:26:30  thiessen
* fix row delete bug ; remember threader options
*
* Revision 1.41  2002/02/19 14:59:39  thiessen
* add CDD reject and purge sequence
*
* Revision 1.40  2002/02/15 15:27:12  thiessen
* on Mac, use meta for MouseDown events
*
* Revision 1.39  2002/02/15 01:02:17  thiessen
* ctrl+click keeps edit modes
*
* Revision 1.38  2002/02/05 18:53:25  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.37  2001/12/05 17:16:56  thiessen
* fix row insert bug
*
* Revision 1.36  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.35  2001/11/27 16:26:08  thiessen
* major update to data management system
*
* Revision 1.34  2001/10/08 00:00:09  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.33  2001/09/27 15:37:59  thiessen
* decouple sequence import and BLAST
*
* Revision 1.32  2001/09/19 22:55:39  thiessen
* add preliminary net import and BLAST
*
* Revision 1.31  2001/08/24 14:54:50  thiessen
* make status row # order-independent :)
*
* Revision 1.30  2001/08/24 14:32:51  thiessen
* add row # to status line
*
* Revision 1.29  2001/08/08 02:25:27  thiessen
* add <memory>
*
* Revision 1.28  2001/07/10 16:39:55  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.27  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.26  2001/06/07 19:05:38  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.25  2001/06/05 13:21:08  thiessen
* fix structure alignment list problems
*
* Revision 1.24  2001/06/04 14:58:00  thiessen
* add proximity sort; highlight sequence on browser launch
*
* Revision 1.23  2001/06/01 18:07:27  thiessen
* fix display clone bug
*
* Revision 1.22  2001/06/01 14:05:12  thiessen
* add float PDB sort
*
* Revision 1.21  2001/06/01 13:35:58  thiessen
* add aligned block number to status line
*
* Revision 1.20  2001/05/31 18:47:08  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.19  2001/05/22 19:09:31  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.18  2001/05/14 16:04:31  thiessen
* fix minor row reordering bug
*
* Revision 1.17  2001/05/11 02:10:42  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.16  2001/05/09 17:15:06  thiessen
* add automatic block removal upon demotion
*
* Revision 1.15  2001/05/02 16:35:15  thiessen
* launch entrez web page on sequence identifier
*
* Revision 1.14  2001/04/19 12:58:32  thiessen
* allow merge and delete of individual updates
*
* Revision 1.13  2001/04/18 15:46:53  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.12  2001/04/05 22:55:35  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.11  2001/04/04 00:27:14  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.10  2001/03/30 14:43:41  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.9  2001/03/30 03:07:34  thiessen
* add threader score calculation & sorting
*
* Revision 1.8  2001/03/22 00:33:17  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.7  2001/03/19 15:50:39  thiessen
* add sort rows by identifier
*
* Revision 1.6  2001/03/13 01:25:05  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.5  2001/03/09 15:49:04  thiessen
* major changes to add initial update viewer
*
* Revision 1.4  2001/03/06 20:49:14  thiessen
* fix row->alignment bug
*
* Revision 1.3  2001/03/06 20:20:50  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.2  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
*/
