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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqalign/Dense_diag.hpp>

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
#include "cn3d_blast.hpp"

// hack so I can catch memory leaks specific to this module, at the line where allocation occurs
#ifdef _DEBUG
#ifdef MemNew
#undef MemNew
#endif
#define MemNew(sz) memset(malloc(sz), 0, (sz))
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

BlockMultipleAlignment::BlockMultipleAlignment(SequenceList *sequenceList, AlignmentManager *alnMgr) :
    sequences(sequenceList), conservationColorer(NULL), alignmentManager(alnMgr),
    alignMasterFrom(-1), alignMasterTo(-1), alignSlaveFrom(-1), alignSlaveTo(-1),
    pssm(NULL), showGeometryViolations(false)
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
    cachePrevRow = -1;
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
    copy->alignSlaveFrom = alignSlaveFrom;
    copy->alignSlaveTo = alignSlaveTo;
    return copy;
}

static Int4 Round(double d)
{
    if (d >= 0.0)
        return (Int4) (d + 0.5);
    else
        return (Int4) (d - 0.5);
}

const BLAST_Matrix * BlockMultipleAlignment::GetPSSM(void) const
{
    if (pssm) return pssm;

    // for now, use threader's SeqMtf
    BLAST_KarlinBlkPtr karlinBlock = BlastKarlinBlkCreate();
    Seq_Mtf *seqMtf = Threader::CreateSeqMtf(this, 1.0, karlinBlock);
    if (!seqMtf) return NULL;

    pssm = (BLAST_Matrix *) MemNew(sizeof(BLAST_Matrix));
    pssm->is_prot = TRUE;
    pssm->name = StringSave("BLOSUM62");
    pssm->karlinK = karlinBlock->K;
    pssm->rows = seqMtf->n + 1;
    pssm->columns = 26;

#ifdef PRINT_PSSM
    FILE *f = fopen("blast_matrix.txt", "w");
#endif

    int i, j;
    pssm->matrix = (Int4 **) MemNew(pssm->rows * sizeof(Int4 *));
    for (i=0; i<pssm->rows; ++i) {
        pssm->matrix[i] = (Int4 *) MemNew(pssm->columns * sizeof(Int4));
#ifdef PRINT_PSSM
        fprintf(f, "matrix %i : ", i);
#endif

        // set scores from threader matrix
        if (i < seqMtf->n) {
            // initialize all rows with custom score, or BLAST_SCORE_MIN; to match what Aron's function creates
            for (j=0; j<pssm->columns; ++j)
                pssm->matrix[i][j] = (j == 21 ? -1 : (j == 25 ? -4 : BLAST_SCORE_MIN));

            for (j=0; j<seqMtf->AlphabetSize; ++j) {
                pssm->matrix[i][LookupBLASTResidueNumberFromThreaderResidueNumber(j)] =
                    Round(((double) seqMtf->ww[i][j]) / Threader::SCALING_FACTOR);
            }
        } else {
            // initialize last row with BLAST_SCORE_MIN
            for (j=0; j<pssm->columns; ++j)
                pssm->matrix[i][j] = BLAST_SCORE_MIN;
        }
#ifdef PRINT_PSSM
        for (j=0; j<pssm->columns; ++j)
            fprintf(f, "%i ", pssm->matrix[i][j]);
        fprintf(f, "\n");
#endif
    }

#ifdef PRINT_PSSM
    // for diffing with scoremat stored in ascii CD
    fprintf(f, "{\n");
    for (i=0; i<seqMtf->n; ++i) {
        for (j=0; j<pssm->columns; ++j) {
            fprintf(f, "      %i,\n", pssm->matrix[i][j]);
        }
    }
    fprintf(f, "}\n");
    // for diffing with .mtx file
    for (i=0; i<seqMtf->n; ++i) {
        for (j=0; j<pssm->columns; ++j) {
            fprintf(f, "%i  ", pssm->matrix[i][j]);
        }
        fprintf(f, "\n");
    }
#endif

#ifdef _DEBUG
    pssm->posFreqs = (Nlm_FloatHi **) MemNew(pssm->rows * sizeof(Nlm_FloatHi *));
    for (i=0; i<pssm->rows; ++i) {
        pssm->posFreqs[i] = (Nlm_FloatHi *) MemNew(pssm->columns * sizeof(Nlm_FloatHi));
#ifdef PRINT_PSSM
        fprintf(f, "freqs %i : ", i);
#endif
        if (i < seqMtf->n) {
            for (j=0; j<seqMtf->AlphabetSize; ++j) {
                pssm->posFreqs[i][LookupBLASTResidueNumberFromThreaderResidueNumber(j)] =
                    seqMtf->freqs[i][j] / Threader::SCALING_FACTOR;
            }
        }
#ifdef PRINT_PSSM
        for (j=0; j<pssm->columns; ++j)
            fprintf(f, "%g ", pssm->posFreqs[i][j]);
        fprintf(f, "\n");
#endif
    }

    // we're not actually using the frequency table; just printing it out for debugging/testing
    for (i=0; i<pssm->rows; ++i) MemFree(pssm->posFreqs[i]);
    MemFree(pssm->posFreqs);
#endif // _DEBUG

    pssm->posFreqs = NULL;

#ifdef PRINT_PSSM
    fclose(f);
#endif

    FreeSeqMtf(seqMtf);
    BlastKarlinBlkDestruct(karlinBlock);
    return pssm;
}

