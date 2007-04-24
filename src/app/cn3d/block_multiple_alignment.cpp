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

#include <objects/seqalign/Dense_diag.hpp>

#include "remove_header_conflicts.hpp"

#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "molecule.hpp"
#include "conservation_colorer.hpp"
#include "style_manager.hpp"
#include "structure_set.hpp"
#include "messenger.hpp"
#include "cn3d_colors.hpp"
#include "alignment_manager.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_threader.hpp"
#include "cn3d_pssm.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

BlockMultipleAlignment::BlockMultipleAlignment(SequenceList *sequenceList, AlignmentManager *alnMgr) :
    alignmentManager(alnMgr), sequences(sequenceList), conservationColorer(NULL),
    pssm(NULL), showGeometryViolations(false),
    alignMasterFrom(-1), alignMasterTo(-1), alignDependentFrom(-1), alignDependentTo(-1)
{
    InitCache();
    rowDoubles.resize(sequenceList->size(), 0.0);
    rowStrings.resize(sequenceList->size());
    geometryViolations.resize(sequenceList->size());

    // create conservation colorer
    conservationColorer = new ConservationColorer(this);
}

void BlockMultipleAlignment::InitCache(void)
{
    cachePrevRow = kMax_UInt;
    cachePrevBlock = NULL;
    cacheBlockIterator = blocks.begin();
}

BlockMultipleAlignment::~BlockMultipleAlignment(void)
{
    BlockList::iterator i, ie = blocks.end();
    for (i=blocks.begin(); i!=ie; ++i) delete *i;
    delete sequences;
    delete conservationColorer;
    RemovePSSM();
}

BlockMultipleAlignment * BlockMultipleAlignment::Clone(void) const
{
    // must actually copy the list
    BlockMultipleAlignment *copy = new BlockMultipleAlignment(new SequenceList(*sequences), alignmentManager);
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        Block *newBlock = (*b)->Clone(copy);
        copy->blocks.push_back(newBlock);
        if ((*b)->IsAligned()) {
            MarkBlockMap::const_iterator r = markBlocks.find(*b);
            if (r != markBlocks.end())
                copy->markBlocks[newBlock] = true;
        }
    }
    copy->UpdateBlockMapAndColors();
    copy->rowDoubles = rowDoubles;
    copy->rowStrings = rowStrings;
    copy->geometryViolations = geometryViolations;
    copy->showGeometryViolations = showGeometryViolations;
    copy->updateOrigin = updateOrigin;
    copy->alignMasterFrom = alignMasterFrom;
    copy->alignMasterTo = alignMasterTo;
    copy->alignDependentFrom = alignDependentFrom;
    copy->alignDependentTo = alignDependentTo;
    return copy;
}

const PSSMWrapper& BlockMultipleAlignment::GetPSSM(void) const
{
    if (!pssm)
        pssm = new PSSMWrapper(this);
    return *pssm;
}

void BlockMultipleAlignment::RemovePSSM(void) const
{
    if (pssm) {
        delete pssm;
        pssm = NULL;
    }
}

void BlockMultipleAlignment::FreeColors(void)
{
    conservationColorer->FreeColors();
    RemovePSSM();
}

bool BlockMultipleAlignment::CheckAlignedBlock(const Block *block) const
{
    if (!block || !block->IsAligned()) {
        ERRORMSG("BlockMultipleAlignment::CheckAlignedBlock() - checks aligned blocks only");
        return false;
    }
    if (block->NSequences() != sequences->size()) {
        ERRORMSG("BlockMultipleAlignment::CheckAlignedBlock() - block size mismatch");
        return false;
    }

    // make sure ranges are reasonable for each sequence
    unsigned int row;
    const Block
        *prevBlock = GetBlockBefore(block),
        *nextBlock = GetBlockAfter(block);
    const Block::Range *range, *prevRange = NULL, *nextRange = NULL;
    SequenceList::const_iterator sequence = sequences->begin();
    for (row=0; row<block->NSequences(); ++row, ++sequence) {
        range = block->GetRangeOfRow(row);
        if (prevBlock) prevRange = prevBlock->GetRangeOfRow(row);
        if (nextBlock) nextRange = nextBlock->GetRangeOfRow(row);
        if (range->to - range->from + 1 != (int)block->width ||     // check block width
            (prevRange && range->from <= prevRange->to) ||          // check for range overlap
            (nextRange && range->to >= nextRange->from) ||          // check for range overlap
            range->from > range->to ||                              // check range values
            range->to >= (int)(*sequence)->Length()) {              // check bounds of end
            ERRORMSG("BlockMultipleAlignment::CheckAlignedBlock() - range error");
            return false;
        }
    }

    return true;
}

bool BlockMultipleAlignment::AddAlignedBlockAtEnd(UngappedAlignedBlock *newBlock)
{
	bool okay = CheckAlignedBlock(newBlock);
    if (okay)
		blocks.push_back(newBlock);
    return okay;
}

UnalignedBlock * BlockMultipleAlignment::
    CreateNewUnalignedBlockBetween(const Block *leftBlock, const Block *rightBlock)
{
    if ((leftBlock && !leftBlock->IsAligned()) || (rightBlock && !rightBlock->IsAligned())) {
        ERRORMSG("CreateNewUnalignedBlockBetween() - passed an unaligned block");
        return NULL;
    }

    unsigned int row, length;
    int from, to;
    SequenceList::const_iterator s, se = sequences->end();

    UnalignedBlock *newBlock = new UnalignedBlock(this);
    newBlock->width = 0;

    for (row=0, s=sequences->begin(); s!=se; ++row, ++s) {

        if (leftBlock)
            from = leftBlock->GetRangeOfRow(row)->to + 1;
        else
            from = 0;

        if (rightBlock)
            to = rightBlock->GetRangeOfRow(row)->from - 1;
        else
            to = (*s)->Length() - 1;

        newBlock->SetRangeOfRow(row, from, to);

        if (to + 1 < from) { // just to make sure...
            ERRORMSG("CreateNewUnalignedBlockBetween() - unaligned length < 0");
            return NULL;
        }
        length = to - from + 1;
        if (length > newBlock->width)
            newBlock->width = length;
    }

    if (newBlock->width == 0) {
        delete newBlock;
        return NULL;
    } else
        return newBlock;
}

bool BlockMultipleAlignment::AddUnalignedBlocks(void)
{
    BlockList::iterator a, ae = blocks.end();
    const Block *alignedBlock = NULL, *prevAlignedBlock = NULL;
    Block *newUnalignedBlock;

    // unaligned blocks to the left of each aligned block
    for (a=blocks.begin(); a!=ae; ++a) {
        alignedBlock = *a;
        newUnalignedBlock = CreateNewUnalignedBlockBetween(prevAlignedBlock, alignedBlock);
        if (newUnalignedBlock) blocks.insert(a, newUnalignedBlock);
        prevAlignedBlock = alignedBlock;
    }

    // right tail
    newUnalignedBlock = CreateNewUnalignedBlockBetween(alignedBlock, NULL);
    if (newUnalignedBlock)
        blocks.insert(a, newUnalignedBlock);

    return true;
}

bool BlockMultipleAlignment::UpdateBlockMapAndColors(bool clearRowInfo)
{
    unsigned int i = 0, j;
    int n = 0;
    BlockList::iterator b, be = blocks.end();

    // reset old stuff, recalculate width
    totalWidth = 0;
    for (b=blocks.begin(); b!=be; ++b) totalWidth += (*b)->width;
//    TESTMSG("alignment display size: " << totalWidth << " x " << NRows());

    // fill out the block map
    conservationColorer->Clear();
    blockMap.resize(totalWidth);
    UngappedAlignedBlock *aBlock;
    for (b=blocks.begin(); b!=be; ++b) {
        aBlock = dynamic_cast<UngappedAlignedBlock*>(*b);
        if (aBlock) {
            conservationColorer->AddBlock(aBlock);
            ++n;
        }
        for (j=0; j<(*b)->width; ++j, ++i) {
            blockMap[i].block = *b;
            blockMap[i].blockColumn = j;
            blockMap[i].alignedBlockNum = (aBlock ? n : -1);
        }
    }

    // if alignment changes, any pssm/scores/status/special colors become invalid
    RemovePSSM();
    if (clearRowInfo) ClearRowInfo();
    ShowGeometryViolations(showGeometryViolations); // recalculate GV's

    return true;
}

