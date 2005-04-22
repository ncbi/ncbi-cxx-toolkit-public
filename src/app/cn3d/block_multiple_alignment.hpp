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

#include <blastkar.h>
#include <thrdatd.h>
#include <thrddecl.h>

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

class BlockMultipleAlignment
{
public:
    typedef std::vector < const Sequence * > SequenceList;
    // list will be owned/freed by this object
    BlockMultipleAlignment(SequenceList *sequenceList, AlignmentManager *alnMgr);

    ~BlockMultipleAlignment(void);

    const SequenceList * GetSequences(void) const { return sequences; }
    AlignmentManager *alignmentManager;

    // create a C-object SeqAlign from this alignment (actually a linked list of pairwise
    // SeqAlign's; should be freed with SeqAlignSetFree())
    SeqAlignPtr CreateCSeqAlign(void) const;

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
    bool IsAligned(int row, int seqIndex) const;

    // find out if a residue is aligned, by Sequence - only works for non-repeated Sequences!
    bool IsAligned(const Sequence *sequence, int seqIndex) const
    {
        int row = GetRowForSequence(sequence);
        if (row < 0) return false;
        return IsAligned(row, seqIndex);
    }

    // stuff regarding master sequence
    const Sequence * GetMaster(void) const { return (*sequences)[0]; }
    bool IsMaster(const Sequence *sequence) const { return (sequence == (*sequences)[0]); }

    // return sequence for given row
    const Sequence * GetSequenceOfRow(int row) const
    {
        if (row >= 0 && row < sequences->size())
            return (*sequences)[row];
        else
            return NULL;
    }

    // given a sequence, return row number in this alignment (or -1 if not found)
    int GetRowForSequence(const Sequence *sequence) const;

