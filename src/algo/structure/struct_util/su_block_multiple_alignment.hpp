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

#ifndef SU_BLOCK_MULTIPLE_ALIGNMENT__HPP
#define SU_BLOCK_MULTIPLE_ALIGNMENT__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <list>
#include <vector>
#include <map>


BEGIN_SCOPE(struct_util)

class Sequence;
class Block;
class UngappedAlignedBlock;
class UnalignedBlock;

class BlockMultipleAlignment : public ncbi::CObject
{
public:
    static const unsigned int UNDEFINED;

    typedef std::vector < const Sequence * > SequenceList;
    BlockMultipleAlignment(const SequenceList& sequenceList);   // first sequence is master
    ~BlockMultipleAlignment(void);

    // create a C-object SeqAlign from this alignment (actually a linked list of pairwise
    // SeqAlign's; should be freed with SeqAlignSetFree())
//    SeqAlignPtr CreateCSeqAlign(void) const;

    // add a new aligned block - will be "owned" and deallocated by BlockMultipleAlignment
    bool AddAlignedBlockAtEnd(UngappedAlignedBlock *newBlock);

    // these two should be called after all aligned blocks have been added; fills out
    // unaligned blocks inbetween aligned blocks (and at ends). Also sets length.
    bool AddUnalignedBlocks(void);

    // Fills out the BlockMap for mapping alignment column -> block+column, special colors,
    // and sets up conservation colors (although they're not calculated until needed).
    bool UpdateBlockMap(bool clearRowInfo = true);

    // find out if a residue is aligned, by row
    bool IsAligned(unsigned int row, unsigned int seqIndex) const;

    // stuff regarding master sequence
    const Sequence * GetMaster(void) const { return m_sequences[0]; }
    bool IsMaster(const Sequence *sequence) const { return (sequence == m_sequences[0]); }

    // return sequence for given row
    const Sequence * GetSequenceOfRow(unsigned int row) const
    {
        if (row < m_sequences.size())
            return m_sequences[row];
        else
            return NULL;
    }

    // will be used to control padding of unaligned blocks
    enum eUnalignedJustification {
        eLeft,
        eRight,
        eCenter,
        eSplit
    };

    // return alignment position of left side first aligned block (UNDEFINED if no aligned blocks)
    unsigned int GetFirstAlignedBlockPosition(void) const;

    // makes a new copy of itself
    BlockMultipleAlignment * Clone(void) const;

    // character query interface - "column" must be in alignment range [0 .. AlignmentWidth()-1]
    bool GetCharacterAt(unsigned int alignmentColumn, unsigned int row,
        eUnalignedJustification justification, char *character) const;

    // get sequence and index (if any) at given position, and whether that residue is aligned
    bool GetSequenceAndIndexAt(unsigned int alignmentColumn, unsigned int row,
        eUnalignedJustification justification,
        const Sequence **sequence, unsigned int *index, bool *isAligned) const;

    // given row and sequence index, return alignment index; not the most efficient function - use sparingly
    unsigned int GetAlignmentIndex(unsigned int row, unsigned int seqIndex, eUnalignedJustification justification);

    // fill in a vector of UngappedAlignedBlocks
    typedef std::vector < const UngappedAlignedBlock * > UngappedAlignedBlockList;
    void GetUngappedAlignedBlocks(UngappedAlignedBlockList *blocks) const;

    // PSSM for this alignment (cached)
//    const BLAST_Matrix * GetPSSM(void) const;
//    void RemovePSSM(void) const;

    // NULL if block before is aligned; if NULL passed, retrieves last block (if unaligned; else NULL)
    const UnalignedBlock * GetUnalignedBlockBefore(const UngappedAlignedBlock *aBlock) const;

    unsigned int NBlocks(void) const { return m_blocks.size(); }
    unsigned int NAlignedBlocks(void) const;
    unsigned int NRows(void) const { return m_sequences.size(); }
    unsigned int AlignmentWidth(void) const { return m_totalWidth; }