bool BlockMultipleAlignment::GetCharacterTraitsAt(
    unsigned int alignmentColumn, unsigned int row, eUnalignedJustification justification,
    char *character, Vector *color, bool *isHighlighted,
    bool *drawBackground, Vector *cellBackgroundColor) const
{
    const Sequence *sequence;
    int seqIndex;
    bool isAligned;

    if (!GetSequenceAndIndexAt(alignmentColumn, row, justification, &sequence, &seqIndex, &isAligned))
        return false;

    *character = (seqIndex >= 0) ? sequence->sequenceString[seqIndex] : '~';
    if (isAligned)
        *character = toupper((unsigned char)(*character));
    else
        *character = tolower((unsigned char)(*character));

    // try to color by molecule first
    if (sequence->molecule) {
        *color = (seqIndex >= 0) ?
            sequence->molecule->GetResidueColor(seqIndex) :
            GlobalColors()->Get(Colors::eNoCoordinates);;
    }

    // otherwise (for unstructured sequence):
    else {
        StyleSettings::eColorScheme colorScheme =
            sequence->parentSet->styleManager->GetGlobalStyle().proteinBackbone.colorScheme;

        // color by hydrophobicity
        if (sequence->isProtein && colorScheme == StyleSettings::eHydrophobicity) {
            double hydrophobicity = GetHydrophobicity(toupper((unsigned char)(*character)));
            *color = (hydrophobicity != UNKNOWN_HYDROPHOBICITY) ?
                GlobalColors()->Get(Colors::eHydrophobicityMap, hydrophobicity) :
                GlobalColors()->Get(Colors::eNoHydrophobicity);
        }
        // or color by charge
        else if (sequence->isProtein && colorScheme == StyleSettings::eCharge) {
            int charge = GetCharge(toupper((unsigned char)(*character)));
            *color = GlobalColors()->Get(
                (charge > 0) ? Colors::ePositive : ((charge < 0) ? Colors::eNegative : Colors::eNeutral));;
        }
        // else, color by alignment color
        else {
            const Vector *aColor;
            if (isAligned && (aColor = GetAlignmentColor(row, seqIndex, colorScheme)) != NULL) {
                *color = *aColor;
            } else {
                *color = GlobalColors()->Get(Colors::eUnaligned);
            }
        }
    }

    if (seqIndex >= 0)
        *isHighlighted = GlobalMessenger()->IsHighlighted(sequence, seqIndex);
    else
        *isHighlighted = false;

    // add special alignment coloring (but don't override highlight)
    *drawBackground = false;
    if (!(*isHighlighted)) {

        // check for block flagged for realignment
        if (markBlocks.find(blockMap[alignmentColumn].block) != markBlocks.end()) {
            *drawBackground = true;
            *cellBackgroundColor = GlobalColors()->Get(Colors::eMarkBlock);
        }

        // optionally show geometry violations
        if (showGeometryViolations && seqIndex >= 0 && geometryViolations[row][seqIndex]) {
            *drawBackground = true;
            *cellBackgroundColor = GlobalColors()->Get(Colors::eGeometryViolation);
        }

        // check for unmergeable alignment
        const BlockMultipleAlignment *referenceAlignment = alignmentManager->GetCurrentMultipleAlignment();
        if (referenceAlignment && referenceAlignment != this &&
			seqIndex >= 0 && GetMaster() == referenceAlignment->GetMaster()) {

            bool unmergeable = false;
            const Block *block = blockMap[alignmentColumn].block;

            // case where master residues are aligned in multiple but not in this one
            if (row == 0 && !isAligned && referenceAlignment->IsAligned(row, seqIndex))
                unmergeable = true;

            // block boundaries in this inside aligned block of multiple
            else if (row > 0 && isAligned) {
                const Block::Range *range = block->GetRangeOfRow(row);
                bool
                    isLeftEdge = (seqIndex == range->from),
                    isRightEdge = (seqIndex == range->to);
                if (isLeftEdge || isRightEdge) {

                    // get corresponding block of multiple
                    const Block::Range *masterRange = block->GetRangeOfRow(0);
                    unsigned int masterIndex = masterRange->from + seqIndex - range->from;
                    const Block *multipleBlock = referenceAlignment->GetBlock(0, masterIndex);
                    masterRange = multipleBlock->GetRangeOfRow(0);

                    if (multipleBlock->IsAligned() &&
                        ((isLeftEdge && (int)masterIndex > masterRange->from) ||
                         (isRightEdge && (int)masterIndex < masterRange->to)))
                        unmergeable = true;
                }
            }

            // check for inserts relative to the multiple
            else if (row > 0 && !isAligned) {
                const Block
                    *blockBefore = GetBlockBefore(block),
                    *blockAfter = GetBlockAfter(block),
                    *refBlock;
                if (blockBefore && blockBefore->IsAligned() && blockAfter && blockAfter->IsAligned() &&
                        referenceAlignment->GetBlock(0, blockBefore->GetRangeOfRow(0)->to) ==
                        (refBlock=referenceAlignment->GetBlock(0, blockAfter->GetRangeOfRow(0)->from)) &&
                        refBlock->IsAligned()
                    )
                    unmergeable = true;
            }

            if (unmergeable) {
                *drawBackground = true;
                *cellBackgroundColor = GlobalColors()->Get(Colors::eMergeFail);
            }
        }
    }

    return true;
}

bool BlockMultipleAlignment::GetSequenceAndIndexAt(
    unsigned int alignmentColumn, unsigned int row, eUnalignedJustification requestedJustification,
    const Sequence **sequence, int *index, bool *isAligned) const
{
    if (sequence) *sequence = (*sequences)[row];

    const BlockInfo& blockInfo = blockMap[alignmentColumn];

    if (!blockInfo.block->IsAligned()) {
        if (isAligned) *isAligned = false;
        // override requested justification for end blocks
        if (blockInfo.block == blocks.back()) // also true if there's a single aligned block
            requestedJustification = eLeft;
        else if (blockInfo.block == blocks.front())
            requestedJustification = eRight;
    } else
        if (isAligned) *isAligned = true;

    if (index)
        *index = blockInfo.block->GetIndexAt(blockInfo.blockColumn, row, requestedJustification);

    return true;
}

int BlockMultipleAlignment::GetRowForSequence(const Sequence *sequence) const
{
    // this only works for structured sequences, since non-structure sequences can
    // be repeated any number of times in the alignment; assumes repeated structures
    // will each have a unique Sequence object
    if (!sequence || !sequence->molecule) {
        ERRORMSG("BlockMultipleAlignment::GetRowForSequence() - Sequence must have associated structure");
        return -1;
    }

    if (cachePrevRow >= NRows() || sequence != (*sequences)[cachePrevRow]) {
        unsigned int row;
        for (row=0; row<NRows(); ++row) if ((*sequences)[row] == sequence) break;
        if (row == NRows()) {
//            ERRORMSG("BlockMultipleAlignment::GetRowForSequence() - can't find given Sequence");
            return -1;
        }
        cachePrevRow = row;
    }
    return cachePrevRow;
}

const Vector * BlockMultipleAlignment::GetAlignmentColor(
    unsigned int row, unsigned int seqIndex, StyleSettings::eColorScheme colorScheme) const
{
     const UngappedAlignedBlock *block = dynamic_cast<const UngappedAlignedBlock*>(GetBlock(row, seqIndex));
     if (!block || !block->IsAligned()) {
        WARNINGMSG("BlockMultipleAlignment::GetAlignmentColor() called on unaligned residue");
        return NULL;
    }

    const Vector *alignedColor;

    switch (colorScheme) {
        case StyleSettings::eAligned:
            alignedColor = GlobalColors()->Get(Colors::eConservationMap, Colors::nConservationMap - 1);
            break;
        case StyleSettings::eIdentity:
            alignedColor = conservationColorer->
                GetIdentityColor(block, seqIndex - block->GetRangeOfRow(row)->from);
            break;
        case StyleSettings::eVariety:
            alignedColor = conservationColorer->
                GetVarietyColor(block, seqIndex - block->GetRangeOfRow(row)->from);
            break;
        case StyleSettings::eWeightedVariety:
            alignedColor = conservationColorer->
                GetWeightedVarietyColor(block, seqIndex - block->GetRangeOfRow(row)->from);
            break;
        case StyleSettings::eInformationContent:
            alignedColor = conservationColorer->
                GetInformationContentColor(block, seqIndex - block->GetRangeOfRow(row)->from);
            break;
        case StyleSettings::eFit:
            alignedColor = conservationColorer->
                GetFitColor(block, seqIndex - block->GetRangeOfRow(row)->from, row);
            break;
        case StyleSettings::eBlockFit:
            alignedColor = conservationColorer->GetBlockFitColor(block, row);
            break;
        case StyleSettings::eBlockZFit:
            alignedColor = conservationColorer->GetBlockZFitColor(block, row);
            break;
        case StyleSettings::eBlockRowFit:
            alignedColor = conservationColorer->GetBlockRowFitColor(block, row);
            break;
        default:
            alignedColor = NULL;
    }

    return alignedColor;
}

