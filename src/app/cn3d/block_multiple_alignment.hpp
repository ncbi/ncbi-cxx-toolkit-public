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
* ===========================================================================
*/

#ifndef CN3D_BLOCK_MULTIPLE_ALIGNMENT__HPP
#define CN3D_BLOCK_MULTIPLE_ALIGNMENT__HPP

#include <corelib/ncbistl.hpp>

#include <objects/cdd/Update_align.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <list>
#include <vector>
#include <map>

#include "vector_math.hpp"
#include "style_manager.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class ConservationColorer;
class AlignmentManager;
class Block;
class UngappedAlignedBlock;
class UnalignedBlock;
class Messenger;
class Threader;
class PSSMWrapper;

class BlockMultipleAlignment
{
public:
    typedef std::vector < const Sequence * > SequenceList;
    // list will be owned/freed by this object
    BlockMultipleAlignment(SequenceList *sequenceList, AlignmentManager *alnMgr);

    ~BlockMultipleAlignment(void);

    const SequenceList * GetSequences(void) const { return sequences; }

    // to track the origin of this alignment if it came from an update
    ncbi::CRef < ncbi::objects::CUpdate_align > updateOrigin;

    // add a new aligned block - will be "owned" and deallocated by BlockMultipleAlignment
    bool AddAlignedBlockAtEnd(UngappedAlignedBlock *newBlock);

    // these two should be called after all aligned blocks have been added; fills out
    // unaligned blocks inbetween aligned blocks (and at ends). Also sets length.
    bool AddUnalignedBlocks(void);

    // Fills out the BlockMap for mapping alignment column -> block+column, special colors,
    // and sets up conservation colors (although they're not calculated until needed).
    bool UpdateBlockMapAndColors(bool clearRowInfo = true);

    // find out if a residue is aligned, by row
    bool IsAligned(unsigned int row, unsigned int seqIndex) const;

    // find out if a residue is aligned, by Sequence - only works for non-repeated Sequences!
    bool IsAligned(const Sequence *sequence, unsigned int seqIndex) const
    {
        int row = GetRowForSequence(sequence);
        if (row < 0) return false;
        return IsAligned(row, seqIndex);
    }

    // stuff regarding master sequence
    const Sequence * GetMaster(void) const { return (*sequences)[0]; }
    bool IsMaster(const Sequence *sequence) const { return (sequence == (*sequences)[0]); }

    // return sequence for given row
    const Sequence * GetSequenceOfRow(unsigned int row) const
    {
        if (row < sequences->size())
            return (*sequences)[row];
        else
            return NULL;
    }

    // given a sequence, return row number in this alignment (or -1 if not found)
    int GetRowForSequence(const Sequence *sequence) const;

    // get a color for an aligned residue that's dependent on the entire alignment
    // (e.g., for coloring by sequence conservation)
    const Vector * GetAlignmentColor(const Sequence *sequence, unsigned int seqIndex,
        StyleSettings::eColorScheme colorScheme) const;
    const Vector * GetAlignmentColor(unsigned int row, unsigned int seqIndex,
        StyleSettings::eColorScheme colorScheme) const;

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

    // character query interface - "column" must be in alignment range [0 .. totalWidth-1]
    bool GetCharacterTraitsAt(unsigned int alignmentColumn, unsigned int row, eUnalignedJustification justification,
        char *character, Vector *color, bool *isHighlighted,
        bool *drawBackground, Vector *cellBackgroundColor) const;

    // get sequence and index (if any) at given position, and whether that residue is aligned
    bool GetSequenceAndIndexAt(unsigned int alignmentColumn, unsigned int row, eUnalignedJustification justification,
        const Sequence **sequence, int *index, bool *isAligned) const;

    // given row and sequence index, return alignment index; not the most efficient function - use sparingly
    int GetAlignmentIndex(unsigned int row, unsigned int seqIndex, eUnalignedJustification justification) const;