void BlockMultipleAlignment::RemovePSSM(void) const
{
    if (pssm) {
        BLAST_MatrixDestruct(pssm);
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
        ERRORMSG("MultipleAlignment::CheckAlignedBlock() - checks aligned blocks only");
        return false;
    }
    if (block->NSequences() != sequences->size()) {
        ERRORMSG("MultipleAlignment::CheckAlignedBlock() - block size mismatch");
        return false;
    }

    // make sure ranges are reasonable for each sequence
    int row;
    const Block
        *prevBlock = GetBlockBefore(block),
        *nextBlock = GetBlockAfter(block);
    const Block::Range *range, *prevRange = NULL, *nextRange = NULL;
    SequenceList::const_iterator sequence = sequences->begin();
    for (row=0; row<block->NSequences(); ++row, ++sequence) {
        range = block->GetRangeOfRow(row);
        if (prevBlock) prevRange = prevBlock->GetRangeOfRow(row);
        if (nextBlock) nextRange = nextBlock->GetRangeOfRow(row);
        if (range->to - range->from + 1 != block->width ||       // check block width
            (prevRange && range->from <= prevRange->to) ||          // check for range overlap
            (nextRange && range->to >= nextRange->from) ||          // check for range overlap
            range->from > range->to ||                              // check range values
            range->to >= (*sequence)->Length()) {                   // check bounds of end
            ERRORMSG("MultipleAlignment::CheckAlignedBlock() - range error");
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
    if ((leftBlock && !leftBlock->IsAligned()) ||
        (rightBlock && !rightBlock->IsAligned())) {
        ERRORMSG("CreateNewUnalignedBlockBetween() - passed an unaligned block");
        return NULL;
    }

    int row, from, to, length;
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

        length = to - from + 1;
        if (length < 0) { // just to make sure...
            ERRORMSG("CreateNewUnalignedBlockBetween() - unaligned length < 0");
            return NULL;
        }
        if (length > newBlock->width) newBlock->width = length;
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
    if (newUnalignedBlock) {
        blocks.insert(a, newUnalignedBlock);
    }

    return true;
}

bool BlockMultipleAlignment::UpdateBlockMapAndColors(bool clearRowInfo)
{
    int i = 0, j, n = 0;
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
            blockMap[i].alignedBlockNum = aBlock ? n : -1;
        }
    }

    // if alignment changes, any pssm/scores/status/special colors become invalid
    RemovePSSM();
    if (clearRowInfo) ClearRowInfo();
    ShowGeometryViolations(showGeometryViolations); // recalculate GV's

    return true;
}

bool BlockMultipleAlignment::GetCharacterTraitsAt(
    int alignmentColumn, int row, eUnalignedJustification justification,
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
        *character = toupper(*character);
    else
        *character = tolower(*character);

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
            double hydrophobicity = GetHydrophobicity(toupper(*character));
            *color = (hydrophobicity != UNKNOWN_HYDROPHOBICITY) ?
                GlobalColors()->Get(Colors::eHydrophobicityMap, hydrophobicity) :
                GlobalColors()->Get(Colors::eNoHydrophobicity);
        }
        // or color by charge
        else if (sequence->isProtein && colorScheme == StyleSettings::eCharge) {
            int charge = GetCharge(toupper(*character));
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
                    int masterIndex = masterRange->from + seqIndex - range->from;
                    const Block *multipleBlock = referenceAlignment->GetBlock(0, masterIndex);
                    masterRange = multipleBlock->GetRangeOfRow(0);

                    if (multipleBlock->IsAligned() &&
                        ((isLeftEdge && masterIndex > masterRange->from) ||
                         (isRightEdge && masterIndex < masterRange->to)))
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
    int alignmentColumn, int row, eUnalignedJustification requestedJustification,
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

    if (cachePrevRow < 0 || cachePrevRow >= NRows() || sequence != (*sequences)[cachePrevRow]) {
        int row;
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
    int row, int seqIndex, StyleSettings::eColorScheme colorScheme) const
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
    const Sequence *sequence, int seqIndex, StyleSettings::eColorScheme colorScheme) const
{
    int row = GetRowForSequence(sequence);
    if (row < 0) return NULL;
    return GetAlignmentColor(row, seqIndex, colorScheme);
}