const Vector * BlockMultipleAlignment::GetAlignmentColor(
    const Sequence *sequence, unsigned int seqIndex, StyleSettings::eColorScheme colorScheme) const
{
    int row = GetRowForSequence(sequence);
    if (row < 0) return NULL;
    return GetAlignmentColor(row, seqIndex, colorScheme);
}

bool BlockMultipleAlignment::IsAligned(unsigned int row, unsigned int seqIndex) const
{
    const Block *block = GetBlock(row, seqIndex);
    return (block && block->IsAligned());
}

const Block * BlockMultipleAlignment::GetBlock(unsigned int row, unsigned int seqIndex) const
{
    // make sure we're in range for this sequence
    if (row >= NRows() || seqIndex >= (*sequences)[row]->Length()) {
        ERRORMSG("BlockMultipleAlignment::GetBlock() - coordinate out of range");
        return NULL;
    }

    const Block::Range *range;

    // first check to see if it's in the same block as last time.
    if (cachePrevBlock) {
        range = cachePrevBlock->GetRangeOfRow(row);
        if ((int)seqIndex >= range->from && (int)seqIndex <= range->to) return cachePrevBlock;
        ++cacheBlockIterator; // start search at next block
    } else {
        cacheBlockIterator = blocks.begin();
    }

    // otherwise, perform block search. This search is most efficient when queries
    // happen in order from left to right along a given row.
    do {
        if (cacheBlockIterator == blocks.end()) cacheBlockIterator = blocks.begin();
        range = (*cacheBlockIterator)->GetRangeOfRow(row);
        if ((int)seqIndex >= range->from && (int)seqIndex <= range->to) {
            cachePrevBlock = *cacheBlockIterator; // cache this block
            return cachePrevBlock;
        }
        ++cacheBlockIterator;
    } while (1);
}

int BlockMultipleAlignment::GetFirstAlignedBlockPosition(void) const
{
    BlockList::const_iterator b = blocks.begin();
    if (blocks.size() > 0 && (*b)->IsAligned()) // first block is aligned
        return 0;
    else if (blocks.size() >= 2 && (*(++b))->IsAligned()) // second block is aligned
        return blocks.front()->width;
    else
        return -1;
}

int BlockMultipleAlignment::GetAlignedDependentIndex(unsigned int masterSeqIndex, unsigned int dependentRow) const
{
    const UngappedAlignedBlock
        *aBlock = dynamic_cast<const UngappedAlignedBlock*>(GetBlock(0, masterSeqIndex));
    if (!aBlock) return -1;

    const Block::Range
        *masterRange = aBlock->GetRangeOfRow(0),
        *dependentRange = aBlock->GetRangeOfRow(dependentRow);
    return (dependentRange->from + masterSeqIndex - masterRange->from);
}

void BlockMultipleAlignment::SelectedRange(unsigned int row, unsigned int alnIndexFrom, unsigned int alnIndexTo,
    eUnalignedJustification justification, bool toggle) const
{
    // translate from,to (alignment columns) into sequence indexes
    const Sequence *sequence;
    int fromSeqIndex, toSeqIndex;
    bool ignored;

    // trim selection area to size of this alignment
    if (alnIndexTo >= totalWidth) alnIndexTo = totalWidth - 1;

    // find first residue within range
    while (alnIndexFrom <= alnIndexTo) {
        GetSequenceAndIndexAt(alnIndexFrom, row, justification, &sequence, &fromSeqIndex, &ignored);
        if (fromSeqIndex >= 0) break;
        ++alnIndexFrom;
    }
    if (alnIndexFrom > alnIndexTo) return;

    // find last residue within range
    while (alnIndexTo >= alnIndexFrom) {
        GetSequenceAndIndexAt(alnIndexTo, row, justification, &sequence, &toSeqIndex, &ignored);
        if (toSeqIndex >= 0) break;
        --alnIndexTo;
    }

    if (toggle)
        GlobalMessenger()->ToggleHighlights(sequence, fromSeqIndex, toSeqIndex);
    else
        GlobalMessenger()->AddHighlights(sequence, fromSeqIndex, toSeqIndex);
}

void BlockMultipleAlignment::SelectBlocks(unsigned int alnIndexFrom, unsigned int alnIndexTo, bool toggle) const
{
    typedef vector < pair < unsigned int, unsigned int > > VecPair;
    VecPair highlightRanges;

    BlockList::const_iterator b, be = blocks.end();
    unsigned int start = 0, from, to;
    for (b=blocks.begin(); b!=be; ++b) {
        if ((*b)->IsAligned()) {
            from = start;
            to = start + (*b)->width - 1;
            // highlight if the block intersects with the selection
            if ((alnIndexFrom >= from && alnIndexFrom <= to) || (alnIndexTo >= from && alnIndexTo <= to)
                    || (alnIndexFrom < from && alnIndexTo > to))
                highlightRanges.push_back(make_pair(from, to));
        }
        start += (*b)->width;
    }

    if (highlightRanges.size() == 0)
        return;

    // select all ranges in each row; justification is irrelevant since we're in aligned blocks only
    for (unsigned int row=0; row<NRows(); ++row) {
        VecPair::const_iterator p, pe = highlightRanges.end();
        for (p=highlightRanges.begin(); p!=pe; ++p)
            SelectedRange(row, p->first, p->second, eLeft, toggle);
    }
}

void BlockMultipleAlignment::GetAlignedBlockPosition(unsigned int alignmentIndex,
    int *blockColumn, int *blockWidth) const
{
    *blockColumn = *blockWidth = -1;
    const BlockInfo& info = blockMap[alignmentIndex];
    if (info.block->IsAligned()) {
        *blockColumn = info.blockColumn;
        *blockWidth = info.block->width;
    }
}

Block * BlockMultipleAlignment::GetBlockBefore(const Block *block) const
{
    Block *prevBlock = NULL;
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        if (*b == block) break;
        prevBlock = *b;
    }
    return prevBlock;
}

const UnalignedBlock * BlockMultipleAlignment::GetUnalignedBlockBefore(
    const UngappedAlignedBlock *aBlock) const
{
    const Block *prevBlock;
    if (aBlock)
        prevBlock = GetBlockBefore(aBlock);
    else
        prevBlock = blocks.back();
    return dynamic_cast<const UnalignedBlock*>(prevBlock);
}

const UnalignedBlock * BlockMultipleAlignment::GetUnalignedBlockAfter(
    const UngappedAlignedBlock *aBlock) const
{
    const Block *nextBlock;
    if (aBlock)
        nextBlock = GetBlockAfter(aBlock);
    else
        nextBlock = blocks.front();
    return dynamic_cast<const UnalignedBlock*>(nextBlock);
}

Block * BlockMultipleAlignment::GetBlockAfter(const Block *block) const
{
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        if (*b == block) {
            ++b;
            if (b == be) break;
            return *b;
        }
    }
    return NULL;
}

void BlockMultipleAlignment::InsertBlockBefore(Block *newBlock, const Block *insertAt)
{
    BlockList::iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        if (*b == insertAt) {
            blocks.insert(b, newBlock);
            return;
        }
    }
    WARNINGMSG("BlockMultipleAlignment::InsertBlockBefore() - couldn't find insertAt block");
}

void BlockMultipleAlignment::InsertBlockAfter(const Block *insertAt, Block *newBlock)
{
    BlockList::iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        if (*b == insertAt) {
            ++b;
            blocks.insert(b, newBlock);
            return;
        }
    }
    WARNINGMSG("BlockMultipleAlignment::InsertBlockBefore() - couldn't find insertAt block");
}

void BlockMultipleAlignment::RemoveBlock(Block *block)
{
    BlockList::iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        if (*b == block) {
            delete *b;
            blocks.erase(b);
            InitCache();
            return;
        }
    }
    WARNINGMSG("BlockMultipleAlignment::RemoveBlock() - couldn't find block");
}