    // called when user selects some part of a row
    void SelectedRange(unsigned int row, unsigned int alnIndexFrom, unsigned int alnIndexTo,
        eUnalignedJustification justification, bool toggle) const;
    void SelectBlocks(unsigned int alnIndexFrom, unsigned int alnIndexTo, bool toggle) const;

    // fill in a vector of Blocks
    typedef std::vector < const Block * > ConstBlockList;
    void GetBlocks(ConstBlockList *blocks) const;

    // fill in a vector of UngappedAlignedBlocks; returns # aligned blocks
    typedef std::vector < const UngappedAlignedBlock * > UngappedAlignedBlockList;
    unsigned int GetUngappedAlignedBlocks(UngappedAlignedBlockList *blocks) const;

    // free color storage
    void FreeColors(void);

    // highlight aligned columns based on master indexes (mainly for alignment annotations);
    // returns false if any residue in the range is unaligned (or out of range), true on success
    bool HighlightAlignedColumnsOfMasterRange(unsigned int from, unsigned int to) const;

    // PSSM for this alignment (cached)
    const PSSMWrapper& GetPSSM(void) const;
    void RemovePSSM(void) const;


    ///// editing functions /////

    // if in an aligned block, give block column and width of that position; otherwise, -1
    void GetAlignedBlockPosition(unsigned int alignmentIndex, int *blockColumn, int *blockWidth) const;

    // get seqIndex of dependent aligned to the given master seqIndex; -1 if master residue unaligned
    int GetAlignedDependentIndex(unsigned int masterSeqIndex, unsigned int dependentRow) const;

    // returns true if any boundary shift actually occurred
    bool MoveBlockBoundary(unsigned int columnFrom, unsigned int columnTo);

    // splits a block such that alignmentIndex is the first column of the new block;
    // returns false if no split occurred (e.g. if index is not inside aligned block)
    bool SplitBlock(unsigned int alignmentIndex);

    // merges all blocks that overlap specified range - assuming no unaligned blocks
    // in that range. Returns true if any merge(s) occurred, false otherwise.
    bool MergeBlocks(unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex);

    // creates a block, if given region of an unaligned block in which no gaps
    // are present. Returns true if a block is created.
    bool CreateBlock(unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex, eUnalignedJustification justification);

    // deletes the block containing this index; returns true if deletion occurred.
    bool DeleteBlock(unsigned int alignmentIndex);

    // deletes all blocks; returns true if there were any blocks to delete
    bool DeleteAllBlocks(void);

    // shifts (horizontally) the residues in and immediately surrounding an
    // aligned block; returns true if any shift occurs.
    bool ShiftRow(unsigned int row, unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex, eUnalignedJustification justification);

    // only if the residue is unaligned, move it into the first aligned position of the adjacent aligned block
    bool ZipAlignResidue(unsigned int row, unsigned int alignmentIndex, bool moveRight, eUnalignedJustification justification);

    // scans for the best position of a particular block; returns true if the block moved
    bool OptimizeBlock(unsigned int row, unsigned int alignmentIndex, eUnalignedJustification justification);

    // delete a row; returns true if successful
    bool DeleteRow(unsigned int row);

    // flag an aligned block for realignment - block will be removed upon ExtractRows; returns true if
    // column is in fact an aligned block
    bool MarkBlock(unsigned int column);
    bool ClearMarks(void);  // remove all block flags
    void GetMarkedBlockNumbers(std::vector < unsigned int > *alignedBlockNumbers) const;

    // this function does two things: it extracts from a multiple alignment all dependent rows listed for
    // removal; and if pairwiseAlignments!=NULL, for each dependent removed creates a new BlockMultipleAlignment
    // that contains the alignment of just that dependent with the master, as it was in the original multiple
    // (i.e., not according to the corresponding pre-IBM MasterDependentAlignment)
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    bool ExtractRows(const std::vector < unsigned int >& dependentsToRemove, AlignmentList *pairwiseAlignments);

