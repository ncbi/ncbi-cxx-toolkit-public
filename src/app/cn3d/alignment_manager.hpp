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

#include <list>
#include <vector>

#include <corelib/ncbistl.hpp>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class SequenceSet;
class AlignmentSet;
class MasterSlaveAlignment;
class BlockMultipleAlignment;
class SequenceViewer;
class Messenger;

class AlignmentManager
{
public:
    AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet, Messenger *messenger);
    ~AlignmentManager(void);

    const SequenceSet *sequenceSet;
    const AlignmentSet *alignmentSet;

    // creates the current multiple alignment from the given pairwise alignments (which are
    // assumed to be members of the AlignmentSet).
    typedef std::list < const MasterSlaveAlignment * > AlignmentList;
    BlockMultipleAlignment *
		CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments);

private:
    // for now, will own the current multiple alignment
    BlockMultipleAlignment *currentMultipleAlignment;

    Messenger *messenger;

public:
    const BlockMultipleAlignment * GetCurrentMultipleAlignment(void) const
        { return currentMultipleAlignment; }
};


///// The stuff below makes up the implementation of a multiple alignment /////

class Block; // defined below

class BlockMultipleAlignment
{
public:
    typedef std::vector < const Sequence * > SequenceList;
    BlockMultipleAlignment(const SequenceList *sequenceList);

    ~BlockMultipleAlignment(void);

    const SequenceList *sequences;

    // add a new aligned block - will be "owned" and deallocated by BlockMultipleAlignment
    bool AddAlignedBlockAtEnd(Block *newBlock);

    // should be called after all aligned blocks have been added; fills out
    // unaligned blocks inbetween aligned blocks (and at ends). Also sets length,
    // and fills out the BlockMap for mapping alignment column -> block+column.
    bool AddUnalignedBlocksAndIndex(void);

    // will be used to control padding of unaligned blocks
    enum eUnalignedJustification {
        eLeft,
        eRight,
        eCenter,
        eSplit
    };    
    
private:
    typedef std::list < Block * > BlockList;
    BlockList blocks;
    int totalWidth;

    typedef struct {
        const Block *block;
        int blockColumn;
    } BlockInfo;
    typedef std::vector < BlockInfo > BlockMap;
    BlockMap blockMap;

    Block * CreateNewUnalignedBlockBetween(const Block *left, const Block *right);

    eUnalignedJustification currentJustification;

public:
    int NBlocks(void) const { return blocks.size(); }
    int NSequences(void) const { return sequences->size(); }
    int AlignmentWidth(void) const { return totalWidth; }

    void SetUnalignedJustification(eUnalignedJustification j) { currentJustification = j; }

    // character query interface - "column" must be in alignment range
    // 0 ... totalWidth-1
    bool GetCharacterTraitsAt(int alignmentColumn, int row,
        char *character, Vector *color, bool *isHighlighted) const;

    // get sequence and index (if any) at given position, and whether that residue is aligned
    bool GetSequenceAndIndexAt(int alignmentColumn, int row,
        const Sequence **sequence, int *index, bool *isAligned) const;
};

// base class for Block - BlockMultipleAlignment is made up of a list of these
class Block
{
public:
    const bool isAligned;
    int width;

    typedef struct {
        int from, to;
    } Range;

    // given a row number (from 0 ... nSequences-1), give the sequence range covered by this block
    const Range * GetRangeOfRow(int row) const { return &(ranges[row]); }

    // set range
    void SetRangeOfRow(int row, int from, int to)
    {
        ranges[row].from = from;
        ranges[row].to = to;
    }

    // get sequence index for a column, which must be in block range (0 ... width-1)
    virtual int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const = 0;

    typedef std::vector < const Sequence * > SequenceList;

protected:
    typedef std::vector < Range > RangeList;
    RangeList ranges;

    const SequenceList *sequences;

public:
    Block(const SequenceList *sequenceList, bool isAlignedBlock) :
        isAligned(isAlignedBlock), sequences(sequenceList), ranges(sequenceList->size()) { }

    int NSequences(void) const { return ranges.size(); }
};

// a gapless aligned block; width must be >= 1
class UngappedAlignedBlock : public Block
{
public:
    UngappedAlignedBlock(const SequenceList *sequenceList) : Block(sequenceList, true) { }
    int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const
            { return (GetRangeOfRow(row)->from + blockColumn); }
};

// an unaligned block; max width of block must be >=1, but range over any given
// sequence can be length 0 (but not <0). If length 0, "to" is the residue after
// the block, and "from" (=to - 1) is the residue before.
class UnalignedBlock : public Block
{
public:
    UnalignedBlock(const SequenceList *sequenceList) : Block(sequenceList, false) { }
    int GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_MANAGER__HPP