bool BlockMultipleAlignment::MoveBlockBoundary(unsigned int columnFrom, unsigned int columnTo)
{
    int blockColumn, blockWidth;
    GetAlignedBlockPosition(columnFrom, &blockColumn, &blockWidth);
    if (blockColumn < 0 || blockWidth <= 0) return false;

    TRACEMSG("trying to move block boundary from " << columnFrom << " to " << columnTo);

    const BlockInfo& info = blockMap[columnFrom];
    unsigned int row;
    int requestedShift = columnTo - columnFrom, actualShift = 0;
    const Block::Range *range;

    // shrink block from left
    if (blockColumn == 0 && requestedShift > 0 && requestedShift < (int)info.block->width) {
        actualShift = requestedShift;
        TRACEMSG("shrinking block from left");
        for (row=0; row<NRows(); ++row) {
            range = info.block->GetRangeOfRow(row);
            info.block->SetRangeOfRow(row, range->from + requestedShift, range->to);
        }
        info.block->width -= requestedShift;
        Block *prevBlock = GetBlockBefore(info.block);
        if (prevBlock && !prevBlock->IsAligned()) {
            for (row=0; row<NRows(); ++row) {
                range = prevBlock->GetRangeOfRow(row);
                prevBlock->SetRangeOfRow(row, range->from, range->to + requestedShift);
            }
            prevBlock->width += requestedShift;
        } else {
            Block *newUnalignedBlock = CreateNewUnalignedBlockBetween(prevBlock, info.block);
            if (newUnalignedBlock) InsertBlockBefore(newUnalignedBlock, info.block);
            TRACEMSG("added new unaligned block");
        }
    }

    // shrink block from right (requestedShift < 0)
    else if (blockColumn == (int)info.block->width - 1 &&
                requestedShift < 0 && -requestedShift < (int)info.block->width) {
        actualShift = requestedShift;
        TRACEMSG("shrinking block from right");
        for (row=0; row<NRows(); ++row) {
            range = info.block->GetRangeOfRow(row);
            info.block->SetRangeOfRow(row, range->from, range->to + requestedShift);
        }
        info.block->width += requestedShift;
        Block *nextBlock = GetBlockAfter(info.block);
        if (nextBlock && !nextBlock->IsAligned()) {
            for (row=0; row<NRows(); ++row) {
                range = nextBlock->GetRangeOfRow(row);
                nextBlock->SetRangeOfRow(row, range->from + requestedShift, range->to);
            }
            nextBlock->width -= requestedShift;
        } else {
            Block *newUnalignedBlock = CreateNewUnalignedBlockBetween(info.block, nextBlock);
            if (newUnalignedBlock) InsertBlockAfter(info.block, newUnalignedBlock);
            TRACEMSG("added new unaligned block");
        }
    }

    // grow block to right
    else if (blockColumn == (int)info.block->width - 1 && requestedShift > 0) {
        Block *nextBlock = GetBlockAfter(info.block);
        if (nextBlock && !nextBlock->IsAligned()) {
            int nRes;
            actualShift = requestedShift;
            for (row=0; row<NRows(); ++row) {
                range = nextBlock->GetRangeOfRow(row);
                nRes = range->to - range->from + 1;
                if (nRes < actualShift) actualShift = nRes;
            }
            if (actualShift) {
                TRACEMSG("growing block to right");
                for (row=0; row<NRows(); ++row) {
                    range = info.block->GetRangeOfRow(row);
                    info.block->SetRangeOfRow(row, range->from, range->to + actualShift);
                    range = nextBlock->GetRangeOfRow(row);
                    nextBlock->SetRangeOfRow(row, range->from + actualShift, range->to);
                }
                info.block->width += actualShift;
                nextBlock->width -= actualShift;
                if (nextBlock->width == 0) {
                    RemoveBlock(nextBlock);
                    TRACEMSG("removed empty block");
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
                TRACEMSG("growing block to left");
                for (row=0; row<NRows(); ++row) {
                    range = info.block->GetRangeOfRow(row);
                    info.block->SetRangeOfRow(row, range->from + actualShift, range->to);
                    range = prevBlock->GetRangeOfRow(row);
                    prevBlock->SetRangeOfRow(row, range->from, range->to + actualShift);
                }
                info.block->width -= actualShift;
                prevBlock->width += actualShift;
                if (prevBlock->width == 0) {
                    RemoveBlock(prevBlock);
                    TRACEMSG("removed empty block");
                }
            }
        }
    }

    if (actualShift != 0) {
        UpdateBlockMapAndColors();
        return true;
    } else
        return false;
}

bool BlockMultipleAlignment::ShiftRow(unsigned int row, unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex,
    eUnalignedJustification justification)
{
    if (fromAlignmentIndex == toAlignmentIndex) return false;

    Block
        *blockFrom = blockMap[fromAlignmentIndex].block,
        *blockTo = blockMap[toAlignmentIndex].block;

    // at least one end of the drag must be in an aligned block
    UngappedAlignedBlock *ABlock =
        dynamic_cast<UngappedAlignedBlock*>(blockFrom);
    if (ABlock) {
        if (blockTo != blockFrom && blockTo->IsAligned()) return false;
    } else {
        ABlock = dynamic_cast<UngappedAlignedBlock*>(blockTo);
        if (!ABlock) return false;
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
        int fromSeqIndex, toSeqIndex;
        GetSequenceAndIndexAt(fromAlignmentIndex, row, justification, NULL, &fromSeqIndex, NULL);
        GetSequenceAndIndexAt(toAlignmentIndex, row, justification, NULL, &toSeqIndex, NULL);
        if (fromSeqIndex < 0 || toSeqIndex < 0) return false;
        requestedShift = toSeqIndex - fromSeqIndex;
    }

    // vs. dragging from aligned
    else {
        requestedShift = toAlignmentIndex - fromAlignmentIndex;
    }

    const Block::Range *prevRange = NULL, *nextRange = NULL,
        *range = ABlock->GetRangeOfRow(row);
    if (prevUABlock) prevRange = prevUABlock->GetRangeOfRow(row);
    if (nextUABlock) nextRange = nextUABlock->GetRangeOfRow(row);
    if (requestedShift > 0) {
        if (prevUABlock) width = prevRange->to - prevRange->from + 1;
        actualShift = (width > requestedShift) ? requestedShift : width;
    } else {
        if (nextUABlock) width = nextRange->to - nextRange->from + 1;
        actualShift = (width > -requestedShift) ? requestedShift : -width;
    }
    if (actualShift == 0) return false;

    TRACEMSG("shifting row " << row << " by " << actualShift);

    ABlock->SetRangeOfRow(row, range->from - actualShift, range->to - actualShift);

    if (prevUABlock) {
        prevUABlock->SetRangeOfRow(row, prevRange->from, prevRange->to - actualShift);
        prevUABlock->width = 0;
        for (unsigned int i=0; i<NRows(); ++i) {
            prevRange = prevUABlock->GetRangeOfRow(i);
            width = prevRange->to - prevRange->from + 1;
            if (width < 0) ERRORMSG("BlockMultipleAlignment::ShiftRow() - negative width on left");
            if (width > (int)prevUABlock->width) prevUABlock->width = width;
        }
        if (prevUABlock->width == 0) {
            TRACEMSG("removing zero-width unaligned block on left");
            RemoveBlock(prevUABlock);
        }
    } else {
        TRACEMSG("creating unaligned block on left");
        prevUABlock = CreateNewUnalignedBlockBetween(GetBlockBefore(ABlock), ABlock);
        InsertBlockBefore(prevUABlock, ABlock);
    }

    if (nextUABlock) {
        nextUABlock->SetRangeOfRow(row, nextRange->from - actualShift, nextRange->to);
        nextUABlock->width = 0;
        for (unsigned int i=0; i<NRows(); ++i) {
            nextRange = nextUABlock->GetRangeOfRow(i);
            width = nextRange->to - nextRange->from + 1;
            if (width < 0) ERRORMSG("BlockMultipleAlignment::ShiftRow() - negative width on right");
            if (width > (int)nextUABlock->width) nextUABlock->width = width;
        }
        if (nextUABlock->width == 0) {
            TRACEMSG("removing zero-width unaligned block on right");
            RemoveBlock(nextUABlock);
        }
    } else {
        TRACEMSG("creating unaligned block on right");
        nextUABlock = CreateNewUnalignedBlockBetween(ABlock, GetBlockAfter(ABlock));
        InsertBlockAfter(ABlock, nextUABlock);
    }

    if (!CheckAlignedBlock(ABlock))
        ERRORMSG("BlockMultipleAlignment::ShiftRow() - shift failed to create valid aligned block");

    UpdateBlockMapAndColors();
    return true;
}

bool BlockMultipleAlignment::ZipAlignResidue(unsigned int row, unsigned int alignmentIndex, bool moveRight, eUnalignedJustification justification)
{
    if (blockMap[alignmentIndex].block->IsAligned()) {
        TRACEMSG("residue is already aligned");
        return false;
    }

    UngappedAlignedBlock *aBlock = dynamic_cast<UngappedAlignedBlock*>(
        moveRight ? GetBlockAfter(blockMap[alignmentIndex].block) : GetBlockBefore(blockMap[alignmentIndex].block));
    if (!aBlock) {
        TRACEMSG("no aligned block to the " << (moveRight ? "right" : "left"));
        return false;
    }

    return ShiftRow(row, alignmentIndex,
        GetAlignmentIndex(row,
            (moveRight ? aBlock->GetRangeOfRow(row)->from : aBlock->GetRangeOfRow(row)->to),
            justification),
        justification);
}

bool BlockMultipleAlignment::OptimizeBlock(unsigned int row, unsigned int alignmentIndex, eUnalignedJustification justification)
{
    // is this an aligned block?
    UngappedAlignedBlock *block = dynamic_cast<UngappedAlignedBlock*>(blockMap[alignmentIndex].block);
    if (!block) {
        TRACEMSG("block is unaligned");
        return false;
    }

    // see if there's any room to shift
    const UnalignedBlock *prevBlock = GetUnalignedBlockBefore(block), *nextBlock = GetUnalignedBlockAfter(block);
    unsigned int maxShiftRight = 0, maxShiftLeft = 0;
    const Block::Range *range;
    if (prevBlock) {
        range = prevBlock->GetRangeOfRow(row);
        maxShiftRight = range->to - range->from + 1;
    }
    if (nextBlock) {
        range = nextBlock->GetRangeOfRow(row);
        maxShiftLeft = range->to - range->from + 1;
    }

    // if this is first or last block, constrain by footprint
    int blockNum = GetAlignedBlockNumber(alignmentIndex);
    if (blockNum == 1 || blockNum == (int)NAlignedBlocks()) {
        int excess = 0;
        if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &excess))
            WARNINGMSG("Can't get footprint excess residues from registry");
        if (blockNum == 1) {
            if ((int)maxShiftRight > excess) {
                WARNINGMSG("maxShiftRight constrained to " << excess << " by footprint excess residues preference");
                maxShiftRight = excess;
            }
        } else { // last block
            if ((int)maxShiftLeft > excess) {
                WARNINGMSG("maxShiftLeft constrained to " << excess << " by footprint excess residues preference");
                maxShiftLeft = excess;
            }
        }
    }

    TRACEMSG("maxShiftRight " << maxShiftRight << ", maxShiftLeft " << maxShiftLeft);
    if (maxShiftRight == 0 && maxShiftLeft == 0) {
        TRACEMSG("no room to shift this block");
        return false;
    }

    // scan all possible block positions, find max information content
    range = block->GetRangeOfRow(row);
    unsigned int bestSeqIndexStart = range->from;
    float score, maxScore = kMin_Float;
    for (unsigned int seqIndexStart = range->from - maxShiftRight; seqIndexStart <= range->from + maxShiftLeft; ++seqIndexStart) {

        // calculate block's info content given each position of this row
        score = 0.0f;
        for (unsigned int blockColumn=0; blockColumn<block->width; ++blockColumn) {

            // create profile for this column
            typedef std::map < char, unsigned int > ColumnProfile;
            ColumnProfile profile;
            ColumnProfile::iterator p, pe;
            for (unsigned int r=0; r<NRows(); ++r) {
                unsigned int seqIndex = (r == row) ? (seqIndexStart + blockColumn) : (block->GetRangeOfRow(r)->from + blockColumn);
                char ch = ScreenResidueCharacter(GetSequenceOfRow(r)->sequenceString[seqIndex]);
                if ((p=profile.find(ch)) != profile.end())
                    ++(p->second);
                else
                    profile[ch] = 1;
            }

            // information content (calculated in bits -> logs of base 2) for this column
            for (p=profile.begin(), pe=profile.end(); p!=pe; ++p) {
                static const float ln2 = log(2.0f), threshhold = 0.0001f;
                float expFreq = GetStandardProbability(p->first);
                if (expFreq > threshhold) {
                    float obsFreq = 1.0f * p->second / NRows(),
                          freqRatio = obsFreq / expFreq;
                    if (freqRatio > threshhold)
                        score += obsFreq * ((float) log(freqRatio)) / ln2; // information content
                }
            }
        }

        // keep track of best position
        if (seqIndexStart == range->from - maxShiftRight || score > maxScore) {
            maxScore = score;
            bestSeqIndexStart = seqIndexStart;
        }
    }

    // if the best position is other than the current, then shift the block accordingly
    bool moved = ((int)bestSeqIndexStart != range->from);
    if (moved) {
        if ((int)bestSeqIndexStart < range->from)
            TRACEMSG("shifting block right by " << (range->from - bestSeqIndexStart));
        else
            TRACEMSG("shifting block left by " << (bestSeqIndexStart - range->from));
        alignmentIndex = GetAlignmentIndex(row, range->from, justification);
        unsigned int alnIdx2 = GetAlignmentIndex(row, bestSeqIndexStart, justification);
        moved = ShiftRow(row, alnIdx2, alignmentIndex, justification);
        if (!moved)
            ERRORMSG("ShiftRow() failed!");
    } else
        TRACEMSG("block was not moved");

    return moved;
}