    // merge in the contents of the given alignment (assuming same master, compatible block structure),
    // addings its rows to the end of this alignment; returns true if merge successful. Does not change
    // block structure - just adds the part of new alignment's aligned blocks that intersect with this
    // object's aligned blocks
    bool MergeAlignment(const BlockMultipleAlignment *newAlignment);

    // turn on/off geometry violations; if param is true, will return # violations found
    unsigned int ShowGeometryViolations(bool showGeometryViolations);

private:
    AlignmentManager *alignmentManager;
    SequenceList *sequences;
    ConservationColorer *conservationColorer;
    mutable PSSMWrapper *pssm;

    typedef std::list < Block * > BlockList;
    BlockList blocks;

    unsigned int totalWidth;

    typedef struct {
        Block *block;
        int blockColumn, alignedBlockNum;
    } BlockInfo;
    typedef std::vector < BlockInfo > BlockMap;
    BlockMap blockMap;

    // to flag blocks for realignment
    typedef std::map < const Block * , bool > MarkBlockMap;
    MarkBlockMap markBlocks;

    bool CheckAlignedBlock(const Block *newBlock) const;
    UnalignedBlock * CreateNewUnalignedBlockBetween(const Block *left, const Block *right);
    Block * GetBlockBefore(const Block *block) const;
    Block * GetBlockAfter(const Block *block) const;
    void InsertBlockBefore(Block *newBlock, const Block *insertAt);
    void InsertBlockAfter(const Block *insertAt, Block *newBlock);
    void RemoveBlock(Block *block);

    // for cacheing of residue->block lookups
    mutable unsigned int cachePrevRow;
    mutable const Block *cachePrevBlock;
    mutable BlockList::const_iterator cacheBlockIterator;
    void InitCache(void);

    // given a row and seqIndex, find block that contains that residue
    const Block * GetBlock(unsigned int row, unsigned int seqIndex) const;

    // intended for volatile storage of row-associated info (e.g. for alignment scores, etc.)
    mutable std::vector < double > rowDoubles;
    mutable std::vector < std::string > rowStrings;

    // for flagging residues (e.g. for geometry violations)
    typedef std::vector < std::vector < bool > > RowSeqIndexFlags;
    RowSeqIndexFlags geometryViolations;
    bool showGeometryViolations;

    unsigned int SumOfAlignedBlockWidths(void) const;

public:

    // NULL if block before is aligned; if NULL passed, retrieves last block (if unaligned; else NULL)
    const UnalignedBlock * GetUnalignedBlockBefore(const UngappedAlignedBlock *aBlock) const;
    // NULL if block after is aligned; if NULL passed, retrieves first block (if unaligned; else NULL)
    const UnalignedBlock * GetUnalignedBlockAfter(const UngappedAlignedBlock *aBlock) const;

    unsigned int NBlocks(void) const { return blocks.size(); }
    bool HasNoAlignedBlocks(void) const;
    unsigned int NAlignedBlocks(void) const;
    unsigned int NRows(void) const { return sequences->size(); }
    unsigned int AlignmentWidth(void) const { return totalWidth; }

    // return a number from 1..n for aligned blocks, -1 for unaligned
    int GetAlignedBlockNumber(unsigned int alignmentIndex) const { return blockMap[alignmentIndex].alignedBlockNum; }

    // returns the loop length of the unaligned region at the given row and alignment index; -1 if that position is aligned
    int GetLoopLength(unsigned int row, unsigned int alignmentIndex);

    // return [0..1] for aligned residues depending on the alignment position - first aligned column is 0, last is 1;
    // -1 if unaligned. This is mainly for alignment rainbow coloring
    double GetRelativeAlignmentFraction(unsigned int alignmentIndex) const;

    // for storing/querying info
    double GetRowDouble(unsigned int row) const { return rowDoubles[row]; }
    void SetRowDouble(unsigned int row, double value) const { rowDoubles[row] = value; }
    const std::string& GetRowStatusLine(unsigned int row) const { return rowStrings[row]; }
    void SetRowStatusLine(unsigned int row, const std::string& value) const { rowStrings[row] = value; }

