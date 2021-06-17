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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/tables/raw_scoremat.h>

#include <objects/seqalign/Dense_diag.hpp>

#include <algo/structure/struct_util/su_block_multiple_alignment.hpp>
#include <algo/structure/struct_util/su_sequence_set.hpp>
#include <algo/structure/struct_util/su_pssm.hpp>
#include "su_private.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(struct_util)

BlockMultipleAlignment::BlockMultipleAlignment(const SequenceList& sequenceList)
{
    m_sequences = sequenceList;
    m_pssm = NULL;

    InitCache();
    m_rowDoubles.resize(m_sequences.size(), 0.0);
    m_rowStrings.resize(m_sequences.size());
}

void BlockMultipleAlignment::InitCache(void)
{
    m_cachePrevRow = eUndefined;
    m_cachePrevBlock = NULL;
    m_cacheBlockIterator = m_blocks.begin();
}

BlockMultipleAlignment::~BlockMultipleAlignment(void)
{
    RemovePSSM();
}

BlockMultipleAlignment * BlockMultipleAlignment::Clone(void) const
{
    BlockMultipleAlignment *copy = new BlockMultipleAlignment(m_sequences);
    BlockList::const_iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b)
        copy->m_blocks.push_back(CRef<Block>((*b)->Clone(copy)));
    copy->UpdateBlockMap();
    copy->m_rowDoubles = m_rowDoubles;
    copy->m_rowStrings = m_rowStrings;
    return copy;
}

/*
static inline unsigned int ScreenResidueCharacter(char original)
{
    char ch = toupper((unsigned char) original);
    switch (ch) {
        case 'A': case 'R': case 'N': case 'D': case 'C':
        case 'Q': case 'E': case 'G': case 'H': case 'I':
        case 'L': case 'K': case 'M': case 'F': case 'P':
        case 'S': case 'T': case 'W': case 'Y': case 'V':
        case 'B': case 'Z':
            break;
        default:
            ch = 'X'; // make all but natural aa's just 'X'
    }
    return ch;
}

static int GetBLOSUM62Score(char a, char b)
{
    static SNCBIFullScoreMatrix Blosum62Matrix;
    static bool unpacked = false;

    if (!unpacked) {
        NCBISM_Unpack(&NCBISM_Blosum62, &Blosum62Matrix);
        unpacked = true;
    }

    return Blosum62Matrix.s[ScreenResidueCharacter(a)][ScreenResidueCharacter(b)];
}
*/

const BLAST_Matrix * BlockMultipleAlignment::GetPSSM(void) const
{
    if (m_pssm) return m_pssm;
    m_pssm = CreateBlastMatrix(this);
    return m_pssm;
}

void BlockMultipleAlignment::RemovePSSM(void) const
{
    if (m_pssm) {
        delete m_pssm;
        m_pssm = NULL;
    }
}

void BlockMultipleAlignment::GetBlockList(ConstBlockList& cbl) const
{
    cbl.clear();
    cbl.reserve(m_blocks.size());
    BlockList::const_iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b)
        cbl.push_back(CConstRef<Block>(b->GetPointer()));
}

bool BlockMultipleAlignment::CheckAlignedBlock(const Block *block) const
{
    if (!block || !block->IsAligned()) {
        ERROR_MESSAGE("CheckAlignedBlock() - checks aligned blocks only");
        return false;
    }
    if (block->NSequences() != m_sequences.size()) {
        ERROR_MESSAGE("CheckAlignedBlock() - block size mismatch");
        return false;
    }

    // make sure ranges are reasonable for each sequence
    unsigned int row;
    const Block
        *prevBlock = GetBlockBefore(block),
        *nextBlock = GetBlockAfter(block);
    const Block::Range *range, *prevRange = NULL, *nextRange = NULL;
    SequenceList::const_iterator sequence = m_sequences.begin();
    for (row=0; row<block->NSequences(); ++row, ++sequence) {
        range = block->GetRangeOfRow(row);
        if (prevBlock) prevRange = prevBlock->GetRangeOfRow(row);
        if (nextBlock) nextRange = nextBlock->GetRangeOfRow(row);
        if (range->to - range->from + 1 != (int)block->m_width ||   // check block m_width
            (prevRange && range->from <= prevRange->to) ||          // check for range overlap
            (nextRange && range->to >= nextRange->from) ||          // check for range overlap
            range->from > range->to ||                              // check range values
            range->to >= (int)(*sequence)->Length())                // check bounds of end
        {
            ERROR_MESSAGE("CheckAlignedBlock() - range error");
            return false;
        }
    }

    return true;
}

bool BlockMultipleAlignment::AddAlignedBlockAtEnd(UngappedAlignedBlock *newBlock)
{
    m_blocks.push_back(CRef<Block>(newBlock));
    return CheckAlignedBlock(newBlock);
}

UnalignedBlock * BlockMultipleAlignment::
    CreateNewUnalignedBlockBetween(const Block *leftBlock, const Block *rightBlock)
{
    if ((leftBlock && !leftBlock->IsAligned()) ||
        (rightBlock && !rightBlock->IsAligned())) {
        ERROR_MESSAGE("CreateNewUnalignedBlockBetween() - passed an unaligned block");
        return NULL;
    }

    unsigned int row, from, to, length;
    SequenceList::const_iterator s, se = m_sequences.end();

    UnalignedBlock *newBlock = new UnalignedBlock(this);
    newBlock->m_width = 0;

    for (row=0, s=m_sequences.begin(); s!=se; ++row, ++s) {

        if (leftBlock)
            from = leftBlock->GetRangeOfRow(row)->to + 1;
        else
            from = 0;

        if (rightBlock)
            to = rightBlock->GetRangeOfRow(row)->from - 1;
        else
            to = (*s)->Length() - 1;

        newBlock->SetRangeOfRow(row, from, to);

        length = to - from + 1;
        if ((((int)to) - ((int)from) + 1) < 0) { // just to make sure...
            ERROR_MESSAGE("CreateNewUnalignedBlockBetween() - unaligned length < 0");
            return NULL;
        }
        if (length > newBlock->m_width)
            newBlock->m_width = length;
    }

    if (newBlock->m_width == 0) {
        delete newBlock;
        return NULL;
    } else
        return newBlock;
}

bool BlockMultipleAlignment::AddUnalignedBlocks(void)
{
    BlockList::iterator a, ae = m_blocks.end();
    const Block *alignedBlock = NULL, *prevAlignedBlock = NULL;
    Block *newUnalignedBlock;

    // unaligned m_blocks to the left of each aligned block
    for (a=m_blocks.begin(); a!=ae; ++a) {
        alignedBlock = *a;
        newUnalignedBlock = CreateNewUnalignedBlockBetween(prevAlignedBlock, alignedBlock);
        if (newUnalignedBlock)
            m_blocks.insert(a, CRef<Block>(newUnalignedBlock));
        prevAlignedBlock = alignedBlock;
    }

    // right tail
    newUnalignedBlock = CreateNewUnalignedBlockBetween(alignedBlock, NULL);
    if (newUnalignedBlock) {
        m_blocks.insert(a, CRef<Block>(newUnalignedBlock));
    }

    return true;
}

