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
*      Classes to manipulate alignments
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2000/10/12 19:20:37  thiessen
* working block deletion
*
* Revision 1.14  2000/10/12 16:22:37  thiessen
* working block split
*
* Revision 1.13  2000/10/12 02:14:32  thiessen
* working block boundary editing
*
* Revision 1.12  2000/10/05 18:34:35  thiessen
* first working editing operation
*
* Revision 1.11  2000/10/04 17:40:44  thiessen
* rearrange STL #includes
*
* Revision 1.10  2000/10/02 23:25:06  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.9  2000/09/20 22:22:02  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.8  2000/09/15 19:24:33  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.7  2000/09/11 22:57:55  thiessen
* working highlighting
*
* Revision 1.6  2000/09/11 14:06:02  thiessen
* working alignment coloring
*
* Revision 1.5  2000/09/11 01:45:52  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.4  2000/09/08 20:16:10  thiessen
* working dynamic alignment views
*
* Revision 1.3  2000/08/30 23:45:36  thiessen
* working alignment display
*
* Revision 1.2  2000/08/30 19:49:02  thiessen
* working sequence window
*
* Revision 1.1  2000/08/29 04:34:14  thiessen
* working alignment manager, IBM
*
* ===========================================================================
*/

#ifndef CN3D_ALIGNMENT_MANAGER__HPP
#define CN3D_ALIGNMENT_MANAGER__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <vector>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class SequenceSet;
class AlignmentSet;
class MasterSlaveAlignment;
class SequenceViewer;
class Messenger;
class ConservationColorer;


///// The next few classes make up the implementation of a multiple alignment /////

class Block; // defined below
class UngappedAlignedBlock;

class BlockMultipleAlignment
{
public:
    typedef std::vector < const Sequence * > SequenceList;
    BlockMultipleAlignment(const SequenceList *sequenceList, Messenger *messenger);

    ~BlockMultipleAlignment(void);

    const SequenceList *sequences;

    // add a new aligned block - will be "owned" and deallocated by BlockMultipleAlignment
    bool AddAlignedBlockAtEnd(UngappedAlignedBlock *newBlock);

    // these two should be called after all aligned blocks have been added; fills out
    // unaligned blocks inbetween aligned blocks (and at ends). Also sets length.
    bool AddUnalignedBlocks(void);

    // Fills out the BlockMap for mapping alignment column -> block+column,
    // and calculates conservation colors.
    bool UpdateBlockMapAndConservationColors(void);

    // find out if a residue is aligned - only works for non-repeated sequences!
    bool IsAligned(const Sequence *sequence, int seqIndex) const;

    // return true if this sequence is the master
    bool IsMaster(const Sequence *sequence) const
        { return (sequence == sequences->at(0)); }

    // return sequence for given row
    const Sequence * GetSequenceOfRow(int row) const
    {
        if (row >= 0 && row < sequences->size())
            return sequences->at(row);
        else
            return NULL;
    }
    
    // given a sequence, return row number in this alignment (or -1 if not found)
    int GetRowForSequence(const Sequence *sequence) const;

    // get a color for an aligned residue that's dependent on the entire alignment
    // (e.g., for coloring by sequence conservation)
    const Vector * GetAlignmentColor(const Sequence *sequence, int seqIndex) const;
    const Vector * GetAlignmentColor(int row, int seqIndex) const;

    // will be used to control padding of unaligned blocks
    enum eUnalignedJustification {
        eLeft,
        eRight,
        eCenter,
        eSplit
    };

    // return alignment position of left side first aligned block (-1 if no aligned blocks)
    int GetFirstAlignedBlockPosition(void) const;


    ///// editing functions /////

    // if in an aligned block, give block column and width of that position; otherwise, -1
    void GetAlignedBlockPosition(int alignmentIndex, int *blockColumn, int *blockWidth) const;

    // returns true if any boundary shift actually occurred
    bool MoveBlockBoundary(int columnFrom, int columnTo);

    // splits a block such that alignmentIndex is the first column of the new block;
    // returns false if no split occurred (e.g. if index is not inside aligned block)
    bool SplitBlock(int alignmentIndex);

    // merges all blocks that overlap specified range - assuming no unaligned blocks
    // in that range. Returns true if any merge(s) occurred, false otherwise.
    bool MergeBlocks(int fromAlignmentIndex, int toAlignmentIndex);

    // deletes the block containing this index; returns true if deletion occurred.
    bool DeleteBlock(int alignmentIndex);
    
private:
    ConservationColorer *conservationColorer;

    Messenger *messenger;

    typedef std::list < Block * > BlockList;
    BlockList blocks;
    int totalWidth;

    typedef struct {
        Block *block;
        int blockColumn;
    } BlockInfo;
    typedef std::vector < BlockInfo > BlockMap;
    BlockMap blockMap;