    // empties all rows' infos
    void ClearRowInfo(void) const
    {
        for (unsigned int r=0; r<NRows(); ++r) {
            rowDoubles[r] = 0.0;
            rowStrings[r].erase();
        }
    }

    // kludge for now for storing allowed alignment regions, e.g. when demoted from multiple.
    // (only two ranges for now, since this is used only with pairwise alignments)
    int alignMasterFrom, alignMasterTo, alignDependentFrom, alignDependentTo;
};


// static function to create Seq-aligns out of multiple
ncbi::objects::CSeq_align * CreatePairwiseSeqAlignFromMultipleRow(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment::UngappedAlignedBlockList& blocks, unsigned int dependentRow);


// base class for Block - BlockMultipleAlignment is made up of a list of these
class Block
{
public:
    unsigned int width;

    virtual bool IsAligned(void) const = 0;

    typedef struct {
        int from, to;   // signed because 'to' may be -1 (zero-width block starting at index zero)
    } Range;

    // get sequence index for a column, which must be in block range (0 ... width-1)
    virtual int GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const = 0;

    // delete a row
    virtual void DeleteRow(unsigned int row) = 0;
    virtual void DeleteRows(std::vector < bool >& removeRows, unsigned int nToRemove) = 0;

    // makes a new copy of itself
    virtual Block * Clone(const BlockMultipleAlignment *newMultiple) const = 0;

protected:
    Block(const BlockMultipleAlignment *multiple) :
        width(0), ranges(multiple->NRows()), parentAlignment(multiple)  { }

    typedef std::vector < Range > RangeList;
    RangeList ranges;

    const BlockMultipleAlignment *parentAlignment;

public:
    virtual ~Block(void) { }    // virtual destructor for base class

    bool IsFrom(const BlockMultipleAlignment *alignment) const { return (alignment == parentAlignment); }

    // given a row number (from 0 ... nSequences-1), give the sequence range covered by this block
    const Range* GetRangeOfRow(unsigned int row) const { return &(ranges[row]); }

    // set range
    void SetRangeOfRow(unsigned int row, int from, int to)
    {
        ranges[row].from = from;
        ranges[row].to = to;
    }

    // resize - add new (empty) rows at end
    void AddRows(unsigned int nNewRows) { ranges.resize(ranges.size() + nNewRows); }

    unsigned int NSequences(void) const { return ranges.size(); }
};


// a gapless aligned block; width must be >= 1
class UngappedAlignedBlock : public Block
{
public:
    UngappedAlignedBlock(const BlockMultipleAlignment *multiple) : Block(multiple) { }

    bool IsAligned(void) const { return true; }

    int GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification =
            BlockMultipleAlignment::eCenter) const  // justification ignored for aligned block
        { return (GetRangeOfRow(row)->from + blockColumn); }

    char GetCharacterAt(unsigned int blockColumn, unsigned int row) const;

    void DeleteRow(unsigned int row);
    void DeleteRows(std::vector < bool >& removeRows, unsigned int nToRemove);

    Block * Clone(const BlockMultipleAlignment *newMultiple) const;
};


// an unaligned block; max width of block must be >=1, but range over any given
// sequence can be length 0 (but not <0). If length 0, "to" is the residue after
// the block, and "from" (=to - 1) is the residue before.
class UnalignedBlock : public Block
{
public:
    UnalignedBlock(const BlockMultipleAlignment *multiple) : Block(multiple) { }

    void Resize(void);  // recompute block width from current ranges

    bool IsAligned(void) const { return false; }

    int GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const;

    void DeleteRow(unsigned int row);
    void DeleteRows(std::vector < bool >& removeRows, unsigned int nToRemove);

    Block * Clone(const BlockMultipleAlignment *newMultiple) const;

    // return the length of the shortest region that any row contributes to this block
    unsigned int MinResidues(void) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_BLOCK_MULTIPLE_ALIGNMENT__HPP