bool BlockMultipleAlignment::SplitBlock(unsigned int alignmentIndex)
{
    const BlockInfo& info = blockMap[alignmentIndex];
    if (!info.block->IsAligned() || info.block->width < 2 || info.blockColumn == 0)
        return false;

    TRACEMSG("splitting block");
    UngappedAlignedBlock *newAlignedBlock = new UngappedAlignedBlock(this);
    newAlignedBlock->width = info.block->width - info.blockColumn;
    info.block->width = info.blockColumn;

    const Block::Range *range;
    unsigned int oldTo;
    for (unsigned int row=0; row<NRows(); ++row) {
        range = info.block->GetRangeOfRow(row);
        oldTo = range->to;
        info.block->SetRangeOfRow(row, range->from, range->from + info.block->width - 1);
        newAlignedBlock->SetRangeOfRow(row, oldTo - newAlignedBlock->width + 1, oldTo);
    }

    InsertBlockAfter(info.block, newAlignedBlock);
    if (!CheckAlignedBlock(info.block) || !CheckAlignedBlock(newAlignedBlock))
        ERRORMSG("BlockMultipleAlignment::SplitBlock() - split failed to create valid blocks");

    UpdateBlockMapAndColors();
    return true;
}

bool BlockMultipleAlignment::MergeBlocks(unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex)
{
    Block
        *expandedBlock = blockMap[fromAlignmentIndex].block,
        *lastBlock = blockMap[toAlignmentIndex].block;
    if (expandedBlock == lastBlock) return false;
    unsigned int i;
    for (i=fromAlignmentIndex; i<=toAlignmentIndex; ++i)
        if (!blockMap[i].block->IsAligned()) return false;

    TRACEMSG("merging block(s)");
    for (i=0; i<NRows(); ++i)
        expandedBlock->SetRangeOfRow(i,
            expandedBlock->GetRangeOfRow(i)->from, lastBlock->GetRangeOfRow(i)->to);
    expandedBlock->width = lastBlock->GetRangeOfRow(0)->to - expandedBlock->GetRangeOfRow(0)->from + 1;

    Block *deletedBlock = NULL, *blockToDelete;
    for (i=fromAlignmentIndex; i<=toAlignmentIndex; ++i) {
        blockToDelete = blockMap[i].block;
        if (blockToDelete == expandedBlock) continue;
        if (blockToDelete != deletedBlock) {
            deletedBlock = blockToDelete;
            RemoveBlock(blockToDelete);
        }
    }

    if (!CheckAlignedBlock(expandedBlock))
        ERRORMSG("BlockMultipleAlignment::MergeBlocks() - merge failed to create valid block");

    UpdateBlockMapAndColors();
    return true;
}