bool BlockMultipleAlignment::UpdateBlockMap(bool clearRowInfo)
{
    unsigned int i = 0, j, n = 0;
    BlockList::iterator b, be = m_blocks.end();

    // reset old stuff, recalculate m_width
    m_totalWidth = 0;
    for (b=m_blocks.begin(); b!=be; ++b)
        m_totalWidth += (*b)->m_width;

    // fill out the block map
    m_blockMap.resize(m_totalWidth);
    UngappedAlignedBlock *aBlock;
    for (b=m_blocks.begin(); b!=be; ++b) {
        aBlock = dynamic_cast<UngappedAlignedBlock*>(b->GetPointer());
        if (aBlock)
            ++n;
        for (j=0; j<(*b)->m_width; ++j, ++i) {
            m_blockMap[i].block = *b;
            m_blockMap[i].blockColumn = j;
            m_blockMap[i].alignedBlockNum = aBlock ? n : eUndefined;
        }
    }

    // if alignment changes, any pssm/scores/status become invalid
    RemovePSSM();
    if (clearRowInfo) ClearRowInfo();

    return true;
}

bool BlockMultipleAlignment::GetCharacterAt(
    unsigned int alignmentColumn, unsigned int row, eUnalignedJustification justification,
    char *character) const
{
    const Sequence *sequence;
    unsigned int seqIndex;
    bool isAligned;

    if (!GetSequenceAndIndexAt(alignmentColumn, row, justification, &sequence, &seqIndex, &isAligned))
        return false;

    *character = (seqIndex != eUndefined) ? sequence->m_sequenceString[seqIndex] : '~';
    if (isAligned)
        *character = toupper((unsigned char)(*character));
    else
        *character = tolower((unsigned char)(*character));

    return true;
}

bool BlockMultipleAlignment::GetSequenceAndIndexAt(
    unsigned int alignmentColumn, unsigned int row, eUnalignedJustification requestedJustification,
    const Sequence **sequence, unsigned int *index, bool *isAligned) const
{
    if (sequence)
        *sequence = m_sequences[row];

    const BlockInfo& blockInfo = m_blockMap[alignmentColumn];

    if (!blockInfo.block->IsAligned()) {
        if (isAligned)
            *isAligned = false;
        // override requested justification for end m_blocks
        if (blockInfo.block == m_blocks.back().GetPointer()) // also true if there's a single aligned block
            requestedJustification = eLeft;
        else if (blockInfo.block == m_blocks.front().GetPointer())
            requestedJustification = eRight;
    } else
        if (isAligned)
            *isAligned = true;

    if (index)
        *index = blockInfo.block->GetIndexAt(blockInfo.blockColumn, row, requestedJustification);

    return true;
}

bool BlockMultipleAlignment::IsAligned(unsigned int row, unsigned int seqIndex) const
{
    const Block *block = GetBlock(row, seqIndex);
    return (block && block->IsAligned());
}

const Block * BlockMultipleAlignment::GetBlock(unsigned int row, unsigned int seqIndex) const
{
    // make sure we're in range for this sequence
    if (row >= NRows() || seqIndex >= m_sequences[row]->Length()) {
        ERROR_MESSAGE("BlockMultipleAlignment::GetBlock() - coordinate out of range");
        return NULL;
    }

    const Block::Range *range;

    // first check to see if it's in the same block as last time.
    if (m_cachePrevBlock) {
        range = m_cachePrevBlock->GetRangeOfRow(row);
        if ((int)seqIndex >= range->from && (int)seqIndex <= range->to)
            return m_cachePrevBlock;
        ++m_cacheBlockIterator; // start search at next block
    } else {
        m_cacheBlockIterator = m_blocks.begin();
    }

    // otherwise, perform block search. This search is most efficient when queries
    // happen in order from left to right along a given row.
    do {
        if (m_cacheBlockIterator == m_blocks.end())
            m_cacheBlockIterator = m_blocks.begin();
        range = (*m_cacheBlockIterator)->GetRangeOfRow(row);
        if ((int)seqIndex >= range->from && (int)seqIndex <= range->to) {
            m_cachePrevBlock = *m_cacheBlockIterator; // cache this block
            return m_cachePrevBlock;
        }
        ++m_cacheBlockIterator;
    } while (1);
}

unsigned int BlockMultipleAlignment::GetFirstAlignedBlockPosition(void) const
{
    BlockList::const_iterator b = m_blocks.begin();
    if (m_blocks.size() > 0 && (*b)->IsAligned()) // first block is aligned
        return 0;
    else if (m_blocks.size() >= 2 && (*(++b))->IsAligned()) // second block is aligned
        return m_blocks.front()->m_width;
    else
        return eUndefined;
}

unsigned int BlockMultipleAlignment::GetAlignedSlaveIndex(unsigned int masterSeqIndex, unsigned int slaveRow) const
{
    const UngappedAlignedBlock
        *aBlock = dynamic_cast<const UngappedAlignedBlock*>(GetBlock(0, masterSeqIndex));
    if (!aBlock)
        return eUndefined;

    const Block::Range
        *masterRange = aBlock->GetRangeOfRow(0),
        *slaveRange = aBlock->GetRangeOfRow(slaveRow);
    return (slaveRange->from + masterSeqIndex - masterRange->from);
}

void BlockMultipleAlignment::GetAlignedBlockPosition(unsigned int alignmentIndex,
    unsigned int *blockColumn, unsigned int *blockWidth) const
{
    *blockColumn = *blockWidth = eUndefined;
    const BlockInfo& info = m_blockMap[alignmentIndex];
    if (info.block->IsAligned()) {
        *blockColumn = info.blockColumn;
        *blockWidth = info.block->m_width;
    }
}

Block * BlockMultipleAlignment::GetBlockBefore(const Block *block)
{
    Block *prevBlock = NULL;
    BlockList::iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        if (*b == block) break;
        prevBlock = b->GetPointer();
    }
    return prevBlock;
}

const Block * BlockMultipleAlignment::GetBlockBefore(const Block *block) const
{
    const Block *prevBlock = NULL;
    BlockList::const_iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        if (*b == block) break;
        prevBlock = b->GetPointer();
    }
    return prevBlock;
}

Block * BlockMultipleAlignment::GetBlockAfter(const Block *block)
{
    BlockList::iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        if (*b == block) {
            ++b;
            if (b == be) break;
            return *b;
        }
    }
    return NULL;
}

