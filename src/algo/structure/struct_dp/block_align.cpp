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
*      Dynamic programming-based alignment algorithms: block aligner
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_limits.h>

#include <vector>
#include <algorithm>

#include "struct_dp/struct_dp.h"

USING_NCBI_SCOPE;


const int DP_NEGATIVE_INFINITY = kMin_Int;
const unsigned int DP_UNFROZEN_BLOCK = kMax_UInt;

BEGIN_SCOPE(struct_dp)

#define ERROR_MESSAGE(s) ERR_POST(Error << "block_align: " << s << '!')
#define WARNING_MESSAGE(s) ERR_POST(Warning << "block_align: " << s)
#define INFO_MESSAGE(s) ERR_POST(Info << "block_align: " << s)

#define NO_TRACEBACK kMax_UInt

class Cell
{
public:
    int score;
    unsigned int tracebackResidue;
    Cell(void) : score(DP_NEGATIVE_INFINITY), tracebackResidue(NO_TRACEBACK) { }
};

class Matrix
{
public:
    typedef vector < Cell > ResidueRow;
    typedef vector < ResidueRow > Grid;
    Grid grid;
    Matrix(unsigned int nBlocks, unsigned int nResidues) : grid(nBlocks + 1)
        { for (unsigned int i=0; i<nBlocks; i++) grid[i].resize(nResidues + 1); }
    ResidueRow& operator [] (unsigned int block) { return grid[block]; }
    const ResidueRow& operator [] (unsigned int block) const { return grid[block]; }
};

// checks to make sure frozen block positions are legal (borrowing my own code from blockalign.c)
int ValidateFrozenBlockPositions(const DP_BlockInfo *blocks, unsigned int queryFrom, unsigned int queryTo)
{
    static const unsigned int NONE = kMax_UInt;
    unsigned int
        block,
        unfrozenBlocksLength = 0,
        prevFrozenBlock = NONE,
        maxGapsLength = 0;

    for (block=0; block<blocks->nBlocks; block++) {

        /* keep track of max gap space between frozen blocks */
        if (block > 0)
            maxGapsLength += blocks->maxLoops[block - 1];

        /* to allow room for unfrozen blocks between frozen ones */
        if (blocks->freezeBlocks[block] == DP_UNFROZEN_BLOCK) {
            unfrozenBlocksLength += blocks->blockSizes[block];
            continue;
        }

        /* check absolute block end positions */
        if (blocks->freezeBlocks[block] < queryFrom) {
            ERROR_MESSAGE("Frozen block " << (block+1) << " can't start < " << (queryFrom+1));
            return STRUCT_DP_PARAMETER_ERROR;
        }
        if (blocks->freezeBlocks[block] + blocks->blockSizes[block] - 1 > queryTo) {
            ERROR_MESSAGE("Frozen block " << (block+1) << " can't end > " << (queryTo+1));
            return STRUCT_DP_PARAMETER_ERROR;
        }

        /* checks for legal distances between frozen blocks */
        if (prevFrozenBlock != NONE) {
            unsigned int prevFrozenBlockEnd =
                blocks->freezeBlocks[prevFrozenBlock] + blocks->blockSizes[prevFrozenBlock] - 1;

            /* check for adequate room for unfrozen blocks between frozen blocks */
            if (blocks->freezeBlocks[block] <= (prevFrozenBlockEnd + unfrozenBlocksLength)) {
                ERROR_MESSAGE("Frozen block " << (block+1) << " starts before end of prior frozen block, "
                    "or doesn't leave room for intervening unfrozen block(s)");
                return STRUCT_DP_PARAMETER_ERROR;
            }

            /* check for too much gap space since last frozen block,
               but if frozen blocks are adjacent, issue warning only */
            if (blocks->freezeBlocks[block] > prevFrozenBlockEnd + 1 + unfrozenBlocksLength + maxGapsLength)
            {
                if (prevFrozenBlock == block - 1) {
                    WARNING_MESSAGE("Frozen block " << (block+1) << " is further than allowed loop length"
                        " from adjacent frozen block " << (prevFrozenBlock+1));
                } else {
                    ERROR_MESSAGE("Frozen block " << (block+1) << " is too far away from prior frozen block"
                        " given allowed intervening loop length(s) " << maxGapsLength
                        << " plus unfrozen block length(s) " << unfrozenBlocksLength);
                    return STRUCT_DP_PARAMETER_ERROR;
                }
            }
        }

        /* reset counters after each frozen block */
        prevFrozenBlock = block;
        unfrozenBlocksLength = maxGapsLength = 0;
    }

    return STRUCT_DP_OKAY;
}

