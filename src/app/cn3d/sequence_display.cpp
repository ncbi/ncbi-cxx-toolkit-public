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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include "cn3d/sequence_display.hpp"
#include "cn3d/viewer_window_base.hpp"
#include "cn3d/viewer_base.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// Should be replaced by some reference to a Colors object, eventually...
static const wxColour highlightColor(255,255,0);  // yellow

// block marker string stuff
static const char
    blockLeftEdgeChar = '<',
    blockRightEdgeChar = '>',
    blockOneColumnChar = '^',
    blockInsideChar = '-';
static const std::string blockBoundaryStringTitle("(blocks)");


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
// SequenceDisplayWidget.
////////////////////////////////////////////////////////////////////////////////

SequenceDisplay::SequenceDisplay(bool editable, ViewerWindowBase* const *parentViewerWindow) :
    isEditable(editable), maxRowWidth(0), viewerWindow(parentViewerWindow), startingColumn(0)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (int i=0; i<rows.size(); i++) delete rows[i];
}

SequenceDisplay * SequenceDisplay::Clone(BlockMultipleAlignment *newAlignment) const
{
    SequenceDisplay *copy = new SequenceDisplay(isEditable, viewerWindow);
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
    UpdateBlockBoundaryRow();
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

        if ((*viewerWindow)->DoDeleteRow()) {
            if (row > 0) {
                DisplayRowFromAlignment *selectedRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
                if (selectedRow == NULL || selectedRow->row == 0 || selectedRow->alignment->NRows() <= 2 ||
                    selectedRow->alignment != (*viewerWindow)->viewer->GetCurrentAlignment()) {
                    ERR_POST(Warning << "Can't delete that row...");
                } else {
                    // delete row based on alignment row # (not display row #)
                    if ((*viewerWindow)->viewer->GetCurrentAlignment()->DeleteRow(selectedRow->row)) {

                        // delete this row from the display, and update higher row #'s
                        RowVector::iterator r, re = rows.end(), toDelete;
                        for (r=rows.begin(); r!=re; r++) {
                            DisplayRowFromAlignment
                                *currentARow = dynamic_cast<DisplayRowFromAlignment*>(*r);
                            if (!currentARow)
                                continue;
                            else if (currentARow == selectedRow)
                                toDelete = r;
                            else if (currentARow->row > selectedRow->row)
                                (currentARow->row)--;
                        }
                        rows.erase(toDelete);

                        (*viewerWindow)->DeleteRowOff();
                        UpdateAfterEdit();
                    }
                }
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
                if ((*viewerWindow)->viewer->GetCurrentAlignment()->
                        ShiftRow(alnRow->row, columnFrom, columnTo,
                            (*viewerWindow)->GetCurrentJustification()))
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
        GlobalMessenger()->PostRedrawSequenceViewer((*viewerWindow)->viewer);
    }
}

void SequenceDisplay::RedrawAlignedMolecules(void) const
{
    for (int i=0; i<rows.size(); i++) {
        const Sequence *sequence = rows[i]->GetSequence();
        if (sequence && sequence->molecule)
            GlobalMessenger()->PostRedrawMolecule(sequence->molecule);
    }
}

DisplayRowFromString * SequenceDisplay::FindBlockBoundaryRow(void)
{
    DisplayRowFromString *blockBoundaryRow = NULL;
    for (int row=0; row<rows.size(); row++) {
        if ((blockBoundaryRow = dynamic_cast<DisplayRowFromString*>(rows[row])) != NULL) {
            if (blockBoundaryRow->title == blockBoundaryStringTitle)
                break;
            else
                blockBoundaryRow = NULL;
        }
    }
    return blockBoundaryRow;
}

void SequenceDisplay::AddBlockBoundaryRow(void)
{
    if (!IsEditable() || FindBlockBoundaryRow() != NULL) return;

    DisplayRowFromString *blockBoundaryRow =
        new DisplayRowFromString("", Vector(0,0,0), blockBoundaryStringTitle, true, Vector(0.8,0.8,1));
    PrependRow(blockBoundaryRow);
    UpdateBlockBoundaryRow();
    (*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::UpdateBlockBoundaryRow(void)
{
    DisplayRowFromString *blockBoundaryRow;
    if (!IsEditable() || (blockBoundaryRow = FindBlockBoundaryRow()) == NULL) return;

    int alignmentWidth = (*viewerWindow)->viewer->GetCurrentAlignment()->AlignmentWidth();
    blockBoundaryRow->theString.resize(alignmentWidth);

    // fill out block boundary marker string
    int blockColumn, blockWidth;
    for (int i=0; i<alignmentWidth; i++) {
        (*viewerWindow)->viewer->GetCurrentAlignment()->GetAlignedBlockPosition(i, &blockColumn, &blockWidth);
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
    GlobalMessenger()->PostRedrawSequenceViewer((*viewerWindow)->viewer);
}

void SequenceDisplay::RemoveBlockBoundaryRow(void)
{
    DisplayRowFromString *blockBoundaryRow = FindBlockBoundaryRow();
    if (blockBoundaryRow) {
        RemoveRow(blockBoundaryRow);
        (*viewerWindow)->UpdateDisplay(this);
    }
}

END_SCOPE(Cn3D)