const Block * BlockMultipleAlignment::GetBlockAfter(const Block *block) const
{
    BlockList::const_iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        if (*b == block) {
            ++b;
            if (b == be) break;
            return *b;
        }
    }
    return NULL;
}

const UnalignedBlock * BlockMultipleAlignment::GetUnalignedBlockBefore(
    const UngappedAlignedBlock *aBlock) const
{
    const Block *prevBlock;
    if (aBlock)
        prevBlock = GetBlockBefore(aBlock);
    else
        prevBlock = m_blocks.back().GetPointer();
    return dynamic_cast<const UnalignedBlock*>(prevBlock);
}

void BlockMultipleAlignment::InsertBlockBefore(Block *newBlock, const Block *insertAt)
{
    BlockList::iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        if (*b == insertAt) {
            m_blocks.insert(b, CRef<Block>(newBlock));
            return;
        }
    }
    WARNING_MESSAGE("BlockMultipleAlignment::InsertBlockBefore() - couldn't find insertAt block");
}

void BlockMultipleAlignment::InsertBlockAfter(const Block *insertAt, Block *newBlock)
{
    BlockList::iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        if (*b == insertAt) {
            ++b;
            m_blocks.insert(b, CRef<Block>(newBlock));
            return;
        }
    }
    WARNING_MESSAGE("BlockMultipleAlignment::InsertBlockBefore() - couldn't find insertAt block");
}

void BlockMultipleAlignment::RemoveBlock(Block *block)
{
    BlockList::iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        if (*b == block) {
            m_blocks.erase(b);
            InitCache();
            return;
        }
    }
    WARNING_MESSAGE("BlockMultipleAlignment::RemoveBlock() - couldn't find block");
}

bool BlockMultipleAlignment::MoveBlockBoundary(unsigned int columnFrom, unsigned int columnTo)
{
    unsigned int blockColumn, blockWidth;
    GetAlignedBlockPosition(columnFrom, &blockColumn, &blockWidth);
    if (blockColumn == eUndefined || blockWidth == 0 || blockWidth == eUndefined) return false;

    TRACE_MESSAGE("trying to move block boundary from " << columnFrom << " to " << columnTo);

    const BlockInfo& info = m_blockMap[columnFrom];
    unsigned int row;
    int requestedShift = columnTo - columnFrom, actualShift = 0;
    const Block::Range *range;

    // shrink block from left
    if (blockColumn == 0 && requestedShift > 0 && requestedShift < (int) info.block->m_width) {
        actualShift = requestedShift;
        TRACE_MESSAGE("shrinking block from left");
        for (row=0; row<NRows(); ++row) {
            range = info.block->GetRangeOfRow(row);
            info.block->SetRangeOfRow(row, range->from + requestedShift, range->to);
        }
        info.block->m_width -= requestedShift;
        Block *prevBlock = GetBlockBefore(info.block);
        if (prevBlock && !prevBlock->IsAligned()) {
            for (row=0; row<NRows(); ++row) {
                range = prevBlock->GetRangeOfRow(row);
                prevBlock->SetRangeOfRow(row, range->from, range->to + requestedShift);
            }
            prevBlock->m_width += requestedShift;
        } else {
            Block *newUnalignedBlock = CreateNewUnalignedBlockBetween(prevBlock, info.block);
            if (newUnalignedBlock)
                InsertBlockBefore(newUnalignedBlock, info.block);
            TRACE_MESSAGE("added new unaligned block");
        }
    }

    // shrink block from right (requestedShift < 0)
    else if (blockColumn == info.block->m_width - 1 &&
                requestedShift < 0 && ((unsigned int) -requestedShift) < info.block->m_width) {
        actualShift = requestedShift;
        TRACE_MESSAGE("shrinking block from right");
        for (row=0; row<NRows(); ++row) {
            range = info.block->GetRangeOfRow(row);
            info.block->SetRangeOfRow(row, range->from, range->to + requestedShift);
        }
        info.block->m_width += requestedShift;
        Block *nextBlock = GetBlockAfter(info.block);
        if (nextBlock && !nextBlock->IsAligned()) {
            for (row=0; row<NRows(); ++row) {
                range = nextBlock->GetRangeOfRow(row);
                nextBlock->SetRangeOfRow(row, range->from + requestedShift, range->to);
            }
            nextBlock->m_width -= requestedShift;
        } else {
            Block *newUnalignedBlock = CreateNewUnalignedBlockBetween(info.block, nextBlock);
            if (newUnalignedBlock)
                InsertBlockAfter(info.block, newUnalignedBlock);
            TRACE_MESSAGE("added new unaligned block");
        }
    }

    // grow block to right
    else if (blockColumn == info.block->m_width - 1 && requestedShift > 0) {
        Block *nextBlock = GetBlockAfter(info.block);
        if (nextBlock && !nextBlock->IsAligned()) {
            int nRes;
            actualShift = requestedShift;
            for (row=0; row<NRows(); ++row) {
                range = nextBlock->GetRangeOfRow(row);
                nRes = range->to - range->from + 1;
                if (nRes < actualShift)
                    actualShift = nRes;
            }
            if (actualShift) {
                TRACE_MESSAGE("growing block to right");
                for (row=0; row<NRows(); ++row) {
                    range = info.block->GetRangeOfRow(row);
                    info.block->SetRangeOfRow(row, range->from, range->to + actualShift);
                    range = nextBlock->GetRangeOfRow(row);
                    nextBlock->SetRangeOfRow(row, range->from + actualShift, range->to);
                }
                info.block->m_width += actualShift;
                nextBlock->m_width -= actualShift;
                if (nextBlock->m_width == 0) {
                    RemoveBlock(nextBlock);
                    TRACE_MESSAGE("removed empty block");
                }
            }
        }
    }

    // grow block to left (requestedShift < 0)
    else if (blockColumn == 0 && requestedShift < 0) {
        Block *prevBlock = GetBlockBefore(info.block);
        if (prevBlock && !prevBlock->IsAligned()) {
            int nRes;
            actualShift = requestedShift;
            for (row=0; row<NRows(); ++row) {
                range = prevBlock->GetRangeOfRow(row);
                nRes = range->to - range->from + 1;
                if (nRes < -actualShift) actualShift = -nRes;
            }
            if (actualShift) {
                TRACE_MESSAGE("growing block to left");
                for (row=0; row<NRows(); ++row) {
                    range = info.block->GetRangeOfRow(row);
                    info.block->SetRangeOfRow(row, range->from + actualShift, range->to);
                    range = prevBlock->GetRangeOfRow(row);
                    prevBlock->SetRangeOfRow(row, range->from, range->to + actualShift);
                }
                info.block->m_width -= actualShift;
                prevBlock->m_width += actualShift;
                if (prevBlock->m_width == 0) {
                    RemoveBlock(prevBlock);
                    TRACE_MESSAGE("removed empty block");
                }
            }
        }
    }

    if (actualShift != 0) {
        UpdateBlockMap();
        return true;
    } else
        return false;
}