// fill matrix values; return true on success. Matrix must have only default values when passed in.
int CalculateGlobalMatrix(Matrix& matrix,
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore,
    unsigned int queryFrom, unsigned int queryTo)
{
    unsigned int block, residue, prevResidue, lastBlock = blocks->nBlocks - 1;
    int score = 0, sum;

    // find possible block positions, based purely on block lengths
    vector < unsigned int > firstPos(blocks->nBlocks), lastPos(blocks->nBlocks);
    for (block=0; block<=lastBlock; block++) {
        if (block == 0) {
            firstPos[0] = queryFrom;
            lastPos[lastBlock] = queryTo - blocks->blockSizes[lastBlock] + 1;
        } else {
            firstPos[block] = firstPos[block - 1] + blocks->blockSizes[block - 1];
            lastPos[lastBlock - block] =
                lastPos[lastBlock - block + 1] - blocks->blockSizes[lastBlock - block];
        }
    }

    // further restrict the search if blocks are frozen
    for (block=0; block<=lastBlock; block++) {
        if (blocks->freezeBlocks[block] != DP_UNFROZEN_BLOCK) {
            if (blocks->freezeBlocks[block] < firstPos[block] ||
                blocks->freezeBlocks[block] > lastPos[block])
            {
                ERROR_MESSAGE("CalculateGlobalMatrix() - frozen block "
                    << (block+1) << " does not leave room for unfrozen blocks");
                return STRUCT_DP_PARAMETER_ERROR;
            }
            firstPos[block] = lastPos[block] = blocks->freezeBlocks[block];
        }
    }

    // fill in first row with scores of first block at all possible positions
    for (residue=firstPos[0]; residue<=lastPos[0]; residue++)
        matrix[0][residue - queryFrom].score = BlockScore(0, residue);

    // for each successive block, find the best allowed pairing of the block with the previous block
    bool blockScoreCalculated;
    for (block=1; block<=lastBlock; block++) {
        for (residue=firstPos[block]; residue<=lastPos[block]; residue++) {
            blockScoreCalculated = false;

            for (prevResidue=firstPos[block - 1]; prevResidue<=lastPos[block - 1]; prevResidue++) {

                // current block must come after the previous block
                if (residue < prevResidue + blocks->blockSizes[block - 1])
                    break;

                // cut off at max loop length from previous block, but not if both blocks are frozen
                if (residue > prevResidue + blocks->blockSizes[block - 1] + blocks->maxLoops[block - 1] &&
                        (blocks->freezeBlocks[block] == DP_UNFROZEN_BLOCK ||
                         blocks->freezeBlocks[block - 1] == DP_UNFROZEN_BLOCK))
                    continue;

                // make sure previous block is at an allowed position
                if (matrix[block - 1][prevResidue - queryFrom].score == DP_NEGATIVE_INFINITY)
                    continue;

                // get score at this position
                if (!blockScoreCalculated) {
                    score = BlockScore(block, residue);
                    if (score == DP_NEGATIVE_INFINITY)
                        break;
                    blockScoreCalculated = true;
                }

                // find highest sum of scores for allowed pairing of this block with any previous
                sum = score + matrix[block - 1][prevResidue - queryFrom].score;
                if (sum > matrix[block][residue - queryFrom].score) {
                    matrix[block][residue - queryFrom].score = sum;
                    matrix[block][residue - queryFrom].tracebackResidue = prevResidue;
                }
            }
        }
    }

    return STRUCT_DP_OKAY;
}