    // get a color for an aligned residue that's dependent on the entire alignment
    // (e.g., for coloring by sequence conservation)
    const Vector * GetAlignmentColor(const Sequence *sequence, int seqIndex,
        StyleSettings::eColorScheme colorScheme) const;
    const Vector * GetAlignmentColor(int row, int seqIndex,
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
    bool GetCharacterTraitsAt(int alignmentColumn, int row, eUnalignedJustification justification,
        char *character, Vector *color, bool *isHighlighted,
        bool *drawBackground, Vector *cellBackgroundColor) const;

    // get sequence and index (if any) at given position, and whether that residue is aligned
    bool GetSequenceAndIndexAt(int alignmentColumn, int row, eUnalignedJustification justification,
        const Sequence **sequence, int *index, bool *isAligned) const;

    // given row and sequence index, return alignment index; not the most efficient function - use sparingly
    int GetAlignmentIndex(int row, int seqIndex, eUnalignedJustification justification) const;

    // called when user selects some part of a row
    void SelectedRange(int row, int alnIndexFrom, int alnIndexTo, eUnalignedJustification justification, bool toggle) const;
    void SelectBlocks(int alnIndexFrom, int alnIndexTo, bool toggle) const;

    // fill in a vector of Blocks
    typedef std::vector < const Block * > ConstBlockList;
    void GetBlocks(ConstBlockList *blocks) const;

    // fill in a vector of UngappedAlignedBlocks
    typedef std::vector < const UngappedAlignedBlock * > UngappedAlignedBlockList;
    void GetUngappedAlignedBlocks(UngappedAlignedBlockList *blocks) const;

    // free color storage
    void FreeColors(void);

    // highlight aligned columns based on master indexes (mainly for alignment annotations);
    // returns false if any residue in the range is unaligned (or out of range), true on success
    bool HighlightAlignedColumnsOfMasterRange(int from, int to) const;

    // PSSM for this alignment (cached)
    const BLAST_Matrix * GetPSSM(void) const;
    void RemovePSSM(void) const;


    ///// editing functions /////

    // if in an aligned block, give block column and width of that position; otherwise, -1
    void GetAlignedBlockPosition(int alignmentIndex, int *blockColumn, int *blockWidth) const;

    // get seqIndex of slave aligned to the given master seqIndex; -1 if master residue unaligned
    int GetAlignedSlaveIndex(int masterSeqIndex, int slaveRow) const;

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

    // deletes all blocks; returns true if there were any blocks to delete
    bool DeleteAllBlocks(void);

    // shifts (horizontally) the residues in and immediately surrounding an
    // aligned block; returns true if any shift occurs.
    bool ShiftRow(int row, int fromAlignmentIndex, int toAlignmentIndex, eUnalignedJustification justification);

    // only if the residue is unaligned, move it into the first aligned position of the adjacent aligned block
    bool ZipAlignResidue(int row, int alignmentIndex, bool moveRight, eUnalignedJustification justification);

    // scans for the best position of a particular block; returns true if the block moved
    bool OptimizeBlock(int row, int alignmentIndex, eUnalignedJustification justification);

    // delete a row; returns true if successful
    bool DeleteRow(int row);

    // flag an aligned block for realignment - block will be removed upon ExtractRows; returns true if
    // column is in fact an aligned block
    bool MarkBlock(int column);
    bool ClearMarks(void);  // remove all block flags

    // this function does two things: it extracts from a multiple alignment all slave rows listed for
    // removal; and if pairwiseAlignments!=NULL, for each slave removed creates a new BlockMultipleAlignment
    // that contains the alignment of just that slave with the master, as it was in the original multiple
    // (i.e., not according to the corresponding pre-IBM MasterSlaveAlignment)
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    bool ExtractRows(const std::vector < int >& slavesToRemove, AlignmentList *pairwiseAlignments);

    // merge in the contents of the given alignment (assuming same master, compatible block structure),
    // addings its rows to the end of this alignment; returns true if merge successful. Does not change
    // block structure - just adds the part of new alignment's aligned blocks that intersect with this
    // object's aligned blocks
    bool MergeAlignment(const BlockMultipleAlignment *newAlignment);

    // turn on/off geometry violations; if param is true, will return # violations found
    int ShowGeometryViolations(bool showGeometryViolations);

private:
    SequenceList *sequences;
    ConservationColorer *conservationColorer;
    mutable BLAST_Matrix *pssm;

    typedef std::list < Block * > BlockList;
    BlockList blocks;

    int totalWidth;

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
    mutable int cachePrevRow;
    mutable const Block *cachePrevBlock;
    mutable BlockList::const_iterator cacheBlockIterator;
    void InitCache(void);

    // given a row and seqIndex, find block that contains that residue
    const Block * GetBlock(int row, int seqIndex) const;

    // intended for volatile storage of row-associated info (e.g. for alignment scores, etc.)
    mutable std::vector < double > rowDoubles;
    mutable std::vector < std::string > rowStrings;

    // for flagging residues (e.g. for geometry violations)
    typedef std::vector < std::vector < bool > > RowSeqIndexFlags;
    RowSeqIndexFlags geometryViolations;
    bool showGeometryViolations;

public:

    // NULL if block before is aligned; if NULL passed, retrieves last block (if unaligned; else NULL)
    const UnalignedBlock * GetUnalignedBlockBefore(const UngappedAlignedBlock *aBlock) const;
    // NULL if block after is aligned; if NULL passed, retrieves first block (if unaligned; else NULL)
    const UnalignedBlock * GetUnalignedBlockAfter(const UngappedAlignedBlock *aBlock) const;

    int NBlocks(void) const { return blocks.size(); }
    bool HasNoAlignedBlocks(void) const;
    int NAlignedBlocks(void) const;
    int NRows(void) const { return sequences->size(); }
    int AlignmentWidth(void) const { return totalWidth; }

    // return a number from 1..n for aligned blocks, -1 for unaligned
    int GetAlignedBlockNumber(int alignmentIndex) const { return blockMap[alignmentIndex].alignedBlockNum; }

    // for storing/querying info
    double GetRowDouble(int row) const { return rowDoubles[row]; }
    void SetRowDouble(int row, double value) const { rowDoubles[row] = value; }
    const std::string& GetRowStatusLine(int row) const { return rowStrings[row]; }
    void SetRowStatusLine(int row, const std::string& value) const { rowStrings[row] = value; }

    // empties all rows' infos
    void ClearRowInfo(void) const
    {
        for (int r=0; r<NRows(); ++r) {
            rowDoubles[r] = 0.0;
            rowStrings[r].erase();
        }
    }

    // kludge for now for storing allowed alignment regions, e.g. when demoted from multiple.
    // (only two ranges for now, since this is used only with pairwise alignments)
    int alignMasterFrom, alignMasterTo, alignSlaveFrom, alignSlaveTo;
};


// static function to create Seq-aligns out of multiple
ncbi::objects::CSeq_align * CreatePairwiseSeqAlignFromMultipleRow(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment::UngappedAlignedBlockList& blocks, int slaveRow);


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

    // delete a row
    virtual void DeleteRow(int row) = 0;
    virtual void DeleteRows(std::vector < bool >& removeRows, int nToRemove) = 0;

    // makes a new copy of itself
    virtual Block * Clone(const BlockMultipleAlignment *newMultiple) const = 0;

protected:
    Block(const BlockMultipleAlignment *multiple) :
        parentAlignment(multiple), ranges(multiple->NRows()) { }