bool BlockMultipleAlignment::CreateBlock(unsigned int fromAlignmentIndex, unsigned int toAlignmentIndex,
    eUnalignedJustification justification)
{
    const BlockInfo& info = blockMap[fromAlignmentIndex];
    UnalignedBlock *prevUABlock = dynamic_cast<UnalignedBlock*>(info.block);
    if (!prevUABlock || info.block != blockMap[toAlignmentIndex].block) return false;
    int seqIndexFrom, seqIndexTo;
    unsigned int row,
        newBlockWidth = toAlignmentIndex - fromAlignmentIndex + 1,
        origWidth = prevUABlock->width;
    vector < unsigned int > seqIndexesFrom(NRows()), seqIndexesTo(NRows());
    const Sequence *seq;
	bool ignored;
    for (row=0; row<NRows(); ++row) {
        if (!GetSequenceAndIndexAt(fromAlignmentIndex, row, justification, &seq, &seqIndexFrom, &ignored) ||
            !GetSequenceAndIndexAt(toAlignmentIndex, row, justification, &seq, &seqIndexTo, &ignored) ||
            seqIndexFrom < 0 || seqIndexTo < 0 ||
            seqIndexTo - seqIndexFrom + 1 != (int)newBlockWidth)
                return false;
        seqIndexesFrom[row] = seqIndexFrom;
        seqIndexesTo[row] = seqIndexTo;
    }

    TRACEMSG("creating new aligned and unaligned blocks");

    UnalignedBlock *nextUABlock = new UnalignedBlock(this);
    UngappedAlignedBlock *ABlock = new UngappedAlignedBlock(this);
    prevUABlock->width = nextUABlock->width = 0;

    bool deletePrevUABlock = true, deleteNextUABlock = true;
    const Block::Range *prevRange;
    int rangeWidth;
    for (row=0; row<NRows(); ++row) {
        prevRange = prevUABlock->GetRangeOfRow(row);

        nextUABlock->SetRangeOfRow(row, seqIndexesTo[row] + 1, prevRange->to);
        rangeWidth = prevRange->to - seqIndexesTo[row];
        if (rangeWidth < 0)
            ERRORMSG("BlockMultipleAlignment::CreateBlock() - negative nextRange width");
        else if (rangeWidth > 0) {
            if (rangeWidth > (int)nextUABlock->width) nextUABlock->width = rangeWidth;
            deleteNextUABlock = false;
        }

        prevUABlock->SetRangeOfRow(row, prevRange->from, seqIndexesFrom[row] - 1);
        rangeWidth = seqIndexesFrom[row] - prevRange->from;
        if (rangeWidth < 0)
            ERRORMSG("BlockMultipleAlignment::CreateBlock() - negative prevRange width");
        else if (rangeWidth > 0) {
            if (rangeWidth > (int)prevUABlock->width) prevUABlock->width = rangeWidth;
            deletePrevUABlock = false;
        }

        ABlock->SetRangeOfRow(row, seqIndexesFrom[row], seqIndexesTo[row]);
    }

    ABlock->width = newBlockWidth;
    if (prevUABlock->width + ABlock->width + nextUABlock->width != origWidth)
        ERRORMSG("BlockMultipleAlignment::CreateBlock() - bad block widths sum");

    InsertBlockAfter(prevUABlock, ABlock);
    InsertBlockAfter(ABlock, nextUABlock);
    if (deletePrevUABlock) {
        TRACEMSG("deleting zero-width unaligned block on left");
        RemoveBlock(prevUABlock);
    }
    if (deleteNextUABlock) {
        TRACEMSG("deleting zero-width unaligned block on right");
        RemoveBlock(nextUABlock);
    }

    if (!CheckAlignedBlock(ABlock))
        ERRORMSG("BlockMultipleAlignment::CreateBlock() - failed to create valid block");

    UpdateBlockMapAndColors();
    return true;
}

bool BlockMultipleAlignment::DeleteBlock(unsigned int alignmentIndex)
{
    Block *block = blockMap[alignmentIndex].block;
    if (!block || !block->IsAligned()) return false;

    TRACEMSG("deleting block");
    Block
        *prevBlock = GetBlockBefore(block),
        *nextBlock = GetBlockAfter(block);

    // unaligned blocks on both sides - note that total alignment width can change!
    if (prevBlock && !prevBlock->IsAligned() && nextBlock && !nextBlock->IsAligned()) {
        const Block::Range *prevRange, *nextRange;
        unsigned int maxWidth = 0, width;
        for (unsigned int row=0; row<NRows(); ++row) {
            prevRange = prevBlock->GetRangeOfRow(row);
            nextRange = nextBlock->GetRangeOfRow(row);
            width = nextRange->to - prevRange->from + 1;
            prevBlock->SetRangeOfRow(row, prevRange->from, nextRange->to);
            if (width > maxWidth) maxWidth = width;
        }
        prevBlock->width = maxWidth;
        TRACEMSG("removing extra unaligned block");
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
        prevBlock->width += block->width;
    }

    // unaligned block on right only
    else if (nextBlock && !nextBlock->IsAligned()) {
        const Block::Range *range, *nextRange;
        for (unsigned int row=0; row<NRows(); ++row) {
            range = block->GetRangeOfRow(row);
            nextRange = nextBlock->GetRangeOfRow(row);
            nextBlock->SetRangeOfRow(row, range->from, nextRange->to);
        }
        nextBlock->width += block->width;
    }

    // no adjacent unaligned blocks
    else {
        TRACEMSG("creating new unaligned block");
        Block *newBlock = CreateNewUnalignedBlockBetween(prevBlock, nextBlock);
        if (prevBlock)
            InsertBlockAfter(prevBlock, newBlock);
        else if (nextBlock)
            InsertBlockBefore(newBlock, nextBlock);
        else
            blocks.push_back(newBlock);
    }

    RemoveBlock(block);
    UpdateBlockMapAndColors();
    return true;
}

bool BlockMultipleAlignment::DeleteAllBlocks(void)
{
    if (blocks.size() == 0) return false;

    DELETE_ALL_AND_CLEAR(blocks, BlockList);
    InitCache();
    AddUnalignedBlocks();   // one single unaligned block for whole alignment
    UpdateBlockMapAndColors();
    return true;
}

bool BlockMultipleAlignment::DeleteRow(unsigned int row)
{
    if (row >= NRows()) {
        ERRORMSG("BlockMultipleAlignment::DeleteRow() - row out of range");
        return false;
    }

    // remove sequence from list
    vector < bool > removeRows(NRows(), false);
    removeRows[row] = true;
    VectorRemoveElements(*sequences, removeRows, 1);
    VectorRemoveElements(rowDoubles, removeRows, 1);
    VectorRemoveElements(rowStrings, removeRows, 1);
    VectorRemoveElements(geometryViolations, removeRows, 1);

    // delete row from all blocks, removing any zero-width blocks
    BlockList::iterator b = blocks.begin(), br, be = blocks.end();
    while (b != be) {
        (*b)->DeleteRow(row);
        if ((*b)->width == 0) {
            br = b;
            ++b;
            TRACEMSG("deleting block resized to zero width");
            RemoveBlock(*br);
        } else
            ++b;
    }

    // update total alignment width
    UpdateBlockMapAndColors();
    InitCache();

    return true;
}

void BlockMultipleAlignment::GetBlocks(ConstBlockList *cbl) const
{
    cbl->clear();
    cbl->reserve(blocks.size());
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b)
        cbl->push_back(*b);
}

unsigned int BlockMultipleAlignment::GetUngappedAlignedBlocks(UngappedAlignedBlockList *uabs) const
{
    uabs->clear();
    uabs->reserve(blocks.size());
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        UngappedAlignedBlock *uab = dynamic_cast<UngappedAlignedBlock*>(*b);
        if (uab) uabs->push_back(uab);
    }
    uabs->resize(uabs->size());
    return uabs->size();
}

bool BlockMultipleAlignment::ExtractRows(
    const vector < unsigned int >& dependentsToRemove, AlignmentList *pairwiseAlignments)
{
    if (dependentsToRemove.size() == 0) return false;

    // make a bool list of rows to remove, also checking to make sure dependent list items are in range
    unsigned int i;
    vector < bool > removeRows(NRows(), false);
    for (i=0; i<dependentsToRemove.size(); ++i) {
        if (dependentsToRemove[i] > 0 && dependentsToRemove[i] < NRows()) {
            removeRows[dependentsToRemove[i]] = true;
        } else {
            ERRORMSG("BlockMultipleAlignment::ExtractRows() - can't extract row "
                << dependentsToRemove[i]);
            return false;
        }
    }

    if (pairwiseAlignments) {
        TRACEMSG("creating new pairwise alignments");
        SetDiagPostLevel(eDiag_Warning);    // otherwise, info messages take a long time if lots of rows

        UngappedAlignedBlockList uaBlocks;
        GetUngappedAlignedBlocks(&uaBlocks);
        UngappedAlignedBlockList::const_iterator u, ue = uaBlocks.end();

        for (i=0; i<dependentsToRemove.size(); ++i) {

            // redraw molecule associated with removed row
            const Molecule *molecule = GetSequenceOfRow(dependentsToRemove[i])->molecule;
            if (molecule) GlobalMessenger()->PostRedrawMolecule(molecule);

            // create new pairwise alignment from each removed row
            SequenceList *newSeqs = new SequenceList(2);
            (*newSeqs)[0] = (*sequences)[0];
            (*newSeqs)[1] = (*sequences)[dependentsToRemove[i]];
            BlockMultipleAlignment *newAlignment = new BlockMultipleAlignment(newSeqs, alignmentManager);
            for (u=uaBlocks.begin(); u!=ue; ++u) {
                // only copy blocks that aren't flagged to be realigned
                if (markBlocks.find(*u) == markBlocks.end()) {
                    UngappedAlignedBlock *newABlock = new UngappedAlignedBlock(newAlignment);
                    const Block::Range *range = (*u)->GetRangeOfRow(0);
                    newABlock->SetRangeOfRow(0, range->from, range->to);
                    range = (*u)->GetRangeOfRow(dependentsToRemove[i]);
                    newABlock->SetRangeOfRow(1, range->from, range->to);
                    newABlock->width = range->to - range->from + 1;
                    newAlignment->AddAlignedBlockAtEnd(newABlock);
                }
            }
            if (!newAlignment->AddUnalignedBlocks() ||
                !newAlignment->UpdateBlockMapAndColors()) {
                ERRORMSG("BlockMultipleAlignment::ExtractRows() - error creating new alignment");
                return false;
            }

            // add aligned region info (for threader to use later on)
            if (uaBlocks.size() > 0) {
                int excess = 0;
                if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &excess))
                    WARNINGMSG("Can't get footprint excess residues from registry");
                newAlignment->alignDependentFrom =
                    uaBlocks.front()->GetRangeOfRow(dependentsToRemove[i])->from - excess;
                if (newAlignment->alignDependentFrom < 0)
                    newAlignment->alignDependentFrom = 0;
                newAlignment->alignDependentTo =
                    uaBlocks.back()->GetRangeOfRow(dependentsToRemove[i])->to + excess;
                if (newAlignment->alignDependentTo >= (int)((*newSeqs)[1]->Length()))
                    newAlignment->alignDependentTo = (*newSeqs)[1]->Length() - 1;
                TRACEMSG((*newSeqs)[1]->identifier->ToString() << " aligned from "
                    << newAlignment->alignDependentFrom << " to " << newAlignment->alignDependentTo);
            }

            pairwiseAlignments->push_back(newAlignment);
        }
        SetDiagPostLevel(eDiag_Info);
    }

    // remove sequences
    TRACEMSG("deleting sequences");
    VectorRemoveElements(*sequences, removeRows, dependentsToRemove.size());
    VectorRemoveElements(rowDoubles, removeRows, dependentsToRemove.size());
    VectorRemoveElements(rowStrings, removeRows, dependentsToRemove.size());
    VectorRemoveElements(geometryViolations, removeRows, dependentsToRemove.size());

    // delete row from all blocks, removing any zero-width blocks
    TRACEMSG("deleting alignment rows from blocks");
    BlockList::const_iterator b = blocks.begin(), br, be = blocks.end();
    while (b != be) {
        (*b)->DeleteRows(removeRows, dependentsToRemove.size());
        if ((*b)->width == 0) {
            br = b;
            ++b;
            TRACEMSG("deleting block resized to zero width");
            RemoveBlock(*br);
        } else
            ++b;
    }

    // update total alignment width
    UpdateBlockMapAndColors();
    InitCache();
    return true;
}