bool BlockMultipleAlignment::ShiftRow(unsigned int row, unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex,
    eUnalignedJustification justification)
{
    if (fromAlignmentIndex == toAlignmentIndex)
        return false;

    Block
        *blockFrom = m_blockMap[fromAlignmentIndex].block,
        *blockTo = m_blockMap[toAlignmentIndex].block;

    // at least one end of the drag must be in an aligned block
    UngappedAlignedBlock *ABlock =
        dynamic_cast<UngappedAlignedBlock*>(blockFrom);
    if (ABlock) {
        if (blockTo != blockFrom && blockTo->IsAligned())
            return false;
    } else {
        ABlock = dynamic_cast<UngappedAlignedBlock*>(blockTo);
        if (!ABlock)
            return false;
    }

    // and the other must be in the same aligned block or an adjacent unaligned block
    UnalignedBlock
        *prevUABlock = dynamic_cast<UnalignedBlock*>(GetBlockBefore(ABlock)),
        *nextUABlock = dynamic_cast<UnalignedBlock*>(GetBlockAfter(ABlock));
    if (blockFrom != blockTo &&
        ((ABlock == blockFrom && prevUABlock != blockTo && nextUABlock != blockTo) ||
         (ABlock == blockTo && prevUABlock != blockFrom && nextUABlock != blockFrom)))
        return false;

    int requestedShift, actualShift = 0, width = 0;

    // slightly different behaviour when dragging from unaligned to aligned...
    if (!blockFrom->IsAligned()) {
        unsigned int fromSeqIndex, toSeqIndex;
        GetSequenceAndIndexAt(fromAlignmentIndex, row, justification, NULL, &fromSeqIndex, NULL);
        GetSequenceAndIndexAt(toAlignmentIndex, row, justification, NULL, &toSeqIndex, NULL);
        if (fromSeqIndex == eUndefined || toSeqIndex == eUndefined)
            return false;
        requestedShift = toSeqIndex - fromSeqIndex;
    }

    // vs. dragging from aligned
    else {
        requestedShift = toAlignmentIndex - fromAlignmentIndex;
    }

    const Block::Range *prevRange = NULL, *nextRange = NULL, *range = ABlock->GetRangeOfRow(row);
    if (prevUABlock) prevRange = prevUABlock->GetRangeOfRow(row);
    if (nextUABlock) nextRange = nextUABlock->GetRangeOfRow(row);
    if (requestedShift > 0) {
        if (prevUABlock)
            width = prevRange->to - prevRange->from + 1;
        actualShift = (width > requestedShift) ? requestedShift : width;
    } else {
        if (nextUABlock)
            width = nextRange->to - nextRange->from + 1;
        actualShift = (width > -requestedShift) ? requestedShift : -width;
    }
    if (actualShift == 0) return false;

    TRACE_MESSAGE("shifting row " << row << " by " << actualShift);

    ABlock->SetRangeOfRow(row, range->from - actualShift, range->to - actualShift);

    if (prevUABlock) {
        prevUABlock->SetRangeOfRow(row, prevRange->from, prevRange->to - actualShift);
        prevUABlock->m_width = 0;
        for (unsigned int i=0; i<NRows(); ++i) {
            prevRange = prevUABlock->GetRangeOfRow(i);
            width = prevRange->to - prevRange->from + 1;
            if (width < 0)
                ERROR_MESSAGE("BlockMultipleAlignment::ShiftRow() - negative width on left");
            if ((unsigned int) width > prevUABlock->m_width)
                prevUABlock->m_width = width;
        }
        if (prevUABlock->m_width == 0) {
            TRACE_MESSAGE("removing zero-m_width unaligned block on left");
            RemoveBlock(prevUABlock);
        }
    } else {
        TRACE_MESSAGE("creating unaligned block on left");
        prevUABlock = CreateNewUnalignedBlockBetween(GetBlockBefore(ABlock), ABlock);
        InsertBlockBefore(prevUABlock, ABlock);
    }

    if (nextUABlock) {
        nextUABlock->SetRangeOfRow(row, nextRange->from - actualShift, nextRange->to);
        nextUABlock->m_width = 0;
        for (unsigned int i=0; i<NRows(); ++i) {
            nextRange = nextUABlock->GetRangeOfRow(i);
            width = nextRange->to - nextRange->from + 1;
            if (width < 0)
                ERROR_MESSAGE("BlockMultipleAlignment::ShiftRow() - negative width on right");
            if ((unsigned int) width > nextUABlock->m_width)
                nextUABlock->m_width = width;
        }
        if (nextUABlock->m_width == 0) {
            TRACE_MESSAGE("removing zero-m_width unaligned block on right");
            RemoveBlock(nextUABlock);
        }
    } else {
        TRACE_MESSAGE("creating unaligned block on right");
        nextUABlock = CreateNewUnalignedBlockBetween(ABlock, GetBlockAfter(ABlock));
        InsertBlockAfter(ABlock, nextUABlock);
    }

    if (!CheckAlignedBlock(ABlock))
        ERROR_MESSAGE("BlockMultipleAlignment::ShiftRow() - shift failed to create valid aligned block");

    UpdateBlockMap();
    return true;
}

bool BlockMultipleAlignment::SplitBlock(unsigned int alignmentIndex)
{
    const BlockInfo& info = m_blockMap[alignmentIndex];
    if (!info.block->IsAligned() || info.block->m_width < 2 || info.blockColumn == 0)
        return false;

    TRACE_MESSAGE("splitting block");
    UngappedAlignedBlock *newAlignedBlock = new UngappedAlignedBlock(this);
    newAlignedBlock->m_width = info.block->m_width - info.blockColumn;
    info.block->m_width = info.blockColumn;

    const Block::Range *range;
    unsigned int oldTo;
    for (unsigned int row=0; row<NRows(); ++row) {
        range = info.block->GetRangeOfRow(row);
        oldTo = range->to;
        info.block->SetRangeOfRow(row, range->from, range->from + info.block->m_width - 1);
        newAlignedBlock->SetRangeOfRow(row, oldTo - newAlignedBlock->m_width + 1, oldTo);
    }

    InsertBlockAfter(info.block, newAlignedBlock);
    if (!CheckAlignedBlock(info.block) || !CheckAlignedBlock(newAlignedBlock))
        ERROR_MESSAGE("BlockMultipleAlignment::SplitBlock() - split failed to create valid m_blocks");

    UpdateBlockMap();
    return true;
}