// fill matrix values; return true on success. Matrix must have only default values when passed in.
int CalculateLocalMatrix(Matrix& matrix,
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore,
    unsigned int queryFrom, unsigned int queryTo)
{
    unsigned int block, residue, prevResidue, lastBlock = blocks->nBlocks - 1, tracebackResidue = 0;
    int score, sum, bestPrevScore;

    // find last possible block positions, based purely on block lengths
    vector < unsigned int > lastPos(blocks->nBlocks);
    for (block=0; block<=lastBlock; block++) {
        if (blocks->blockSizes[block] > queryTo - queryFrom + 1) {
            ERROR_MESSAGE("Block " << (block+1) << " too large for this query range");
            return STRUCT_DP_PARAMETER_ERROR;
        }
        lastPos[block] = queryTo - blocks->blockSizes[block] + 1;
    }

    // first row: positive scores of first block at all possible positions
    for (residue=queryFrom; residue<=lastPos[0]; residue++) {
        score = BlockScore(0, residue);
        matrix[0][residue - queryFrom].score = (score > 0) ? score : 0;
    }

    // first column: positive scores of all blocks at first positions
    for (block=1; block<=lastBlock; block++) {
        score = BlockScore(block, queryFrom);
        matrix[block][0].score = (score > 0) ? score : 0;
    }

    // for each successive block, find the best positive scoring with a previous block, if any
    for (block=1; block<=lastBlock; block++) {
        for (residue=queryFrom+1; residue<=lastPos[block]; residue++) {

            // get score at this position
            score = BlockScore(block, residue);
            if (score == DP_NEGATIVE_INFINITY)
                continue;

            // find max score of any allowed previous block
            bestPrevScore = DP_NEGATIVE_INFINITY;
            for (prevResidue=queryFrom+1; prevResidue<=lastPos[block - 1]; prevResidue++) {

                // current block must come after the previous block
                if (residue < prevResidue + blocks->blockSizes[block - 1])
                    break;

                // cut off at max loop length from previous block
                if (residue > prevResidue + blocks->blockSizes[block - 1] + blocks->maxLoops[block - 1])
                    continue;

                if (matrix[block - 1][prevResidue - queryFrom].score > bestPrevScore) {
                    bestPrevScore = matrix[block - 1][prevResidue - queryFrom].score;
                    tracebackResidue = prevResidue;
                }
            }

            // extend alignment if the sum of this block's + previous block's score is positive
            if (bestPrevScore > 0 && (sum=bestPrevScore+score) > 0) {
                matrix[block][residue - queryFrom].score = sum;
                matrix[block][residue - queryFrom].tracebackResidue = tracebackResidue;
            }

            // otherwise, start new alignment if score is positive
            else if (score > 0)
                matrix[block][residue - queryFrom].score = score;
        }
    }

    return STRUCT_DP_OKAY;
}

int TracebackAlignment(const Matrix& matrix, unsigned int lastBlock, unsigned int lastBlockPos,
    unsigned int queryFrom, DP_AlignmentResult **alignment)
{
    // trace backwards from last block/pos
    vector < unsigned int > blockPositions;
    unsigned int block = lastBlock, pos = lastBlockPos;
    do {
        blockPositions.push_back(pos);  // list is backwards after this...
        pos = matrix[block][pos - queryFrom].tracebackResidue;
        block--;
    } while (pos != NO_TRACEBACK);
    unsigned int firstBlock = block + 1; // last block traced to == first block of the alignment

    // allocate and fill in alignment result structure
    *alignment = new DP_AlignmentResult;
    (*alignment)->score = matrix[lastBlock][lastBlockPos - queryFrom].score;
    (*alignment)->firstBlock = firstBlock;
    (*alignment)->nBlocks = blockPositions.size();
    (*alignment)->blockPositions = new unsigned int[blockPositions.size()];
    for (block=0; block<blockPositions.size(); block++)
        (*alignment)->blockPositions[block] = blockPositions[lastBlock - firstBlock - block];

    return STRUCT_DP_FOUND_ALIGNMENT;
}