bool BlockMultipleAlignment::MergeAlignment(const BlockMultipleAlignment *newAlignment)
{
    // check to see if the alignment is compatible - must have same master sequence
    // and blocks of new alignment must contain blocks of this alignment; at same time,
    // build up map of aligned blocks in new alignment that correspond to aligned blocks
    // of this object, for convenient lookup later
    if (newAlignment->GetMaster() != GetMaster()) return false;

    const Block::Range *newRange, *thisRange;
    BlockList::const_iterator nb, nbe = newAlignment->blocks.end();
    BlockList::iterator b, be = blocks.end();
    typedef map < UngappedAlignedBlock *, const UngappedAlignedBlock * > AlignedBlockMap;
    AlignedBlockMap correspondingNewBlocks;

    for (b=blocks.begin(); b!=be; ++b) {
        if (!(*b)->IsAligned()) continue;
        for (nb=newAlignment->blocks.begin(); nb!=nbe; ++nb) {
            if (!(*nb)->IsAligned()) continue;

            newRange = (*nb)->GetRangeOfRow(0);
            thisRange = (*b)->GetRangeOfRow(0);
            if (newRange->from <= thisRange->from && newRange->to >= thisRange->to) {
                correspondingNewBlocks[dynamic_cast<UngappedAlignedBlock*>(*b)] =
                    dynamic_cast<const UngappedAlignedBlock*>(*nb);
                break;
            }
        }
        if (nb == nbe) return false;    // no corresponding block found
    }

    // add dependent sequences from new alignment; also copy other row-associated info
    unsigned int i, nNewRows = newAlignment->sequences->size() - 1;
    sequences->resize(sequences->size() + nNewRows);
    rowDoubles.resize(rowDoubles.size() + nNewRows);
    rowStrings.resize(rowStrings.size() + nNewRows);
    geometryViolations.resize(geometryViolations.size() + nNewRows);
    for (i=0; i<nNewRows; ++i) {
        unsigned int j = NRows() + i - nNewRows;
        (*sequences)[j] = (*(newAlignment->sequences))[i + 1];
        SetRowDouble(j, newAlignment->GetRowDouble(i + 1));
        SetRowStatusLine(j, newAlignment->GetRowStatusLine(i + 1));
        geometryViolations[j] = newAlignment->geometryViolations[i + 1];
    }

    // now that we know blocks are compatible, add new rows at end of this alignment, containing
    // all rows (except master) from new alignment; only that part of the aligned blocks from
    // the new alignment that intersect with the aligned blocks from this object are added, so
    // that this object's block structure is unchanged

    // resize all aligned blocks
    for (b=blocks.begin(); b!=be; ++b) (*b)->AddRows(nNewRows);

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
    for (b=blocks.begin(); b!=be; ) {
        if (!(*b)->IsAligned()) {
            BlockList::iterator bb(b);
            ++bb;
            delete *b;
            blocks.erase(b);
            b = bb;
        } else
            ++b;
    }
    InitCache();

    // update this alignment, but leave row scores/status alone
    if (!AddUnalignedBlocks() || !UpdateBlockMapAndColors(false)) {
        ERRORMSG("BlockMultipleAlignment::MergeAlignment() - internal update after merge failed");
        return false;
    }
    return true;
}

unsigned int BlockMultipleAlignment::ShowGeometryViolations(bool showGV)
{
    geometryViolations.clear();
    geometryViolations.resize(NRows());

    if (!showGV || !GetMaster()->molecule || GetMaster()->molecule->parentSet->isAlphaOnly) {
        showGeometryViolations = false;
        return 0;
    }

    Threader::GeometryViolationsForRow violations;
    unsigned int nViolations = alignmentManager->threader->GetGeometryViolations(this, &violations);
    if (violations.size() != NRows()) {
        ERRORMSG("BlockMultipleAlignment::ShowGeometryViolations() - wrong size list");
        showGeometryViolations = false;
        return 0;
    }

    for (unsigned int row=0; row<NRows(); ++row) {
        geometryViolations[row].resize(GetSequenceOfRow(row)->Length(), false);
        Threader::IntervalList::const_iterator i, ie = violations[row].end();
        for (i=violations[row].begin(); i!=ie; ++i)
            for (unsigned int l=i->first; l<=i->second; ++l)
                geometryViolations[row][l] = true;
    }

    showGeometryViolations = true;
    return nViolations;
}

CSeq_align * CreatePairwiseSeqAlignFromMultipleRow(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment::UngappedAlignedBlockList& blocks, unsigned int dependentRow)
{
    if (!multiple || dependentRow >= multiple->NRows()) {
        ERRORMSG("CreatePairwiseSeqAlignFromMultipleRow() - bad parameters");
        return NULL;
    }

    CSeq_align *seqAlign = new CSeq_align();
    seqAlign->SetType(CSeq_align::eType_partial);
    seqAlign->SetDim(2);

    CSeq_align::C_Segs::TDendiag& denDiags = seqAlign->SetSegs().SetDendiag();
    denDiags.resize((blocks.size() > 0) ? blocks.size() : 1);

    CSeq_align::C_Segs::TDendiag::iterator d, de = denDiags.end();
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b = blocks.begin();
    const Block::Range *range;
    for (d=denDiags.begin(); d!=de; ++d, ++b) {

        CDense_diag *denDiag = new CDense_diag();
        d->Reset(denDiag);
        denDiag->SetDim(2);
        denDiag->SetIds().resize(2);

        // master row
        denDiag->SetIds().front().Reset(multiple->GetSequenceOfRow(0)->CreateSeqId());
        if (blocks.size() > 0) {
            range = (*b)->GetRangeOfRow(0);
            denDiag->SetStarts().push_back(range->from);
        } else
            denDiag->SetStarts().push_back(0);

        // dependent row
        denDiag->SetIds().back().Reset(multiple->GetSequenceOfRow(dependentRow)->CreateSeqId());
        if (blocks.size() > 0) {
            range = (*b)->GetRangeOfRow(dependentRow);
            denDiag->SetStarts().push_back(range->from);
        } else
            denDiag->SetStarts().push_back(0);

        // block width
        denDiag->SetLen((blocks.size() > 0) ? (*b)->width : 0);
    }

    return seqAlign;
}

bool BlockMultipleAlignment::MarkBlock(unsigned int column)
{
    if (column < blockMap.size() && blockMap[column].block->IsAligned()) {
        TRACEMSG("marked block for realignment");
        markBlocks[blockMap[column].block] = true;
        return true;
    }
    return false;
}