    typedef std::vector < Range > RangeList;
    RangeList ranges;

    const BlockMultipleAlignment *parentAlignment;

public:
    virtual ~Block(void) { }    // virtual destructor for base class

    bool IsFrom(const BlockMultipleAlignment *alignment) const { return (alignment == parentAlignment); }

    // given a row number (from 0 ... nSequences-1), give the sequence range covered by this block
    const Range* GetRangeOfRow(int row) const { return &(ranges[row]); }

    // set range
    void SetRangeOfRow(int row, int from, int to)
    {
        ranges[row].from = from;
        ranges[row].to = to;
    }

    // resize - add new (empty) rows at end
    void AddRows(int nNewRows) { ranges.resize(ranges.size() + nNewRows); }

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

    void DeleteRow(int row);
    void DeleteRows(std::vector < bool >& removeRows, int nToRemove);

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

    int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const;

    void DeleteRow(int row);
    void DeleteRows(std::vector < bool >& removeRows, int nToRemove);

    Block * Clone(const BlockMultipleAlignment *newMultiple) const;

    // return the length of the shortest region that any row contributes to this block
    int MinResidues(void) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_BLOCK_MULTIPLE_ALIGNMENT__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.49  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.48  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.47  2004/10/04 17:00:54  thiessen
* add expand/restrict highlights, delete all blocks/all rows in updates
*
* Revision 1.46  2004/10/01 13:07:44  thiessen
* add ZipAlignResidue
*
* Revision 1.45  2004/09/27 15:33:04  thiessen
* add block info content optimization on ctrl+shift+click
*
* Revision 1.44  2004/09/27 01:00:44  thiessen
* extend "complete contained" blocks ; merge adjacent blocks after extension"
*
* Revision 1.43  2004/09/23 10:31:14  thiessen
* add block extension algorithm
*
* Revision 1.42  2004/07/27 17:38:12  thiessen
* don't call GetPSSM() w/ no aligned blocks
*
* Revision 1.41  2004/06/23 00:15:47  thiessen
* fix row addition/deletion problem with vector synchronization
*
* Revision 1.40  2004/03/15 17:17:56  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.39  2004/02/19 17:04:44  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.38  2003/11/06 18:52:31  thiessen
* make geometry violations shown on/off; allow multiple pmid entry in ref dialog
*
* Revision 1.37  2003/07/14 18:37:07  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.36  2003/02/03 19:20:01  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.35  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.34  2003/01/28 21:07:56  thiessen
* add block fit coloring algorithm; tweak row dragging; fix style bug
*
* Revision 1.33  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.32  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.31  2002/10/23 01:29:25  thiessen
* fix block cache bug
*
* Revision 1.30  2002/07/26 15:28:44  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.29  2002/06/13 13:32:38  thiessen
* add self-hit calculation
*
* Revision 1.28  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.27  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.26  2002/02/21 22:01:49  thiessen
* remember alignment range on demotion
*
* Revision 1.25  2002/02/19 14:59:38  thiessen
* add CDD reject and purge sequence
*
* Revision 1.24  2002/02/05 18:53:24  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.23  2001/09/27 15:36:47  thiessen
* decouple sequence import and BLAST
*
* Revision 1.22  2001/08/29 20:03:27  juran
* Heed warnings via static_cast.
*
* Revision 1.21  2001/08/15 20:52:57  juran
* Heed warnings.
*
* Revision 1.20  2001/07/19 19:12:46  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.19  2001/07/10 16:39:32  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.18  2001/06/21 02:01:06  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.17  2001/06/02 17:22:58  thiessen
* fixes for GCC
*
* Revision 1.16  2001/06/01 13:35:39  thiessen
* add aligned block number to status line
*
* Revision 1.15  2001/05/17 18:34:00  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.14  2001/05/11 02:10:04  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.13  2001/05/09 17:14:51  thiessen
* add automatic block removal upon demotion
*
* Revision 1.12  2001/05/02 13:46:15  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.11  2001/04/05 22:54:50  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.10  2001/04/04 00:27:21  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.9  2001/03/30 03:07:08  thiessen
* add threader score calculation & sorting
*
* Revision 1.8  2001/03/22 00:32:35  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.7  2001/03/09 15:48:42  thiessen
* major changes to add initial update viewer
*
* Revision 1.6  2001/03/06 20:20:43  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.5  2001/02/13 01:03:03  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.4  2001/02/02 20:17:42  thiessen
* can read in CDD with multi-structure but no struct. alignments
*
* Revision 1.3  2001/01/26 19:30:37  thiessen
* limit undo stack size ; fix memory leak
*
* Revision 1.2  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.1  2000/11/30 15:49:06  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
*/