int TracebackGlobalAlignment(const Matrix& matrix,
    const DP_BlockInfo *blocks, unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    // find max score (e.g., best-scoring position of last block)
    int score = DP_NEGATIVE_INFINITY;
    unsigned int residue, lastBlockPos = 0;
    for (residue=queryFrom; residue<=queryTo; residue++) {
        if (matrix[blocks->nBlocks - 1][residue - queryFrom].score > score) {
            score = matrix[blocks->nBlocks - 1][residue - queryFrom].score;
            lastBlockPos = residue;
        }
    }

    if (score == DP_NEGATIVE_INFINITY) {
        ERROR_MESSAGE("TracebackGlobalAlignment() - somehow failed to find any allowed global alignment");
        return STRUCT_DP_ALGORITHM_ERROR;
    }

    INFO_MESSAGE("Score of best global alignment: " << score);
    return TracebackAlignment(matrix, blocks->nBlocks - 1, lastBlockPos, queryFrom, alignment);
}

int TracebackLocalAlignment(const Matrix& matrix,
    const DP_BlockInfo *blocks, unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    // find max score (e.g., best-scoring position of any block)
    int score = DP_NEGATIVE_INFINITY;
    unsigned int block, residue, lastBlock = 0, lastBlockPos = 0;
    for (block=0; block<blocks->nBlocks; block++) {
        for (residue=queryFrom; residue<=queryTo; residue++) {
            if (matrix[block][residue - queryFrom].score > score) {
                score = matrix[block][residue - queryFrom].score;
                lastBlock = block;
                lastBlockPos = residue;
            }
        }
    }

    if (score <= 0) {
        INFO_MESSAGE("No positive-scoring local alignment found.");
        return STRUCT_DP_NO_ALIGNMENT;
    }

    INFO_MESSAGE("Score of best local alignment: " << score);
    return TracebackAlignment(matrix, lastBlock, lastBlockPos, queryFrom, alignment);
}

END_SCOPE(struct_dp)


// leave the C-called functions outside any namespace, just in case that might
// cause any problems when linking to C code...
USING_SCOPE(struct_dp);


int DP_GlobalBlockAlign(
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore,
    unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    if (!blocks || blocks->nBlocks < 1 || !blocks->blockSizes || !BlockScore || queryTo < queryFrom) {
        ERROR_MESSAGE("DP_GlobalBlockAlign() - invalid parameters");
        return STRUCT_DP_PARAMETER_ERROR;
    }

    unsigned int i, sumBlockLen = 0;
    for (i=0; i<blocks->nBlocks; i++)
        sumBlockLen += blocks->blockSizes[i];
    if (sumBlockLen > queryTo - queryFrom + 1) {
        ERROR_MESSAGE("DP_GlobalBlockAlign() - sum of block lengths longer than query region");
        return STRUCT_DP_PARAMETER_ERROR;
    }

    int status = ValidateFrozenBlockPositions(blocks, queryFrom, queryTo);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_GlobalBlockAlign() - ValidateFrozenBlockPositions() returned error");
        return status;
    }

    Matrix matrix(blocks->nBlocks, queryTo - queryFrom + 1);

    status = CalculateGlobalMatrix(matrix, blocks, BlockScore, queryFrom, queryTo);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_GlobalBlockAlign() - CalculateGlobalMatrix() failed");
        return status;
    }

    return TracebackGlobalAlignment(matrix, blocks, queryFrom, queryTo, alignment);
}

