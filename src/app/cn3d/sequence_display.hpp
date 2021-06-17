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

#ifndef CN3D_SEQUENCE_DISPLAY__HPP
#define CN3D_SEQUENCE_DISPLAY__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <map>

#include "viewable_alignment.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"


BEGIN_SCOPE(Cn3D)

typedef std::map < BlockMultipleAlignment *, BlockMultipleAlignment * > Old2NewAlignmentMap;

////////////////////////////////////////////////////////////////////////////////
// The sequence view is composed of rows which can be from sequence, alignment,
// or any string - anything derived from DisplayRow.
////////////////////////////////////////////////////////////////////////////////

class DisplayRow
{
public:
    virtual ~DisplayRow(void) { }
    
    virtual unsigned int Width(void) const = 0;
    virtual bool GetCharacterTraitsAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const = 0;
    virtual bool GetSequenceAndIndexAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequence, int *index) const = 0;
    virtual const Sequence * GetSequence(void) const = 0;
    virtual void SelectedRange(unsigned int from, unsigned int to,
        BlockMultipleAlignment::eUnalignedJustification justification, bool toggle) const = 0;

    virtual DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const = 0;
};

class DisplayRowFromAlignment : public DisplayRow
{
public:
    unsigned int row;
    BlockMultipleAlignment * const alignment;

    DisplayRowFromAlignment(unsigned int r, BlockMultipleAlignment *a) :
        row(r), alignment(a) { }
    virtual ~DisplayRowFromAlignment(void) { }

    unsigned int Width(void) const { return alignment->AlignmentWidth(); }

    DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const
        { return new DisplayRowFromAlignment(row, newAlignments.find(alignment)->second); }

    bool GetCharacterTraitsAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequence, int *index) const
    {
		bool ignore;
        return alignment->GetSequenceAndIndexAt(column, row, justification, sequence, index, &ignore);
    }

    const Sequence * GetSequence(void) const
    {
        return alignment->GetSequenceOfRow(row);
    }

    void SelectedRange(unsigned int from, unsigned int to,
        BlockMultipleAlignment::eUnalignedJustification justification, bool toggle) const
    {
        alignment->SelectedRange(row, from, to, justification, toggle);
    }
};

class DisplayRowFromSequence : public DisplayRow
{
public:
    const Sequence * const sequence;
    const unsigned int fromIndex, toIndex;

    DisplayRowFromSequence(const Sequence *s, unsigned int from, unsigned int to);
    virtual ~DisplayRowFromSequence(void) { }

    unsigned int Width(void) const { return toIndex - fromIndex + 1; }

    DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const
        { return new DisplayRowFromSequence(sequence, fromIndex, toIndex); }

    bool GetCharacterTraitsAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequenceHandle, int *index) const;

    const Sequence * GetSequence(void) const
        { return sequence; }

    void SelectedRange(unsigned int from, unsigned int to,
        BlockMultipleAlignment::eUnalignedJustification justification, bool toggle) const;
};

class DisplayRowFromString : public DisplayRow
{
public:
    const std::string title;
    std::string theString;
    const Vector stringColor, backgroundColor;
    const bool hasBackgroundColor;
    BlockMultipleAlignment * const alignment;

    DisplayRowFromString(const std::string& s, const Vector color = Vector(0,0,0.5),
        const std::string& t = "", bool hasBG = false, Vector bgColor = Vector(1,1,1),
        BlockMultipleAlignment *a = NULL) :
            title(t), theString(s), stringColor(color),
            backgroundColor(bgColor), hasBackgroundColor(hasBG), alignment(a) { }
    virtual ~DisplayRowFromString(void) { }

    unsigned int Width(void) const { return theString.size(); }

    DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const
        { return new DisplayRowFromString(
            theString, stringColor, title, hasBackgroundColor, backgroundColor,
            alignment ? newAlignments.find(alignment)->second : NULL); }

    bool GetCharacterTraitsAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(unsigned int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequenceHandle, int *index) const { return false; }

    const Sequence * GetSequence(void) const { return NULL; }

    void SelectedRange(unsigned int from, unsigned int to,
        BlockMultipleAlignment::eUnalignedJustification justification, bool toggle) const
        { } // do nothing
};


////////////////////////////////////////////////////////////////////////////////
// The SequenceDisplay is the structure that holds the DisplayRows of the view.
// It's also derived from ViewableAlignment, in order to be plugged into a
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

class SequenceViewer;
class UpdateViewer;
class ViewerWindowBase;
class Molecule;

class SequenceDisplay : public ViewableAlignment
{
    friend class SequenceViewer;
    friend class UpdateViewer;

public:
    SequenceDisplay(bool editable, ViewerWindowBase* const *parentViewerWindow);
    virtual ~SequenceDisplay(void);