    // return a number from 1..n for aligned blocks, UNDEFINED for unaligned
    unsigned int GetAlignedBlockNumber(unsigned int alignmentIndex) const
        { return m_blockMap[alignmentIndex].alignedBlockNum; }

    // for storing/querying info
    double GetRowDouble(unsigned int row) const { return m_rowDoubles[row]; }
    void SetRowDouble(unsigned int row, double value) const { m_rowDoubles[row] = value; }
    const std::string& GetRowStatusLine(unsigned int row) const { return m_rowStrings[row]; }
    void SetRowStatusLine(unsigned int row, const std::string& value) const { m_rowStrings[row] = value; }

    // empties all rows' infos
    void ClearRowInfo(void) const
    {
        for (unsigned int r=0; r<NRows(); ++r) {
            m_rowDoubles[r] = 0.0;
            m_rowStrings[r].erase();
        }
    }


    ///// editing functions /////

    // if in an aligned block, give block column and width of that position; otherwise UNDEFINED
    void GetAlignedBlockPosition(unsigned int alignmentIndex,
        unsigned int *blockColumn, unsigned int *blockWidth) const;

    // get seqIndex of slave aligned to the given master seqIndex; UNDEFINED if master residue unaligned
    unsigned int GetAlignedSlaveIndex(unsigned int masterSeqIndex, unsigned int slaveRow) const;

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
    bool CreateBlock(unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex,
        eUnalignedJustification justification);

    // deletes the block containing this index; returns true if deletion occurred.
    bool DeleteBlock(unsigned int alignmentIndex);

    // deletes all blocks; returns true if there were any blocks to delete
    bool DeleteAllBlocks(void);

    // shifts (horizontally) the residues in and immediately surrounding an
    // aligned block; returns true if any shift occurs.
    bool ShiftRow(unsigned int row, unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex,
        eUnalignedJustification justification);

    // delete a row; returns true if successful
    bool DeleteRow(unsigned int row);

    // flag an aligned block for realignment - block will be removed upon ExtractRows; returns true if
    // column is in fact an aligned block
    bool MarkBlock(unsigned int column);
    bool ClearMarks(void);  // remove all block flags

    // this function does two things: it extracts from a multiple alignment all slave rows listed for
    // removal; and if pairwiseAlignments!=NULL, for each slave removed creates a new BlockMultipleAlignment
    // that contains the alignment of just that slave with the master, as it was in the original multiple
    // (i.e., not according to the corresponding pre-IBM MasterSlaveAlignment)
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    bool ExtractRows(const std::vector < unsigned int >& slavesToRemove, AlignmentList *pairwiseAlignments);

    // merge in the contents of the given alignment (assuming same master, compatible block structure),
    // addings its rows to the end of this alignment; returns true if merge successful. Does not change
    // block structure - just adds the part of new alignment's aligned blocks that intersect with this
    // object's aligned blocks
    bool MergeAlignment(const BlockMultipleAlignment *newAlignment);

private:
    SequenceList m_sequences;

    typedef std::list < ncbi::CRef < Block > > BlockList;
    BlockList m_blocks;

    typedef struct {
        Block *block;
        unsigned int blockColumn, alignedBlockNum;
    } BlockInfo;
    typedef std::vector < BlockInfo > BlockMap;
    BlockMap m_blockMap;

    unsigned int m_totalWidth;

    bool CheckAlignedBlock(const Block *newBlock) const;
    UnalignedBlock * CreateNewUnalignedBlockBetween(const Block *left, const Block *right);
    Block * GetBlockBefore(const Block *block);
    Block * GetBlockAfter(const Block *block);
    const Block * GetBlockBefore(const Block *block) const;
    const Block * GetBlockAfter(const Block *block) const;
    void InsertBlockBefore(Block *newBlock, const Block *insertAt);
    void InsertBlockAfter(const Block *insertAt, Block *newBlock);
    void RemoveBlock(Block *block);

    // for cacheing of residue->block lookups
    mutable unsigned int m_cachePrevRow;
    mutable const Block *m_cachePrevBlock;
    mutable BlockList::const_iterator m_cacheBlockIterator;
    void InitCache(void);