bool BlockMultipleAlignment::MergeBlocks(unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex)
{
    Block
        *expandedBlock = m_blockMap[fromAlignmentIndex].block,
        *lastBlock = m_blockMap[toAlignmentIndex].block;
    if (expandedBlock == lastBlock)
        return false;
    unsigned int i;
    for (i=fromAlignmentIndex; i<=toAlignmentIndex; ++i)
        if (!m_blockMap[i].block->IsAligned())
            return false;

    TRACE_MESSAGE("merging block(s)");
    for (i=0; i<NRows(); ++i)
        expandedBlock->SetRangeOfRow(i, expandedBlock->GetRangeOfRow(i)->from, lastBlock->GetRangeOfRow(i)->to);
    expandedBlock->m_width = lastBlock->GetRangeOfRow(0)->to - expandedBlock->GetRangeOfRow(0)->from + 1;

    Block *deletedBlock = NULL, *blockToDelete;
    for (i=fromAlignmentIndex; i<=toAlignmentIndex; ++i) {
        blockToDelete = m_blockMap[i].block;
        if (blockToDelete == expandedBlock)
            continue;
        if (blockToDelete != deletedBlock) {
            deletedBlock = blockToDelete;
            RemoveBlock(blockToDelete);
        }
    }

    if (!CheckAlignedBlock(expandedBlock))
        ERROR_MESSAGE("BlockMultipleAlignment::MergeBlocks() - merge failed to create valid block");

    UpdateBlockMap();
    return true;
}

bool BlockMultipleAlignment::CreateBlock(unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex,
    eUnalignedJustification justification)
{
    const BlockInfo& info = m_blockMap[fromAlignmentIndex];
    UnalignedBlock *prevUABlock = dynamic_cast<UnalignedBlock*>(info.block);
    if (!prevUABlock || info.block != m_blockMap[toAlignmentIndex].block)
        return false;
    unsigned int row, seqIndexFrom, seqIndexTo,
        newBlockWidth = toAlignmentIndex - fromAlignmentIndex + 1,
        origWidth = prevUABlock->m_width;
    vector < unsigned int > seqIndexesFrom(NRows()), seqIndexesTo(NRows());
    const Sequence *seq;
	bool ignored;
    for (row=0; row<NRows(); ++row) {
        if (!GetSequenceAndIndexAt(fromAlignmentIndex, row, justification, &seq, &seqIndexFrom, &ignored) ||
                !GetSequenceAndIndexAt(toAlignmentIndex, row, justification, &seq, &seqIndexTo, &ignored) ||
                seqIndexFrom == eUndefined || seqIndexTo == eUndefined ||
                seqIndexTo - seqIndexFrom + 1 != newBlockWidth)
            return false;
        seqIndexesFrom[row] = seqIndexFrom;
        seqIndexesTo[row] = seqIndexTo;
    }

    TRACE_MESSAGE("creating new aligned and unaligned blocks");

    UnalignedBlock *nextUABlock = new UnalignedBlock(this);
    UngappedAlignedBlock *ABlock = new UngappedAlignedBlock(this);
    prevUABlock->m_width = nextUABlock->m_width = 0;

    bool deletePrevUABlock = true, deleteNextUABlock = true;
    const Block::Range *prevRange;
    int rangeWidth;
    for (row=0; row<NRows(); ++row) {
        prevRange = prevUABlock->GetRangeOfRow(row);

        nextUABlock->SetRangeOfRow(row, seqIndexesTo[row] + 1, prevRange->to);
        rangeWidth = prevRange->to - seqIndexesTo[row];
        if (rangeWidth < 0)
            ERROR_MESSAGE("BlockMultipleAlignment::CreateBlock() - negative nextRange width");
        else if (rangeWidth > 0) {
            if ((unsigned int) rangeWidth > nextUABlock->m_width)
                nextUABlock->m_width = rangeWidth;
            deleteNextUABlock = false;
        }

        prevUABlock->SetRangeOfRow(row, prevRange->from, seqIndexesFrom[row] - 1);
        rangeWidth = seqIndexesFrom[row] - prevRange->from;
        if (rangeWidth < 0)
            ERROR_MESSAGE("BlockMultipleAlignment::CreateBlock() - negative prevRange width");
        else if (rangeWidth > 0) {
            if ((unsigned int) rangeWidth > prevUABlock->m_width)
                prevUABlock->m_width = rangeWidth;
            deletePrevUABlock = false;
        }

        ABlock->SetRangeOfRow(row, seqIndexesFrom[row], seqIndexesTo[row]);
    }

    ABlock->m_width = newBlockWidth;
    if (prevUABlock->m_width + ABlock->m_width + nextUABlock->m_width != origWidth)
        ERROR_MESSAGE("BlockMultipleAlignment::CreateBlock() - bad block m_widths sum");

    InsertBlockAfter(prevUABlock, ABlock);
    InsertBlockAfter(ABlock, nextUABlock);
    if (deletePrevUABlock) {
        TRACE_MESSAGE("deleting zero-width unaligned block on left");
        RemoveBlock(prevUABlock);
    }
    if (deleteNextUABlock) {
        TRACE_MESSAGE("deleting zero-width unaligned block on right");
        RemoveBlock(nextUABlock);
    }

    if (!CheckAlignedBlock(ABlock))
        ERROR_MESSAGE("BlockMultipleAlignment::CreateBlock() - failed to create valid block");

    UpdateBlockMap();
    return true;
}

bool BlockMultipleAlignment::DeleteBlock(unsigned int alignmentIndex)
{
    Block *block = m_blockMap[alignmentIndex].block;
    if (!block || !block->IsAligned())
        return false;

    TRACE_MESSAGE("deleting block");
    Block
        *prevBlock = GetBlockBefore(block),
        *nextBlock = GetBlockAfter(block);

    // unaligned m_blocks on both sides - note that total alignment m_width can change!
    if (prevBlock && !prevBlock->IsAligned() && nextBlock && !nextBlock->IsAligned()) {
        const Block::Range *prevRange, *nextRange;
        unsigned int maxWidth = 0, width;
        for (unsigned int row=0; row<NRows(); ++row) {
            prevRange = prevBlock->GetRangeOfRow(row);
            nextRange = nextBlock->GetRangeOfRow(row);
            width = nextRange->to - prevRange->from + 1;
            prevBlock->SetRangeOfRow(row, prevRange->from, nextRange->to);
            if (width > maxWidth)
                maxWidth = width;
        }
        prevBlock->m_width = maxWidth;
        TRACE_MESSAGE("removing extra unaligned block");
        RemoveBlock(nextBlock);
    }

    // unaligned block on left only
    else if (prevBlock && !prevBlock->IsAligned()) {
        const Block::Range *prevRange, *range;
        for (unsigned int row=0; row<NRows(); ++row) {
            prevRange = prevBlock->GetRangeOfRow(row);
            range = block->GetRangeOfRow(row);
            prevBlock->SetRangeOfRow(row, prevRange->from, range->to);
        }
        prevBlock->m_width += block->m_width;
    }

    // unaligned block on right only
    else if (nextBlock && !nextBlock->IsAligned()) {
        const Block::Range *range, *nextRange;
        for (unsigned int row=0; row<NRows(); ++row) {
            range = block->GetRangeOfRow(row);
            nextRange = nextBlock->GetRangeOfRow(row);
            nextBlock->SetRangeOfRow(row, range->from, nextRange->to);
        }
        nextBlock->m_width += block->m_width;
    }

    // no adjacent unaligned m_blocks
    else {
        TRACE_MESSAGE("creating new unaligned block");
        Block *newBlock = CreateNewUnalignedBlockBetween(prevBlock, nextBlock);
        if (prevBlock)
            InsertBlockAfter(prevBlock, newBlock);
        else if (nextBlock)
            InsertBlockBefore(newBlock, nextBlock);
        else
            m_blocks.push_back(CRef<Block>(newBlock));
    }

    RemoveBlock(block);
    UpdateBlockMap();
    return true;
}

