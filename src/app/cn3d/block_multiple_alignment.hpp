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
*      Classes to hold alignment data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.1  2000/11/30 15:49:06  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* ===========================================================================
*/

#ifndef CN3D_BLOCK_MULTIPLE_ALIGNMENT__HPP
#define CN3D_BLOCK_MULTIPLE_ALIGNMENT__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <vector>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class ConservationColorer;

class Block;
class UngappedAlignedBlock;
class UnalignedBlock;
class Messenger;

class BlockMultipleAlignment
{
public:
    typedef std::vector < const Sequence * > SequenceList;
    BlockMultipleAlignment(const SequenceList *sequenceList);

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

    // stuff regarding master sequence
    const Sequence * GetMaster(void) const { return sequences->at(0); }
    bool IsMaster(const Sequence *sequence) const { return (sequence == sequences->at(0)); }

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

    // makes a new copy of itself
    BlockMultipleAlignment * Clone(void) const;

    // character query interface - "column" must be in alignment range
    // 0 ... totalWidth-1
    bool GetCharacterTraitsAt(int alignmentColumn, int row, eUnalignedJustification justification,
        char *character, Vector *color, bool *isHighlighted) const;

    // get sequence and index (if any) at given position, and whether that residue is aligned
    bool GetSequenceAndIndexAt(int alignmentColumn, int row, eUnalignedJustification justification,
        const Sequence **sequence, int *index, bool *isAligned) const;

    // called when user selects some part of a row
    void SelectedRange(int row, int from, int to, eUnalignedJustification justification) const;

    // get a list of UngappedAlignedBlock's; should be destroyed by called when done
    typedef std::list < const UngappedAlignedBlock * > UngappedAlignedBlockList;
    UngappedAlignedBlockList *GetUngappedAlignedBlocks(void) const;


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

    // creates a block, if given region of an unaligned block in which no gaps
    // are present. Returns true if a block is created.
    bool CreateBlock(int fromAlignmentIndex, int toAlignmentIndex, eUnalignedJustification justification);

    // deletes the block containing this index; returns true if deletion occurred.
    bool DeleteBlock(int alignmentIndex);

    // shifts (horizontally) the residues in and immediately surrounding an
    // aligned block; retunrs true if any shift occurs.
    bool ShiftRow(int row, int fromAlignmentIndex, int toAlignmentIndex);

private:
    ConservationColorer *conservationColorer;

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
    UnalignedBlock * CreateNewUnalignedBlockBetween(const Block *left, const Block *right);
    Block * GetBlockBefore(const Block *block) const;
    Block * GetBlockAfter(const Block *block) const;
    void InsertBlockBefore(Block *newBlock, const Block *insertAt);
    void InsertBlockAfter(const Block *insertAt, Block *newBlock);
    void RemoveBlock(Block *block);

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

    // makes a new copy of itself
    virtual Block * Clone(const BlockMultipleAlignment *newMultiple) const = 0;

protected:
    typedef std::vector < Range > RangeList;
    RangeList ranges;

    const BlockMultipleAlignment *parentAlignment;

public:
    // given a row number (from 0 ... nSequences-1), give the sequence range covered by this block
    const Range* GetRangeOfRow(int row) const { return &(ranges[row]); }

    // set range
    void SetRangeOfRow(int row, int from, int to)
    {
        ranges[row].from = from;
        ranges[row].to = to;
    }

    Block(const BlockMultipleAlignment *multiple) :
        parentAlignment(multiple), ranges(multiple->NRows()) { }

    int NSequences(void) const { return ranges.size(); }
};


// a gapless aligned block; width must be >= 1
class UngappedAlignedBlock : public Block
{
public:
    UngappedAlignedBlock(const BlockMultipleAlignment *multiple) : Block(multiple) { }

    bool IsAligned(void) const { return true; }

    int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification =
            BlockMultipleAlignment::eCenter) const  // justification ignored for aligned block
        { return (GetRangeOfRow(row)->from + blockColumn); }

    char GetCharacterAt(int blockColumn, int row) const;

    Block * Clone(const BlockMultipleAlignment *newMultiple) const;
};


// an unaligned block; max width of block must be >=1, but range over any given
// sequence can be length 0 (but not <0). If length 0, "to" is the residue after
// the block, and "from" (=to - 1) is the residue before.
class UnalignedBlock : public Block
{
public:
    UnalignedBlock(const BlockMultipleAlignment *multiple) : Block(multiple) { }

    bool IsAligned(void) const { return false; }

    int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const;

    Block * Clone(const BlockMultipleAlignment *newMultiple) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_BLOCK_MULTIPLE_ALIGNMENT__HPP