    // given a row and seqIndex, find block that contains that residue
    const Block * GetBlock(unsigned int row, unsigned int seqIndex) const;

    // intended for volatile storage of row-associated info (e.g. for alignment scores, etc.)
    mutable std::vector < double > m_rowDoubles;
    mutable std::vector < std::string > m_rowStrings;

    // associated PSSM
//    mutable BLAST_Matrix *m_pssm;
};


// static function to create Seq-aligns out of multiple
ncbi::objects::CSeq_align * CreatePairwiseSeqAlignFromMultipleRow(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment::UngappedAlignedBlockList& blocks, unsigned int slaveRow);


// base class for Block - BlockMultipleAlignment is made up of a list of these
class Block : public ncbi::CObject
{
public:
    virtual ~Block(void) { }    // virtual destructor for base class

    // makes a new copy of itself
    virtual Block * Clone(const BlockMultipleAlignment *newMultiple) const = 0;

    unsigned int m_width;

    virtual bool IsAligned(void) const = 0;

    // get sequence index for a column, which must be in block range (0 ... width-1)
    virtual unsigned int GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const = 0;

    // delete a row
    virtual void DeleteRow(unsigned int row) = 0;
    virtual void DeleteRows(std::vector < bool >& removeRows, unsigned int nToRemove) = 0;

    bool IsFrom(const BlockMultipleAlignment *alignment) const { return (alignment == m_parentAlignment); }

    // given a row number (from 0 ... nSequences-1), give the sequence range covered by this block
    typedef struct {
        unsigned int from, to;
    } Range;

    const Range* GetRangeOfRow(int row) const { return &(m_ranges[row]); }
    void SetRangeOfRow(unsigned int row, unsigned int from, unsigned int to)
    {
        m_ranges[row].from = from;
        m_ranges[row].to = to;
    }

    // resize - add new (empty) rows at end
    void AddRows(unsigned int nNewRows) { m_ranges.resize(m_ranges.size() + nNewRows); }

    unsigned int NSequences(void) const { return m_ranges.size(); }

protected:
    Block(const BlockMultipleAlignment *multiple) :
        m_parentAlignment(multiple), m_ranges(multiple->NRows()) { }

    const BlockMultipleAlignment *m_parentAlignment;

    typedef std::vector < Range > RangeList;
    RangeList m_ranges;
};


// a gapless aligned block; width must be >= 1
class UngappedAlignedBlock : public Block
{
public:
    UngappedAlignedBlock(const BlockMultipleAlignment *multiple) : Block(multiple) { }

    bool IsAligned(void) const { return true; }

    unsigned int GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification =
            BlockMultipleAlignment::eCenter) const  // justification ignored for aligned block
        { return (GetRangeOfRow(row)->from + blockColumn); }

    char GetCharacterAt(unsigned int blockColumn, unsigned int row) const;

    void DeleteRow(unsigned int row);
    void DeleteRows(std::vector < bool >& removeRows, unsigned int nToRemove);

    Block * Clone(const BlockMultipleAlignment *newMultiple) const;
};


// an unaligned block; max width of block must be >=1. But range over any given
// sequence can be length 0 (but not <0); if length 0, "to" is the residue before
// the block, and "from" (= to+1) is the residue after.
class UnalignedBlock : public Block
{
public:
    UnalignedBlock(const BlockMultipleAlignment *multiple) : Block(multiple) { }

    void Resize(void);  // recompute block width from current ranges

    bool IsAligned(void) const { return false; }

    unsigned int GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const;

    void DeleteRow(unsigned int row);
    void DeleteRows(std::vector < bool >& removeRows, unsigned int nToRemove);

    Block * Clone(const BlockMultipleAlignment *newMultiple) const;
};

END_SCOPE(struct_util)

#endif // SU_BLOCK_MULTIPLE_ALIGNMENT__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/05/25 16:12:30  thiessen
* fix GCC warnings
*
* Revision 1.1  2004/05/25 15:52:18  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
*/