bool BlockMultipleAlignment::DeleteAllBlocks(void)
{
    if (m_blocks.size() == 0)
        return false;

    m_blocks.clear();
    InitCache();
    AddUnalignedBlocks();   // one single unaligned block for whole alignment
    UpdateBlockMap();
    return true;
}

bool BlockMultipleAlignment::DeleteRow(unsigned int row)
{
    if (row >= NRows()) {
        ERROR_MESSAGE("BlockMultipleAlignment::DeleteRow() - row out of range");
        return false;
    }

    // remove sequence from list
    vector < bool > removeRows(NRows(), false);
    removeRows[row] = true;
    VectorRemoveElements(m_sequences, removeRows, 1);
    VectorRemoveElements(m_rowDoubles, removeRows, 1);
    VectorRemoveElements(m_rowStrings, removeRows, 1);

    // delete row from all m_blocks, removing any zero-m_width m_blocks
    BlockList::iterator b = m_blocks.begin(), br, be = m_blocks.end();
    while (b != be) {
        (*b)->DeleteRow(row);
        if ((*b)->m_width == 0) {
            br = b;
            ++b;
            TRACE_MESSAGE("deleting block resized to zero width");
            RemoveBlock(*br);
        } else
            ++b;
    }

    // update total alignment m_width
    UpdateBlockMap();
    InitCache();

    return true;
}

void BlockMultipleAlignment::GetUngappedAlignedBlocks(UngappedAlignedBlockList *uabs) const
{
    uabs->clear();
    uabs->reserve(m_blocks.size());
    BlockList::const_iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        const UngappedAlignedBlock *uab = dynamic_cast<const UngappedAlignedBlock*>(b->GetPointer());
        if (uab)
            uabs->push_back(uab);
    }
    uabs->resize(uabs->size());
}

void BlockMultipleAlignment::GetModifiableUngappedAlignedBlocks(ModifiableUngappedAlignedBlockList *uabs)
{
    uabs->clear();
    uabs->reserve(m_blocks.size());
    BlockList::iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b) {
        UngappedAlignedBlock *uab = dynamic_cast<UngappedAlignedBlock*>(b->GetPointer());
        if (uab)
            uabs->push_back(uab);
    }
    uabs->resize(uabs->size());
}

bool BlockMultipleAlignment::ExtractRows(
    const vector < unsigned int >& slavesToRemove, AlignmentList *pairwiseAlignments)
{
    if (slavesToRemove.size() == 0) return false;

    // make a bool list of rows to remove, also checking to make sure slave list items are in range
    unsigned int i;
    vector < bool > removeRows(NRows(), false);
    for (i=0; i<slavesToRemove.size(); ++i) {
        if (slavesToRemove[i] > 0 && slavesToRemove[i] < NRows()) {
            removeRows[slavesToRemove[i]] = true;
        } else {
            ERROR_MESSAGE("BlockMultipleAlignment::ExtractRows() - can't extract row "
                << slavesToRemove[i]);
            return false;
        }
    }

    if (pairwiseAlignments) {
        TRACE_MESSAGE("creating new pairwise alignments");
        ncbi::EDiagSev oldLevel = SetDiagPostLevel(eDiag_Warning);    // otherwise, info messages take a long time if lots of rows

        UngappedAlignedBlockList uaBlocks;
        GetUngappedAlignedBlocks(&uaBlocks);
        UngappedAlignedBlockList::const_iterator u, ue = uaBlocks.end();

        for (i=0; i<slavesToRemove.size(); ++i) {

            // create new pairwise alignment from each removed row
            SequenceList newSeqs(2);
            newSeqs[0] = m_sequences[0];
            newSeqs[1] = m_sequences[slavesToRemove[i]];
            BlockMultipleAlignment *newAlignment = new BlockMultipleAlignment(newSeqs);
            for (u=uaBlocks.begin(); u!=ue; ++u) {
                UngappedAlignedBlock *newABlock = new UngappedAlignedBlock(newAlignment);
                const Block::Range *range = (*u)->GetRangeOfRow(0);
                newABlock->SetRangeOfRow(0, range->from, range->to);
                range = (*u)->GetRangeOfRow(slavesToRemove[i]);
                newABlock->SetRangeOfRow(1, range->from, range->to);
                newABlock->m_width = range->to - range->from + 1;
                newAlignment->AddAlignedBlockAtEnd(newABlock);
            }
            if (!newAlignment->AddUnalignedBlocks() ||
                !newAlignment->UpdateBlockMap()) {
                ERROR_MESSAGE("BlockMultipleAlignment::ExtractRows() - error creating new alignment");
                return false;
            }

            pairwiseAlignments->push_back(newAlignment);
        }
        SetDiagPostLevel(oldLevel);
    }

    // remove sequences
    TRACE_MESSAGE("deleting sequences");
    VectorRemoveElements(m_sequences, removeRows, slavesToRemove.size());
    VectorRemoveElements(m_rowDoubles, removeRows, slavesToRemove.size());
    VectorRemoveElements(m_rowStrings, removeRows, slavesToRemove.size());

    // delete row from all blocks, removing any zero-width blocks
    TRACE_MESSAGE("deleting alignment rows from blocks");
    BlockList::iterator b = m_blocks.begin(), br, be = m_blocks.end();
    while (b != be) {
        (*b)->DeleteRows(removeRows, slavesToRemove.size());
        if ((*b)->m_width == 0) {
            br = b;
            ++b;
            TRACE_MESSAGE("deleting block resized to zero width");
            RemoveBlock(*br);
        } else
            ++b;
    }

    // update total alignment m_width
    UpdateBlockMap();
    InitCache();
    return true;
}