bool BlockMultipleAlignment::IsAligned(int row, int seqIndex) const
{
    const Block *block = GetBlock(row, seqIndex);
    return (block && block->IsAligned());
}

const Block * BlockMultipleAlignment::GetBlock(int row, int seqIndex) const
{
    // make sure we're in range for this sequence
    if (row < 0 || seqIndex < 0 || row >= NRows() || seqIndex >= (*sequences)[row]->Length()) {
        ERRORMSG("BlockMultipleAlignment::GetBlock() - coordinate out of range");
        return NULL;
    }

    const Block::Range *range;

    // first check to see if it's in the same block as last time.
    if (cachePrevBlock) {
        range = cachePrevBlock->GetRangeOfRow(row);
        if (seqIndex >= range->from && seqIndex <= range->to) return cachePrevBlock;
        ++cacheBlockIterator; // start search at next block
    } else {
        cacheBlockIterator = blocks.begin();
    }

    // otherwise, perform block search. This search is most efficient when queries
    // happen in order from left to right along a given row.
    do {
        if (cacheBlockIterator == blocks.end()) cacheBlockIterator = blocks.begin();
        range = (*cacheBlockIterator)->GetRangeOfRow(row);
        if (seqIndex >= range->from && seqIndex <= range->to) {
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

int BlockMultipleAlignment::GetAlignedSlaveIndex(int masterSeqIndex, int slaveRow) const
{
    const UngappedAlignedBlock
        *aBlock = dynamic_cast<const UngappedAlignedBlock*>(GetBlock(0, masterSeqIndex));
    if (!aBlock) return -1;

    const Block::Range
        *masterRange = aBlock->GetRangeOfRow(0),
        *slaveRange = aBlock->GetRangeOfRow(slaveRow);
    return (slaveRange->from + masterSeqIndex - masterRange->from);
}

void BlockMultipleAlignment::SelectedRange(int row, int alnIndexFrom, int alnIndexTo,
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

void BlockMultipleAlignment::GetAlignedBlockPosition(int alignmentIndex,
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

bool BlockMultipleAlignment::MoveBlockBoundary(int columnFrom, int columnTo)
{
    int blockColumn, blockWidth;
    GetAlignedBlockPosition(columnFrom, &blockColumn, &blockWidth);
    if (blockColumn < 0 || blockWidth <= 0) return false;

    TRACEMSG("trying to move block boundary from " << columnFrom << " to " << columnTo);

    const BlockInfo& info = blockMap[columnFrom];
    int row, requestedShift = columnTo - columnFrom, actualShift = 0;
    const Block::Range *range;

    // shrink block from left
    if (blockColumn == 0 && requestedShift > 0 && requestedShift < info.block->width) {
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
    else if (blockColumn == info.block->width - 1 &&
                requestedShift < 0 && requestedShift > -info.block->width) {
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
    else if (blockColumn == info.block->width - 1 && requestedShift > 0) {
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

bool BlockMultipleAlignment::ShiftRow(int row, int fromAlignmentIndex, int toAlignmentIndex,
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
        for (int i=0; i<NRows(); ++i) {
            prevRange = prevUABlock->GetRangeOfRow(i);
            width = prevRange->to - prevRange->from + 1;
            if (width < 0) ERRORMSG("BlockMultipleAlignment::ShiftRow() - negative width on left");
            if (width > prevUABlock->width) prevUABlock->width = width;
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
        for (int i=0; i<NRows(); ++i) {
            nextRange = nextUABlock->GetRangeOfRow(i);
            width = nextRange->to - nextRange->from + 1;
            if (width < 0) ERRORMSG("BlockMultipleAlignment::ShiftRow() - negative width on right");
            if (width > nextUABlock->width) nextUABlock->width = width;
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

bool BlockMultipleAlignment::ZipAlignResidue(int row, int alignmentIndex, bool moveRight, eUnalignedJustification justification)
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

bool BlockMultipleAlignment::OptimizeBlock(int row, int alignmentIndex, eUnalignedJustification justification)
{
    // is this an aligned block?
    UngappedAlignedBlock *block = dynamic_cast<UngappedAlignedBlock*>(blockMap[alignmentIndex].block);
    if (!block) {
        TRACEMSG("block is unaligned");
        return false;
    }

    // see if there's any room to shift
    const UnalignedBlock *prevBlock = GetUnalignedBlockBefore(block), *nextBlock = GetUnalignedBlockAfter(block);
    int maxShiftRight = 0, maxShiftLeft = 0;
    const Block::Range *range;
    if (prevBlock) {
        range = prevBlock->GetRangeOfRow(row);
        maxShiftRight = range->to - range->from + 1;
    }
    if (nextBlock) {
        range = nextBlock->GetRangeOfRow(row);
        maxShiftLeft = range->to - range->from + 1;
    }
    TRACEMSG("maxShiftRight " << maxShiftRight << ", maxShiftLeft " << maxShiftLeft);
    if (maxShiftRight == 0 && maxShiftLeft == 0) {
        TRACEMSG("no room to shift this block");
        return false;
    }

    // scan all possible block positions, find max information content
    range = block->GetRangeOfRow(row);
    int bestSeqIndexStart = range->from;
    float score, maxScore = kMin_Float;
    for (int seqIndexStart = range->from - maxShiftRight; seqIndexStart <= range->from + maxShiftLeft; ++seqIndexStart) {

        // calculate block's info content given each position of this row
        score = 0.0f;
        for (int blockColumn=0; blockColumn<block->width; ++blockColumn) {

            // create profile for this column
            typedef std::map < char, int > ColumnProfile;
            ColumnProfile profile;
            ColumnProfile::iterator p, pe;
            for (int r=0; r<NRows(); ++r) {
                int seqIndex = (r == row) ? (seqIndexStart + blockColumn) : (block->GetRangeOfRow(r)->from + blockColumn);
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
    bool moved = (bestSeqIndexStart != range->from);
    if (moved) {
        if (bestSeqIndexStart < range->from)
            TRACEMSG("shifting block right by " << (range->from - bestSeqIndexStart));
        else
            TRACEMSG("shifting block left by " << (bestSeqIndexStart - range->from));
        alignmentIndex = GetAlignmentIndex(row, range->from, justification);
        int alnIdx2 = GetAlignmentIndex(row, bestSeqIndexStart, justification);
        moved = ShiftRow(row, alnIdx2, alignmentIndex, justification);
        if (!moved)
            ERRORMSG("ShiftRow() failed!");
    } else
        TRACEMSG("block was not moved");

    return moved;
}

bool BlockMultipleAlignment::SplitBlock(int alignmentIndex)
{
    const BlockInfo& info = blockMap[alignmentIndex];
    if (!info.block->IsAligned() || info.block->width < 2 || info.blockColumn == 0)
        return false;

    TRACEMSG("splitting block");
    UngappedAlignedBlock *newAlignedBlock = new UngappedAlignedBlock(this);
    newAlignedBlock->width = info.block->width - info.blockColumn;
    info.block->width = info.blockColumn;

    const Block::Range *range;
    int oldTo;
    for (int row=0; row<NRows(); ++row) {
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

bool BlockMultipleAlignment::MergeBlocks(int fromAlignmentIndex, int toAlignmentIndex)
{
    Block
        *expandedBlock = blockMap[fromAlignmentIndex].block,
        *lastBlock = blockMap[toAlignmentIndex].block;
    if (expandedBlock == lastBlock) return false;
    int i;
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

bool BlockMultipleAlignment::CreateBlock(int fromAlignmentIndex, int toAlignmentIndex,
    eUnalignedJustification justification)
{
    const BlockInfo& info = blockMap[fromAlignmentIndex];
    UnalignedBlock *prevUABlock = dynamic_cast<UnalignedBlock*>(info.block);
    if (!prevUABlock || info.block != blockMap[toAlignmentIndex].block) return false;
    int row, seqIndexFrom, seqIndexTo,
        newBlockWidth = toAlignmentIndex - fromAlignmentIndex + 1,
        origWidth = prevUABlock->width;
    vector < int > seqIndexesFrom(NRows()), seqIndexesTo(NRows());
    const Sequence *seq;
	bool ignored;
    for (row=0; row<NRows(); ++row) {
        if (!GetSequenceAndIndexAt(fromAlignmentIndex, row, justification, &seq, &seqIndexFrom, &ignored) ||
            !GetSequenceAndIndexAt(toAlignmentIndex, row, justification, &seq, &seqIndexTo, &ignored) ||
            seqIndexFrom < 0 || seqIndexTo < 0 ||
            seqIndexTo - seqIndexFrom + 1 != newBlockWidth) return false;
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
            if (rangeWidth > nextUABlock->width) nextUABlock->width = rangeWidth;
            deleteNextUABlock = false;
        }

        prevUABlock->SetRangeOfRow(row, prevRange->from, seqIndexesFrom[row] - 1);
        rangeWidth = seqIndexesFrom[row] - prevRange->from;
        if (rangeWidth < 0)
            ERRORMSG("BlockMultipleAlignment::CreateBlock() - negative prevRange width");
        else if (rangeWidth > 0) {
            if (rangeWidth > prevUABlock->width) prevUABlock->width = rangeWidth;
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

bool BlockMultipleAlignment::DeleteBlock(int alignmentIndex)
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
        int maxWidth = 0, width;
        for (int row=0; row<NRows(); ++row) {
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
        for (int row=0; row<NRows(); ++row) {
            prevRange = prevBlock->GetRangeOfRow(row);
            range = block->GetRangeOfRow(row);
            prevBlock->SetRangeOfRow(row, prevRange->from, range->to);
        }
        prevBlock->width += block->width;
    }

    // unaligned block on right only
    else if (nextBlock && !nextBlock->IsAligned()) {
        const Block::Range *range, *nextRange;
        for (int row=0; row<NRows(); ++row) {
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

bool BlockMultipleAlignment::DeleteRow(int row)
{
    if (row < 0 || row >= NRows()) {
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

void BlockMultipleAlignment::GetUngappedAlignedBlocks(UngappedAlignedBlockList *uabs) const
{
    uabs->clear();
    uabs->reserve(blocks.size());
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        UngappedAlignedBlock *uab = dynamic_cast<UngappedAlignedBlock*>(*b);
        if (uab) uabs->push_back(uab);
    }
    uabs->resize(uabs->size());
}

bool BlockMultipleAlignment::ExtractRows(
    const vector < int >& slavesToRemove, AlignmentList *pairwiseAlignments)
{
    if (slavesToRemove.size() == 0) return false;

    // make a bool list of rows to remove, also checking to make sure slave list items are in range
    int i;
    vector < bool > removeRows(NRows(), false);
    for (i=0; i<slavesToRemove.size(); ++i) {
        if (slavesToRemove[i] > 0 && slavesToRemove[i] < NRows()) {
            removeRows[slavesToRemove[i]] = true;
        } else {
            ERRORMSG("BlockMultipleAlignment::ExtractRows() - can't extract row "
                << slavesToRemove[i]);
            return false;
        }
    }

    if (pairwiseAlignments) {
        TRACEMSG("creating new pairwise alignments");
        SetDiagPostLevel(eDiag_Warning);    // otherwise, info messages take a long time if lots of rows

        UngappedAlignedBlockList uaBlocks;
        GetUngappedAlignedBlocks(&uaBlocks);
        UngappedAlignedBlockList::const_iterator u, ue = uaBlocks.end();

        for (i=0; i<slavesToRemove.size(); ++i) {

            // redraw molecule associated with removed row
            const Molecule *molecule = GetSequenceOfRow(slavesToRemove[i])->molecule;
            if (molecule) GlobalMessenger()->PostRedrawMolecule(molecule);

            // create new pairwise alignment from each removed row
            SequenceList *newSeqs = new SequenceList(2);
            (*newSeqs)[0] = (*sequences)[0];
            (*newSeqs)[1] = (*sequences)[slavesToRemove[i]];
            BlockMultipleAlignment *newAlignment = new BlockMultipleAlignment(newSeqs, alignmentManager);
            for (u=uaBlocks.begin(); u!=ue; ++u) {
                // only copy blocks that aren't flagged to be realigned
                if (markBlocks.find(*u) == markBlocks.end()) {
                    UngappedAlignedBlock *newABlock = new UngappedAlignedBlock(newAlignment);
                    const Block::Range *range = (*u)->GetRangeOfRow(0);
                    newABlock->SetRangeOfRow(0, range->from, range->to);
                    range = (*u)->GetRangeOfRow(slavesToRemove[i]);
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
                newAlignment->alignSlaveFrom =
                    uaBlocks.front()->GetRangeOfRow(slavesToRemove[i])->from - excess;
                if (newAlignment->alignSlaveFrom < 0)
                    newAlignment->alignSlaveFrom = 0;
                newAlignment->alignSlaveTo =
                    uaBlocks.back()->GetRangeOfRow(slavesToRemove[i])->to + excess;
                if (newAlignment->alignSlaveTo >= (*newSeqs)[1]->Length())
                    newAlignment->alignSlaveTo = (*newSeqs)[1]->Length() - 1;
                TRACEMSG((*newSeqs)[1]->identifier->ToString() << " aligned from "
                    << newAlignment->alignSlaveFrom << " to " << newAlignment->alignSlaveTo);
            }

            pairwiseAlignments->push_back(newAlignment);
        }
        SetDiagPostLevel(eDiag_Info);
    }

    // remove sequences
    TRACEMSG("deleting sequences");
    VectorRemoveElements(*sequences, removeRows, slavesToRemove.size());
    VectorRemoveElements(rowDoubles, removeRows, slavesToRemove.size());
    VectorRemoveElements(rowStrings, removeRows, slavesToRemove.size());
    VectorRemoveElements(geometryViolations, removeRows, slavesToRemove.size());

    // delete row from all blocks, removing any zero-width blocks
    TRACEMSG("deleting alignment rows from blocks");
    BlockList::const_iterator b = blocks.begin(), br, be = blocks.end();
    while (b != be) {
        (*b)->DeleteRows(removeRows, slavesToRemove.size());
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

    // add slave sequences from new alignment; also copy other row-associated info
    int i, nNewRows = newAlignment->sequences->size() - 1;
    sequences->resize(sequences->size() + nNewRows);
    rowDoubles.resize(rowDoubles.size() + nNewRows);
    rowStrings.resize(rowStrings.size() + nNewRows);
    geometryViolations.resize(geometryViolations.size() + nNewRows);
    for (i=0; i<nNewRows; ++i) {
        int j = NRows() + i - nNewRows;
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

int BlockMultipleAlignment::ShowGeometryViolations(bool showGV)
{
    geometryViolations.clear();
    geometryViolations.resize(NRows());

    if (!showGV || !GetMaster()->molecule || GetMaster()->molecule->parentSet->isAlphaOnly) {
        showGeometryViolations = false;
        return 0;
    }

    Threader::GeometryViolationsForRow violations;
    int nViolations = alignmentManager->threader->GetGeometryViolations(this, &violations);
    if (violations.size() != NRows()) {
        ERRORMSG("BlockMultipleAlignment::ShowGeometryViolations() - wrong size list");
        showGeometryViolations = false;
        return 0;
    }

    for (int row=0; row<NRows(); ++row) {
        geometryViolations[row].resize(GetSequenceOfRow(row)->Length(), false);
        Threader::IntervalList::const_iterator i, ie = violations[row].end();
        for (i=violations[row].begin(); i!=ie; ++i)
            for (int l=i->first; l<=i->second; ++l)
                geometryViolations[row][l] = true;
    }

    showGeometryViolations = true;
    return nViolations;
}

CSeq_align * CreatePairwiseSeqAlignFromMultipleRow(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment::UngappedAlignedBlockList& blocks, int slaveRow)
{
    if (!multiple || slaveRow < 0 || slaveRow >= multiple->NRows()) {
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

        // slave row
        denDiag->SetIds().back().Reset(multiple->GetSequenceOfRow(slaveRow)->CreateSeqId());
        if (blocks.size() > 0) {
            range = (*b)->GetRangeOfRow(slaveRow);
            denDiag->SetStarts().push_back(range->from);
        } else
            denDiag->SetStarts().push_back(0);

        // block width
        denDiag->SetLen((blocks.size() > 0) ? (*b)->width : 0);
    }

    return seqAlign;
}

bool BlockMultipleAlignment::MarkBlock(int column)
{
    if (column >= 0 && column < blockMap.size() && blockMap[column].block->IsAligned()) {
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

bool BlockMultipleAlignment::HighlightAlignedColumnsOfMasterRange(int from, int to) const
{
    const Sequence *master = GetMaster();

    // must do one column at a time, rather than range, in case there are inserts wrt the master
    bool anyError = false;
    for (int i=from; i<=to; ++i) {

        // sanity check
        if (i < 0 || i >= master->Length() || !IsAligned(0, i)) {
            WARNINGMSG("Can't highlight alignment at master residue " << (i+1));
            anyError = true;
            // highlight unaligned residues, but master only
            if (i >= 0 && i < master->Length())
                GlobalMessenger()->AddHighlights(GetSequenceOfRow(0), i, i);
            continue;
        }

        // get block and offset
        const Block *block = GetBlock(0, i);
        int blockOffset = i - block->GetRangeOfRow(0)->from;

        // highlight aligned residue in each row
        for (int row=0; row<NRows(); ++row) {
            int slaveIndex = block->GetRangeOfRow(row)->from + blockOffset;
            GlobalMessenger()->AddHighlights(GetSequenceOfRow(row), slaveIndex, slaveIndex);
        }
    }

    return !anyError;
}

int BlockMultipleAlignment::NAlignedBlocks(void) const
{
    int n = 0;
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) if ((*b)->IsAligned()) ++n;
    return n;
}

bool BlockMultipleAlignment::HasNoAlignedBlocks(void) const
{
    return (blocks.size() == 0 || (blocks.size() == 1 && !blocks.front()->IsAligned()));
}

int BlockMultipleAlignment::GetAlignmentIndex(int row, int seqIndex, eUnalignedJustification justification) const
{
    if (row < 0 || row >= NRows() || seqIndex < 0 || seqIndex >= GetSequenceOfRow(row)->Length()) {
        ERRORMSG("BlockMultipleAlignment::GetAlignmentIndex() - coordinate out of range");
        return -1;
    }

    int alignmentIndex, blockColumn;
    const Block *block = NULL;
    const Block::Range *range;

    for (alignmentIndex=0; alignmentIndex<totalWidth; ++alignmentIndex) {

        // check each block to see if index is in range
        if (block != blockMap[alignmentIndex].block) {
            block = blockMap[alignmentIndex].block;

            range = block->GetRangeOfRow(row);
            if (seqIndex >= range->from && seqIndex <= range->to) {

                // override requested justification for end blocks
                if (block == blocks.back()) // also true if there's a single aligned block
                    justification = eLeft;
                else if (block == blocks.front())
                    justification = eRight;

                // search block columns to find index (inefficient, but avoids having to write
                // inverse functions of Block::GetIndexAt()
                for (blockColumn=0; blockColumn<block->width; ++blockColumn) {
                    if (seqIndex == block->GetIndexAt(blockColumn, row, justification))
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

// creates a SeqAlign from a BlockMultipleAlignment
SeqAlignPtr BlockMultipleAlignment::CreateCSeqAlign(void) const
{
    // one SeqAlign (chained into a linked list) for each slave row
    SeqAlignPtr prevSap = NULL, firstSap = NULL;
    for (int row=1; row<NRows(); ++row) {

        SeqAlignPtr sap = SeqAlignNew();
        if (prevSap) prevSap->next = sap;
        prevSap = sap;
		if (!firstSap) firstSap = sap;

        sap->type = SAT_PARTIAL;
        sap->dim = 2;
        sap->segtype = SAS_DENDIAG;

        DenseDiagPtr prevDd = NULL;
        UngappedAlignedBlockList blocks;
        GetUngappedAlignedBlocks(&blocks);
        UngappedAlignedBlockList::const_iterator b, be = blocks.end();

        for (b=blocks.begin(); b!=be; ++b) {
            DenseDiagPtr dd = DenseDiagNew();
            if (prevDd) prevDd->next = dd;
            prevDd = dd;
            if (b == blocks.begin()) sap->segs = dd;

            dd->dim = 2;
            GetSequenceOfRow(0)->AddCSeqId(&(dd->id), false);      // master
            GetSequenceOfRow(row)->AddCSeqId(&(dd->id), false);    // slave
            dd->len = (*b)->width;

            dd->starts = (Int4Ptr) MemNew(2 * sizeof(Int4));
            const Block::Range *range = (*b)->GetRangeOfRow(0);
            dd->starts[0] = range->from;
            range = (*b)->GetRangeOfRow(row);
            dd->starts[1] = range->from;
        }
    }

	return firstSap;
}


///// UngappedAlignedBlock methods /////

char UngappedAlignedBlock::GetCharacterAt(int blockColumn, int row) const
{
    return (*(parentAlignment->GetSequences()))[row]->sequenceString[GetIndexAt(blockColumn, row)];
}

Block * UngappedAlignedBlock::Clone(const BlockMultipleAlignment *newMultiple) const
{
    UngappedAlignedBlock *copy = new UngappedAlignedBlock(newMultiple);
    const Block::Range *range;
    for (int row=0; row<NSequences(); ++row) {
        range = GetRangeOfRow(row);
        copy->SetRangeOfRow(row, range->from, range->to);
    }
    copy->width = width;
    return copy;
}

void UngappedAlignedBlock::DeleteRow(int row)
{
    RangeList::iterator r = ranges.begin();
    for (int i=0; i<row; ++i) ++r;
    ranges.erase(r);
}

void UngappedAlignedBlock::DeleteRows(vector < bool >& removeRows, int nToRemove)
{
    VectorRemoveElements(ranges, removeRows, nToRemove);
}


///// UnalignedBlock methods /////

int UnalignedBlock::GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const
{
    const Block::Range *range = GetRangeOfRow(row);
    int seqIndex, rangeWidth, rangeMiddle, extraSpace;

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
    for (int i=0; i<NSequences(); ++i) {
        int blockWidth = ranges[i].to - ranges[i].from + 1;
        if (blockWidth > width) width = blockWidth;
    }
}

void UnalignedBlock::DeleteRow(int row)
{
    RangeList::iterator r = ranges.begin();
    for (int i=0; i<row; ++i) ++r;
    ranges.erase(r);
    Resize();
}

void UnalignedBlock::DeleteRows(vector < bool >& removeRows, int nToRemove)
{
    VectorRemoveElements(ranges, removeRows, nToRemove);
    Resize();
}

Block * UnalignedBlock::Clone(const BlockMultipleAlignment *newMultiple) const
{
    UnalignedBlock *copy = new UnalignedBlock(newMultiple);
    const Block::Range *range;
    for (int row=0; row<NSequences(); ++row) {
        range = GetRangeOfRow(row);
        copy->SetRangeOfRow(row, range->from, range->to);
    }
    copy->width = width;
    return copy;
}

int UnalignedBlock::MinResidues(void) const
{
    int min = -1, m;
    const Block::Range *range;
    for (int row=0; row<NSequences(); ++row) {
        range = GetRangeOfRow(row);
        m = range->to - range->from + 1;
        if (row == 0 || m < min)
            min = m;
    }
    return min;
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.66  2005/03/07 11:47:00  thiessen
* fix workshop warnings
*
* Revision 1.65  2005/01/28 21:31:02  thiessen
* don't add block if validation fails
*
* Revision 1.64  2004/10/04 17:00:54  thiessen
* add expand/restrict highlights, delete all blocks/all rows in updates
*
* Revision 1.63  2004/10/01 13:07:44  thiessen
* add ZipAlignResidue
*
* Revision 1.62  2004/09/27 15:33:04  thiessen
* add block info content optimization on ctrl+shift+click
*
* Revision 1.61  2004/09/27 01:00:44  thiessen
* extend "complete contained" blocks ; merge adjacent blocks after extension"
*
* Revision 1.60  2004/09/23 10:31:14  thiessen
* add block extension algorithm
*
* Revision 1.59  2004/07/27 17:38:12  thiessen
* don't call GetPSSM() w/ no aligned blocks
*
* Revision 1.58  2004/06/23 00:15:47  thiessen
* fix row addition/deletion problem with vector synchronization
*
* Revision 1.57  2004/05/26 14:18:29  thiessen
* remove status stuff in ExtractRows
*
* Revision 1.56  2004/05/21 21:41:38  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.55  2004/03/15 17:27:02  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.54  2004/02/19 17:04:43  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.53  2003/11/06 18:52:31  thiessen
* make geometry violations shown on/off; allow multiple pmid entry in ref dialog
*
* Revision 1.52  2003/07/14 18:37:07  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.51  2003/06/13 14:46:58  thiessen
* fix row cache bug
*
* Revision 1.50  2003/06/12 14:21:11  thiessen
* when saving single-row alignment, make master-master alignment in asn
*
* Revision 1.49  2003/02/06 16:39:52  thiessen
* add block row fit coloring
*
* Revision 1.48  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.47  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.46  2003/01/30 14:00:23  thiessen
* add Block Z Fit coloring
*
* Revision 1.45  2003/01/28 21:07:56  thiessen
* add block fit coloring algorithm; tweak row dragging; fix style bug
*
* Revision 1.44  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.43  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.42  2002/12/13 15:33:37  thiessen
* tweak to HighlightAlignedColumnsOfMasterRange
*
* Revision 1.41  2002/12/12 23:07:18  thiessen
* improved handling of alignment annotation errors
*
* Revision 1.40  2002/11/18 20:49:11  thiessen
* move unaligned/no-coord colors into Colors class
*
* Revision 1.39  2002/10/23 01:29:25  thiessen
* fix block cache bug
*
* Revision 1.38  2002/08/28 20:30:33  thiessen
* fix proximity sort bug
*
* Revision 1.37  2002/07/26 15:28:44  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.36  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.35  2002/02/21 22:01:49  thiessen
* remember alignment range on demotion
*
* Revision 1.34  2002/02/19 14:59:38  thiessen
* add CDD reject and purge sequence
*
* Revision 1.33  2002/02/05 18:53:24  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.32  2001/12/06 23:13:44  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.31  2001/11/27 16:26:06  thiessen
* major update to data management system
*
* Revision 1.30  2001/10/02 23:26:47  thiessen
* fix bug in highlight columns in master range
*
* Revision 1.29  2001/09/27 15:37:57  thiessen
* decouple sequence import and BLAST
*
* Revision 1.28  2001/09/04 14:40:19  thiessen
* add rainbow and charge coloring
*
* Revision 1.27  2001/08/24 00:41:35  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.26  2001/08/09 19:07:13  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.25  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.24  2001/07/10 16:39:54  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.23  2001/06/21 02:02:33  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.22  2001/06/02 17:22:45  thiessen
* fixes for GCC
*
* Revision 1.21  2001/06/01 13:35:58  thiessen
* add aligned block number to status line
*
* Revision 1.20  2001/05/31 18:47:06  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.19  2001/05/23 17:45:00  thiessen
* fix merge bug where unaligned block needs to be added
*
* Revision 1.18  2001/05/11 02:10:41  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.17  2001/05/09 17:15:06  thiessen
* add automatic block removal upon demotion
*
* Revision 1.16  2001/05/02 13:46:27  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.15  2001/04/19 14:24:05  thiessen
* fix row selection bug when alignments are of different size
*
* Revision 1.14  2001/04/19 12:58:32  thiessen
* allow merge and delete of individual updates
*
* Revision 1.13  2001/04/05 22:55:34  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.12  2001/04/04 00:27:14  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.11  2001/03/30 03:07:33  thiessen
* add threader score calculation & sorting
*
* Revision 1.10  2001/03/22 19:11:09  thiessen
* don't allow drag of gaps
*
* Revision 1.9  2001/03/22 00:33:16  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.8  2001/03/09 15:49:03  thiessen
* major changes to add initial update viewer
*
* Revision 1.7  2001/03/06 20:20:50  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.6  2001/03/01 20:15:50  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.5  2001/02/13 20:33:49  thiessen
* add information content coloring
*
* Revision 1.4  2001/02/13 01:03:56  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.3  2001/02/02 20:17:33  thiessen
* can read in CDD with multi-structure but no struct. alignments
*
* Revision 1.2  2001/01/26 19:29:59  thiessen
* limit undo stack size ; fix memory leak
*
* Revision 1.1  2000/11/30 15:49:35  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
*/