int DP_LocalBlockAlign(
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore,
    unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    if (!blocks || blocks->nBlocks < 1 || !blocks->blockSizes || !BlockScore || queryTo < queryFrom) {
        ERROR_MESSAGE("DP_LocalBlockAlign() - invalid parameters");
        return STRUCT_DP_PARAMETER_ERROR;
    }
    for (unsigned int block=0; block<blocks->nBlocks; block++) {
        if (blocks->freezeBlocks[block] != DP_UNFROZEN_BLOCK) {
            WARNING_MESSAGE("DP_LocalBlockAlign() - frozen block specifications are ignored...");
            break;
        }
    }

    Matrix matrix(blocks->nBlocks, queryTo - queryFrom + 1);

    int status = CalculateLocalMatrix(matrix, blocks, BlockScore, queryFrom, queryTo);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_LocalBlockAlign() - CalculateLocalMatrix() failed");
        return status;
    }

    return TracebackLocalAlignment(matrix, blocks, queryFrom, queryTo, alignment);
}

DP_BlockInfo * DP_CreateBlockInfo(unsigned int nBlocks)
{
    DP_BlockInfo *blocks = new DP_BlockInfo;
    blocks->nBlocks = nBlocks;
    blocks->blockPositions = new unsigned int[nBlocks];
    blocks->blockSizes = new unsigned int[nBlocks];
    blocks->maxLoops = new unsigned int[nBlocks - 1];
    blocks->freezeBlocks = new unsigned int[nBlocks];
    for (unsigned int i=0; i<nBlocks; i++)
        blocks->freezeBlocks[i] = DP_UNFROZEN_BLOCK;
    return blocks;
}

void DP_DestroyBlockInfo(DP_BlockInfo *blocks)
{
    if (!blocks)
        return;
    delete blocks->blockPositions;
    delete blocks->blockSizes;
    delete blocks->maxLoops;
    delete blocks->freezeBlocks;
    delete blocks;
}

void DP_DestroyAlignmentResult(DP_AlignmentResult *alignment)
{
    if (!alignment)
        return;
    delete alignment->blockPositions;
    delete alignment;
}

unsigned int DP_CalculateMaxLoopLength(
        unsigned int nLoops, const unsigned int *loops,
        double percentile, unsigned int extension, unsigned int cutoff)
{
    vector < unsigned int > loopLengths(nLoops);
    unsigned int index, max;
    for (index=0; index<nLoops; index++)
        loopLengths[index] = loops[index];

    stable_sort(loopLengths.begin(), loopLengths.end());

    if (percentile < 1.0) {
        index = (unsigned int)(percentile * (nLoops - 1) + 0.5);
        max = loopLengths[index] + extension;
    } else {  // percentile >= 1.0
        max = ((unsigned int)(percentile * loopLengths[nLoops - 1] + 0.5)) + extension;
    }

    if (cutoff > 0 && max > cutoff)
        max = cutoff;

    return max;
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2003/08/22 14:28:49  thiessen
* add standard loop calculating function
*
* Revision 1.10  2003/08/19 19:36:48  thiessen
* avoid annoying 'might be used uninitialized' warnings
*
* Revision 1.9  2003/08/19 19:26:21  thiessen
* error if block size > query range length
*
* Revision 1.8  2003/07/11 15:27:48  thiessen
* add DP_ prefix to globals
*
* Revision 1.7  2003/06/22 12:18:16  thiessen
* fixes for unsigned/signed comparison
*
* Revision 1.6  2003/06/22 12:11:37  thiessen
* add local alignment
*
* Revision 1.5  2003/06/19 13:48:23  thiessen
* cosmetic/typo fixes
*
* Revision 1.4  2003/06/18 21:55:15  thiessen
* remove unused params
*
* Revision 1.3  2003/06/18 21:46:09  thiessen
* add traceback, alignment return structure
*
* Revision 1.2  2003/06/18 19:10:17  thiessen
* fix lf issues
*
* Revision 1.1  2003/06/18 16:54:12  thiessen
* initial checkin; working global block aligner and demo app
*
*/