    // these functions add a row to the end of the display, from various sources
    void AddRowFromAlignment(unsigned int row, BlockMultipleAlignment *fromAlignment);
    void AddRowFromSequence(const Sequence *sequence, unsigned int from, unsigned int to);
    void AddRowFromString(const std::string& anyString);

    // adds a string row to the alignment, that contains block boundary indicators
    DisplayRowFromString * FindBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const;
    void UpdateBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const;
    void AddBlockBoundaryRow(BlockMultipleAlignment *forAlignment);  // adds to end of display!
    void AddBlockBoundaryRows(void);
    void RemoveBlockBoundaryRows(void);

    // returns a list of all sequences in the display (in order) for the given alignment
    typedef std::vector < const Sequence * > SequenceList;
    void GetSequences(const BlockMultipleAlignment *forAlignment, SequenceList *seqs) const;

    // returns a list of all protein sequences in the display (in order)
    void GetProteinSequences(SequenceList *seqs) const;

    // fills the vector with the current row ordering for the given alignment
    void GetRowOrder(const BlockMultipleAlignment *forAlignment, std::vector < unsigned int > *dependentRowOrder) const;

    // to inform the display that new rows have been added to or removed from the multiple
    void RowsAdded(unsigned int nRowsAddedToMultiple, BlockMultipleAlignment *multiple, int where = -1);
    void RowsRemoved(const std::vector < unsigned int >& rowsRemoved, const BlockMultipleAlignment *multiple);

    // row scoring and sorting functions - only for single-alignment displays!
    // sorting necessarily always leaves master at the top
    bool CalculateRowScoresWithThreader(double weightPSSM);
    void SortRowsByIdentifier(void);
    void SortRowsByThreadingScore(double weightPSSM);
    void SortRowsBySelfHit(void);
    void FloatPDBRowsToTop(void);
    void FloatHighlightsToTop(void);
    void FloatGVToTop(void);
    bool SortRowsByLoopLength(unsigned int row, unsigned int alnIndex);

    // a sort of clustering of similar sequences around a particular row
    bool ProximitySort(unsigned int displayRow);

    // create a new copy of this object
    SequenceDisplay * Clone(const Old2NewAlignmentMap& newAlignments) const;

    // clear out all rows from this display
    void Empty(void);

private:
    const bool isEditable;

    // generic row manipulation functions
    void AddRow(DisplayRow *row);
    BlockMultipleAlignment * GetAlignmentForRow(unsigned int row) const;
    void UpdateBlockBoundaryRow(DisplayRowFromString *blockBoundaryRow) const;

    ViewerWindowBase* const *viewerWindow;

    unsigned int startingColumn;
    typedef std::vector < DisplayRow * > RowVector;
    RowVector rows;

    unsigned int maxRowWidth;
    void UpdateMaxRowWidth(void);
    void UpdateAfterEdit(const BlockMultipleAlignment *forAlignment);

    bool shiftDown, controlDown;

    void SortRows(void);

public:

    unsigned int NRows(void) const { return rows.size(); }
    bool IsEditable(void) const { return isEditable; }
    const Sequence * GetSequenceForRow(unsigned int row) const
    {
        if (row < NRows())
            return rows[row]->GetSequence();
        else
            return NULL;
    }

    // redraw all molecules associated with the SequenceDisplay
    void RedrawAlignedMolecules(void) const;

    // find coordinates of this residue in the display; returns false if not found
    bool GetDisplayCoordinates(const Molecule *molecule, unsigned int seqIndex,
        BlockMultipleAlignment::eUnalignedJustification justification, unsigned int *column, unsigned int *row);

    // column to scroll to when this display is first shown
    void SetStartingColumn(unsigned int column) { startingColumn = column; }
    unsigned int GetStartingColumn(void) const { return startingColumn; }

    // methods required by ViewableAlignment base class
    void GetSize(unsigned int *setColumns, unsigned int *setRows) const
        { *setColumns = maxRowWidth; *setRows = rows.size(); }
    bool GetRowTitle(unsigned int row, wxString *title, wxColour *color) const;
    bool GetCharacterTraitsAt(unsigned int column, unsigned int row,              // location
        char *character,										// character to draw
        wxColour *color,                                        // color of this character
        bool *drawBackground,                                   // special background color?
        wxColour *cellBackgroundColor
    ) const;

    // callbacks for ViewableAlignment
    void MouseOver(int column, int row) const;
    bool MouseDown(int column, int row, unsigned int controls);
    void SelectedRectangle(int columnLeft, int rowTop, int columnRight, int rowBottom);
    void DraggedCell(int columnFrom, int rowFrom, int columnTo, int rowTo);
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_DISPLAY__HPP