    bool CheckAlignedBlock(const Block *newBlock) const;
    Block * CreateNewUnalignedBlockBetween(const Block *left, const Block *right);
    Block * GetBlockBefore(const Block *block) const;
    Block * GetBlockAfter(const Block *block) const;
    void InsertBlockBefore(Block *newBlock, const Block *insertAt);
    void InsertBlockAfter(const Block *insertAt, Block *newBlock);
    void RemoveBlock(Block *block);
    
    eUnalignedJustification currentJustification;

    // for cacheing of residue->block lookups
    int prevRow;
    const Block *prevBlock;
    BlockList::const_iterator blockIterator;
    void InitCache(void);

    // given a row and seqIndex, find block that contains that residue
    const Block * GetBlock(int row, int seqIndex) const;

public:
    int NBlocks(void) const { return blocks.size(); }
    int NRows(void) const { return sequences->size(); }
    int AlignmentWidth(void) const { return totalWidth; }

    void SetUnalignedJustification(eUnalignedJustification j) { currentJustification = j; }

    // character query interface - "column" must be in alignment range
    // 0 ... totalWidth-1
    bool GetCharacterTraitsAt(int alignmentColumn, int row,
        char *character, Vector *color, bool *isHighlighted) const;

    // get sequence and index (if any) at given position, and whether that residue is aligned
    bool GetSequenceAndIndexAt(int alignmentColumn, int row,
        const Sequence **sequence, int *index, bool *isAligned) const;

    // called when user selects some part of a row
    void SelectedRange(int row, int from, int to) const;

};

// base class for Block - BlockMultipleAlignment is made up of a list of these
class Block
{
public:
    int width;

    virtual bool IsAligned(void) const = 0;

    typedef struct {
        int from, to;
    } Range;

    // get sequence index for a column, which must be in block range (0 ... width-1)
    virtual int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const = 0;

    typedef std::vector < const Sequence * > SequenceList;

protected:
    typedef std::vector < Range > RangeList;
    RangeList ranges;

    const SequenceList *sequences;

public:
    // given a row number (from 0 ... nSequences-1), give the sequence range covered by this block
    const Range* GetRangeOfRow(int row) const { return &(ranges[row]); }

    // set range
    void SetRangeOfRow(int row, int from, int to)
    {
        ranges[row].from = from;
        ranges[row].to = to;
    }

    Block(const SequenceList *sequenceList) :
        sequences(sequenceList), ranges(sequenceList->size()) { }

    int NSequences(void) const { return ranges.size(); }
};

// a gapless aligned block; width must be >= 1
class UngappedAlignedBlock : public Block
{
public:
    UngappedAlignedBlock(const SequenceList *sequenceList) : Block(sequenceList) { }

    bool IsAligned(void) const { return true; }

    int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification =
            BlockMultipleAlignment::eCenter) const  // justification ignored for aligned block
        { return (GetRangeOfRow(row)->from + blockColumn); }

    char GetCharacterAt(int blockColumn, int row) const;
};

// an unaligned block; max width of block must be >=1, but range over any given
// sequence can be length 0 (but not <0). If length 0, "to" is the residue after
// the block, and "from" (=to - 1) is the residue before.
class UnalignedBlock : public Block
{
public:
    UnalignedBlock(const SequenceList *sequenceList) : Block(sequenceList) { }

    bool IsAligned(void) const { return false; }

    int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const;
};


///// The next class is the actual AlignmentManager implementation /////

class AlignmentManager
{
public:
    AlignmentManager(const SequenceSet *sSet, AlignmentSet *aSet, Messenger *messenger);
    ~AlignmentManager(void);

    void NewAlignments(const SequenceSet *sSet, AlignmentSet *aSet);

    // creates the current multiple alignment from the given pairwise alignments (which are
    // assumed to be members of the AlignmentSet).
    typedef std::list < const MasterSlaveAlignment * > AlignmentList;
    BlockMultipleAlignment *
		CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments);

private:
    const SequenceSet *sequenceSet;
    AlignmentSet *alignmentSet;

    // viewer for the current alignment
    SequenceViewer *sequenceViewer;
    
    // for now, will own the current multiple alignment
    BlockMultipleAlignment *currentMultipleAlignment;

    Messenger *messenger;

public:
    //const BlockMultipleAlignment * GetCurrentMultipleAlignment(void) const
    //    { return currentMultipleAlignment; }

    // find out if a residue is aligned - only works for non-repeated sequences!
    bool IsAligned(const Sequence *sequence, int seqIndex) const
    { 
        if (currentMultipleAlignment)
            return currentMultipleAlignment->IsAligned(sequence, seqIndex);
        else
            return false;
    }

    // get a color for an aligned residue that's dependent on the entire alignment
    // (e.g., for coloring by sequence conservation)
    const Vector * GetAlignmentColor(const Sequence *sequence, int seqIndex) const
    {
        if (!currentMultipleAlignment) return NULL;
        return currentMultipleAlignment->GetAlignmentColor(sequence, seqIndex);
    }

};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_MANAGER__HPP