bool BlockMultipleAlignment::MergeAlignment(const BlockMultipleAlignment *newAlignment)
{
    // check to see if the alignment is compatible - must have same master sequence
    // and blocks of new alignment must contain m_blocks of this alignment; at same time,
    // build up map of aligned blocks in new alignment that correspond to aligned blocks
    // of this object, for convenient lookup later
    if (newAlignment->GetMaster() != GetMaster())
        return false;

    const Block::Range *newRange, *thisRange;
    BlockList::const_iterator nb, nbe = newAlignment->m_blocks.end();
    BlockList::iterator b, be = m_blocks.end();
    typedef map < UngappedAlignedBlock *, const UngappedAlignedBlock * > AlignedBlockMap;
    AlignedBlockMap correspondingNewBlocks;

    for (b=m_blocks.begin(); b!=be; ++b) {
        if (!(*b)->IsAligned())
            continue;
        for (nb=newAlignment->m_blocks.begin(); nb!=nbe; ++nb) {
            if (!(*nb)->IsAligned())
                continue;

            newRange = (*nb)->GetRangeOfRow(0);
            thisRange = (*b)->GetRangeOfRow(0);
            if (newRange->from <= thisRange->from && newRange->to >= thisRange->to) {
                correspondingNewBlocks[dynamic_cast<UngappedAlignedBlock*>(b->GetPointer())] =
                    dynamic_cast<const UngappedAlignedBlock*>(nb->GetPointer());
                break;
            }
        }
        if (nb == nbe) return false;    // no corresponding block found
    }

    // add slave sequences from new alignment; also copy scores/status
    unsigned int i, nNewRows = newAlignment->m_sequences.size() - 1;
    m_sequences.resize(m_sequences.size() + nNewRows);
    m_rowDoubles.resize(m_rowDoubles.size() + nNewRows);
    m_rowStrings.resize(m_rowStrings.size() + nNewRows);
    for (i=0; i<nNewRows; ++i) {
        m_sequences[m_sequences.size() + i - nNewRows] = newAlignment->m_sequences[i + 1];
        SetRowDouble(NRows() + i - nNewRows, newAlignment->GetRowDouble(i + 1));
        SetRowStatusLine(NRows() + i - nNewRows, newAlignment->GetRowStatusLine(i + 1));
    }

    // now that we know blocks are compatible, add new rows at end of this alignment, containing
    // all rows (except master) from new alignment; only that part of the aligned blocks from
    // the new alignment that intersect with the aligned blocks from this object are added, so
    // that this object's block structure is unchanged

    // resize all aligned blocks
    for (b=m_blocks.begin(); b!=be; ++b)
        (*b)->AddRows(nNewRows);

    // set ranges of aligned blocks, and add them to the list
    AlignedBlockMap::const_iterator ab, abe = correspondingNewBlocks.end();
    const Block::Range *thisMaster, *newMaster;
    for (ab=correspondingNewBlocks.begin(); ab!=abe; ++ab) {
        thisMaster = ab->first->GetRangeOfRow(0);
        newMaster = ab->second->GetRangeOfRow(0);
        for (i=0; i<nNewRows; ++i) {
            newRange = ab->second->GetRangeOfRow(i + 1);
            ab->first->SetRangeOfRow(NRows() + i - nNewRows,
                newRange->from + thisMaster->from - newMaster->from,
                newRange->to + thisMaster->to - newMaster->to);
        }
    }

    // delete then recreate the unaligned blocks, in case a merge requires the
    // creation of a new unaligned block
    for (b=m_blocks.begin(); b!=be; ) {
        if (!(*b)->IsAligned()) {
            BlockList::iterator bb(b);
            ++bb;
            m_blocks.erase(b);
            b = bb;
        } else
            ++b;
    }
    InitCache();

    // update this alignment, but leave row scores/status alone
    if (!AddUnalignedBlocks() || !UpdateBlockMap(false)) {
        ERROR_MESSAGE("BlockMultipleAlignment::MergeAlignment() - internal update after merge failed");
        return false;
    }
    return true;
}

template < class T >
bool ReorderVector(T& v, const std::vector < unsigned int >& newOrder)
{
    // check validity of new ordering
    if (newOrder.size() != v.size()) {
        ERROR_MESSAGE("ReorderVector() - wrong size newOrder");
        return false;
    }
    vector < bool > isPresent(v.size(), false);
    unsigned int r;
    for (r=0; r<v.size(); r++) {
        if (isPresent[newOrder[r]]) {
            ERROR_MESSAGE("ReorderVector() - invalid newOrder: repeated/missing row");
            return false;
        }
        isPresent[newOrder[r]] = true;
    }

    // not terribly efficient - makes a whole new copy with the new order, then re-copies back
    T reordered(v.size());
    for (r=0; r<v.size(); r++)
        reordered[r] = v[newOrder[r]];
    v = reordered;

    return true;
}

bool BlockMultipleAlignment::ReorderRows(const std::vector < unsigned int >& newOrder)
{
    // can't reorder master
    if (newOrder[0] != 0) {
        ERROR_MESSAGE("ReorderRows() - can't move master row");
        return false;
    }
    bool okay =
        (ReorderVector(m_sequences, newOrder) &&
         ReorderVector(m_rowDoubles, newOrder) &&
         ReorderVector(m_rowStrings, newOrder));
    if (!okay) {
        ERROR_MESSAGE("reordering of sequences and status info failed");
        return false;
    }
    BlockList::iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b)
        okay = (okay && (*b)->ReorderRows(newOrder));
    if (!okay)
        ERROR_MESSAGE("reordering of block ranges failed");
    return okay;
}

CSeq_align * CreatePairwiseSeqAlignFromMultipleRow(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment::UngappedAlignedBlockList& m_blocks, unsigned int slaveRow)
{
    if (!multiple || slaveRow >= multiple->NRows()) {
        ERROR_MESSAGE("CreatePairwiseSeqAlignFromMultipleRow() - bad parameters");
        return NULL;
    }

    CSeq_align *seqAlign = new CSeq_align();
    seqAlign->SetType(CSeq_align::eType_partial);
    seqAlign->SetDim(2);

    CSeq_align::C_Segs::TDendiag& denDiags = seqAlign->SetSegs().SetDendiag();
    denDiags.resize((m_blocks.size() > 0) ? m_blocks.size() : 1);

    CSeq_align::C_Segs::TDendiag::iterator d, de = denDiags.end();
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b = m_blocks.begin();
    const Block::Range *range;
    for (d=denDiags.begin(); d!=de; ++d, ++b) {

        CDense_diag *denDiag = new CDense_diag();
        d->Reset(denDiag);
        denDiag->SetDim(2);
        denDiag->SetIds().resize(2);

        // master row
        CRef < CSeq_id > id(new CSeq_id());
        id->Assign(multiple->GetSequenceOfRow(0)->GetPreferredIdentifier());
        denDiag->SetIds().front() = id;
        if (m_blocks.size() > 0) {
            range = (*b)->GetRangeOfRow(0);
            denDiag->SetStarts().push_back(range->from);
        } else
            denDiag->SetStarts().push_back(0);

        // slave row
        id.Reset(new CSeq_id());
        id->Assign(multiple->GetSequenceOfRow(slaveRow)->GetPreferredIdentifier());
        denDiag->SetIds().back() = id;
        if (m_blocks.size() > 0) {
            range = (*b)->GetRangeOfRow(slaveRow);
            denDiag->SetStarts().push_back(range->from);
        } else
            denDiag->SetStarts().push_back(0);

        // block m_width
        denDiag->SetLen((m_blocks.size() > 0) ? (*b)->m_width : 0);
    }

    return seqAlign;
}