bool BlockMultipleAlignment::ClearMarks(void)
{
    if (markBlocks.size() == 0) return false;
    markBlocks.clear();
    return true;
}

void BlockMultipleAlignment::GetMarkedBlockNumbers(std::vector < unsigned int > *alignedBlockNumbers) const
{
    alignedBlockNumbers->resize(markBlocks.size());
    unsigned int a = 0, i = 0;
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        if ((*b)->IsAligned()) {
            if (markBlocks.find(*b) != markBlocks.end())
                (*alignedBlockNumbers)[i++] = a;
            ++a;
        }
    }
}

bool BlockMultipleAlignment::HighlightAlignedColumnsOfMasterRange(unsigned int from, unsigned int to) const
{
    const Sequence *master = GetMaster();

    // must do one column at a time, rather than range, in case there are inserts wrt the master
    bool anyError = false;
    for (unsigned int i=from; i<=to; ++i) {

        // sanity check
        if (i >= master->Length() || !IsAligned(0U, i)) {
            WARNINGMSG("Can't highlight alignment at master residue " << (i+1));
            anyError = true;
            // highlight unaligned residues, but master only
            if (i < master->Length())
                GlobalMessenger()->AddHighlights(GetSequenceOfRow(0), i, i);
            continue;
        }

        // get block and offset
        const Block *block = GetBlock(0, i);
        unsigned int blockOffset = i - block->GetRangeOfRow(0)->from;

        // highlight aligned residue in each row
        for (unsigned int row=0; row<NRows(); ++row) {
            unsigned int dependentIndex = block->GetRangeOfRow(row)->from + blockOffset;
            GlobalMessenger()->AddHighlights(GetSequenceOfRow(row), dependentIndex, dependentIndex);
        }
    }

    return !anyError;
}

unsigned int BlockMultipleAlignment::NAlignedBlocks(void) const
{
    unsigned int n = 0;
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) if ((*b)->IsAligned()) ++n;
    return n;
}

unsigned int BlockMultipleAlignment::SumOfAlignedBlockWidths(void) const
{
    unsigned int w = 0;
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b)
        if ((*b)->IsAligned())
            w += (*b)->width;
    return w;
}

double BlockMultipleAlignment::GetRelativeAlignmentFraction(unsigned int alignmentIndex) const
{
    if (!blockMap[alignmentIndex].block->IsAligned())
        return -1.0;

    unsigned int f = 0;
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        if ((*b)->IsAligned()) {
            if (*b == blockMap[alignmentIndex].block) {
                f += blockMap[alignmentIndex].blockColumn;
                return (((double) f) / (SumOfAlignedBlockWidths() - 1));
            } else {
                f += (*b)->width;
            }
        }
    }
    return -1.0;    // should never get here
}

bool BlockMultipleAlignment::HasNoAlignedBlocks(void) const
{
    return (blocks.size() == 0 || (blocks.size() == 1 && !blocks.front()->IsAligned()));
}

int BlockMultipleAlignment::GetAlignmentIndex(unsigned int row, unsigned int seqIndex, eUnalignedJustification justification) const
{
    if (row >= NRows() || seqIndex >= GetSequenceOfRow(row)->Length()) {
        ERRORMSG("BlockMultipleAlignment::GetAlignmentIndex() - coordinate out of range");
        return -1;
    }

    unsigned int alignmentIndex, blockColumn;
    const Block *block = NULL;
    const Block::Range *range;

    for (alignmentIndex=0; alignmentIndex<totalWidth; ++alignmentIndex) {

        // check each block to see if index is in range
        if (block != blockMap[alignmentIndex].block) {
            block = blockMap[alignmentIndex].block;

            range = block->GetRangeOfRow(row);
            if ((int)seqIndex >= range->from && (int)seqIndex <= range->to) {

                // override requested justification for end blocks
                if (block == blocks.back()) // also true if there's a single aligned block
                    justification = eLeft;
                else if (block == blocks.front())
                    justification = eRight;

                // search block columns to find index (inefficient, but avoids having to write
                // inverse functions of Block::GetIndexAt()
                for (blockColumn=0; blockColumn<block->width; ++blockColumn) {
                    if ((int)seqIndex == block->GetIndexAt(blockColumn, row, justification))
                        return alignmentIndex + blockColumn;
                }
                ERRORMSG("BlockMultipleAlignment::GetAlignmentIndex() - can't find index in block");
                return -1;
            }
        }
    }

    // should never get here...
    ERRORMSG("BlockMultipleAlignment::GetAlignmentIndex() - confused");
    return -1;
}

// returns the loop length of the unaligned region at the given row and alignment index; -1 if that position is aligned
int BlockMultipleAlignment::GetLoopLength(unsigned int row, unsigned int alignmentIndex)
{
    UnalignedBlock *block = dynamic_cast<UnalignedBlock*>(blockMap[alignmentIndex].block);
    if (!block)
        return -1;

    const Block::Range *range = block->GetRangeOfRow(row);
    return (range->to - range->from + 1);
}


///// UngappedAlignedBlock methods /////

char UngappedAlignedBlock::GetCharacterAt(unsigned int blockColumn, unsigned int row) const
{
    return (*(parentAlignment->GetSequences()))[row]->sequenceString[GetIndexAt(blockColumn, row)];
}

Block * UngappedAlignedBlock::Clone(const BlockMultipleAlignment *newMultiple) const
{
    UngappedAlignedBlock *copy = new UngappedAlignedBlock(newMultiple);
    const Block::Range *range;
    for (unsigned int row=0; row<NSequences(); ++row) {
        range = GetRangeOfRow(row);
        copy->SetRangeOfRow(row, range->from, range->to);
    }
    copy->width = width;
    return copy;
}

void UngappedAlignedBlock::DeleteRow(unsigned int row)
{
    RangeList::iterator r = ranges.begin();
    for (unsigned int i=0; i<row; ++i) ++r;
    ranges.erase(r);
}

void UngappedAlignedBlock::DeleteRows(vector < bool >& removeRows, unsigned int nToRemove)
{
    VectorRemoveElements(ranges, removeRows, nToRemove);
}


///// UnalignedBlock methods /////

int UnalignedBlock::GetIndexAt(unsigned int blockColumn, unsigned int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const
{
    const Block::Range *range = GetRangeOfRow(row);
    int seqIndex = -1;
    unsigned int rangeWidth, rangeMiddle, extraSpace;

    switch (justification) {
        case BlockMultipleAlignment::eLeft:
            seqIndex = range->from + blockColumn;
            break;
        case BlockMultipleAlignment::eRight:
            seqIndex = range->to - width + blockColumn + 1;
            break;
        case BlockMultipleAlignment::eCenter:
            rangeWidth = (range->to - range->from + 1);
            extraSpace = (width - rangeWidth) / 2;
            if (blockColumn < extraSpace || blockColumn >= extraSpace + rangeWidth)
                seqIndex = -1;
            else
                seqIndex = range->from + blockColumn - extraSpace;
            break;
        case BlockMultipleAlignment::eSplit:
            rangeWidth = (range->to - range->from + 1);
            rangeMiddle = (rangeWidth / 2) + (rangeWidth % 2);
            extraSpace = width - rangeWidth;
            if (blockColumn < rangeMiddle)
                seqIndex = range->from + blockColumn;
            else if (blockColumn >= extraSpace + rangeMiddle)
                seqIndex = range->to - width + blockColumn + 1;
            else
                seqIndex = -1;
            break;
    }
    if (seqIndex < range->from || seqIndex > range->to) seqIndex = -1;

    return seqIndex;
}

void UnalignedBlock::Resize(void)
{
    width = 0;
    for (unsigned int i=0; i<NSequences(); ++i) {
        unsigned int blockWidth = ranges[i].to - ranges[i].from + 1;
        if (blockWidth > width) width = blockWidth;
    }
}

void UnalignedBlock::DeleteRow(unsigned int row)
{
    RangeList::iterator r = ranges.begin();
    for (unsigned int i=0; i<row; ++i) ++r;
    ranges.erase(r);
    Resize();
}

void UnalignedBlock::DeleteRows(vector < bool >& removeRows, unsigned int nToRemove)
{
    VectorRemoveElements(ranges, removeRows, nToRemove);
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
    copy->width = width;
    return copy;
}

unsigned int UnalignedBlock::MinResidues(void) const
{
    int min = 0, m;
    const Block::Range *range;
    for (unsigned int row=0; row<NSequences(); ++row) {
        range = GetRangeOfRow(row);
        m = range->to - range->from + 1;
        if (row == 0 || m < min)
            min = m;
    }
    return min;
}

END_SCOPE(Cn3D)