unsigned int BlockMultipleAlignment::NAlignedBlocks(void) const
{
    unsigned int n = 0;
    BlockList::const_iterator b, be = m_blocks.end();
    for (b=m_blocks.begin(); b!=be; ++b)
        if ((*b)->IsAligned())
            ++n;
    return n;
}

unsigned int BlockMultipleAlignment::GetAlignmentIndex(unsigned int row, unsigned int seqIndex, eUnalignedJustification justification)
{
    if (row >= NRows() || seqIndex >= GetSequenceOfRow(row)->Length()) {
        ERROR_MESSAGE("BlockMultipleAlignment::GetAlignmentIndex() - coordinate out of range");
        return eUndefined;
    }

    unsigned int alignmentIndex, blockColumn;
    const Block *block = NULL;
    const Block::Range *range;

    for (alignmentIndex=0; alignmentIndex<m_totalWidth; ++alignmentIndex) {

        // check each block to see if index is in range
        if (block != m_blockMap[alignmentIndex].block) {
            block = m_blockMap[alignmentIndex].block;

            range = block->GetRangeOfRow(row);
            if ((int)seqIndex >= range->from && (int)seqIndex <= range->to) {

                // override requested justification for end blocks
                if (block == m_blocks.back()) // also true if there's a single aligned block
                    justification = eLeft;
                else if (block == m_blocks.front())
                    justification = eRight;

                // search block columns to find index (inefficient, but avoids having to write
                // inverse functions of Block::GetIndexAt()
                for (blockColumn=0; blockColumn<block->m_width; ++blockColumn) {
                    if (seqIndex == block->GetIndexAt(blockColumn, row, justification))
                        return alignmentIndex + blockColumn;
                }
                ERROR_MESSAGE("BlockMultipleAlignment::GetAlignmentIndex() - can't find index in block");
                return eUndefined;
            }
        }
    }

    // should never get here...
    ERROR_MESSAGE("BlockMultipleAlignment::GetAlignmentIndex() - confused");
    return eUndefined;
}


bool Block::ReorderRows(const std::vector < unsigned int >& newOrder)
{
    return ReorderVector(m_ranges, newOrder);
}


///// UngappedAlignedBlock methods /////

char UngappedAlignedBlock::GetCharacterAt(unsigned int blockColumn, unsigned int row) const
{
    return m_parentAlignment->GetSequenceOfRow(row)->m_sequenceString[GetIndexAt(blockColumn, row)];
}

Block * UngappedAlignedBlock::Clone(const BlockMultipleAlignment *newMultiple) const
{
    UngappedAlignedBlock *copy = new UngappedAlignedBlock(newMultiple);
    const Block::Range *range;
    for (unsigned int row=0; row<NSequences(); ++row) {
        range = GetRangeOfRow(row);
        copy->SetRangeOfRow(row, range->from, range->to);
    }
    copy->m_width = m_width;
    return copy;
}

void UngappedAlignedBlock::DeleteRow(unsigned int row)
{
    RangeList::iterator r = m_ranges.begin();
    for (unsigned int i=0; i<row; ++i)
        ++r;
    m_ranges.erase(r);
}

void UngappedAlignedBlock::DeleteRows(vector < bool >& removeRows, unsigned int nToRemove)
{
    VectorRemoveElements(m_ranges, removeRows, nToRemove);
}


///// UnalignedBlock methods /////

unsigned int UnalignedBlock::GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const
{
    const Block::Range *range = GetRangeOfRow(row);
    unsigned int seqIndex = BlockMultipleAlignment::eUndefined, rangeWidth, rangeMiddle, extraSpace;

    switch (justification) {
        case BlockMultipleAlignment::eLeft:
            seqIndex = range->from + blockColumn;
            break;
        case BlockMultipleAlignment::eRight:
            seqIndex = range->to - m_width + blockColumn + 1;
            break;
        case BlockMultipleAlignment::eCenter:
            rangeWidth = (range->to - range->from + 1);
            extraSpace = (m_width - rangeWidth) / 2;
            if (blockColumn < extraSpace || blockColumn >= extraSpace + rangeWidth)
                seqIndex = BlockMultipleAlignment::eUndefined;
            else
                seqIndex = range->from + blockColumn - extraSpace;
            break;
        case BlockMultipleAlignment::eSplit:
            rangeWidth = (range->to - range->from + 1);
            rangeMiddle = (rangeWidth / 2) + (rangeWidth % 2);
            extraSpace = m_width - rangeWidth;
            if (blockColumn < rangeMiddle)
                seqIndex = range->from + blockColumn;
            else if (blockColumn >= extraSpace + rangeMiddle)
                seqIndex = range->to - m_width + blockColumn + 1;
            else
                seqIndex = BlockMultipleAlignment::eUndefined;
            break;
    }
    if ((int)seqIndex < range->from || (int)seqIndex > range->to)
        seqIndex = BlockMultipleAlignment::eUndefined;

    return seqIndex;
}

void UnalignedBlock::Resize(void)
{
    m_width = 0;
    for (unsigned int i=0; i<NSequences(); ++i) {
        unsigned int blockWidth = m_ranges[i].to - m_ranges[i].from + 1;
        if (blockWidth > m_width)
            m_width = blockWidth;
    }
}

void UnalignedBlock::DeleteRow(unsigned int row)
{
    RangeList::iterator r = m_ranges.begin();
    for (unsigned int i=0; i<row; ++i)
        ++r;
    m_ranges.erase(r);
    Resize();
}

void UnalignedBlock::DeleteRows(vector < bool >& removeRows, unsigned int nToRemove)
{
    VectorRemoveElements(m_ranges, removeRows, nToRemove);
    Resize();
}

Block * UnalignedBlock::Clone(const BlockMultipleAlignment *newMultiple) const
{
    UnalignedBlock *copy = new UnalignedBlock(newMultiple);
    const Block::Range *range;
    for (unsigned int row=0; row<NSequences(); ++row) {
        range = GetRangeOfRow(row);
        copy->SetRangeOfRow(row, range->from, range->to);
    }
    copy->m_width = m_width;
    return copy;
}

END_SCOPE(struct_util)
