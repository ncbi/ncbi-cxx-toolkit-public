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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_limits.h>

#include <vector>
#include <list>
#include <algorithm>

#include <algo/structure/struct_dp/struct_dp.h>

USING_NCBI_SCOPE;


const int DP_NEGATIVE_INFINITY = kMin_Int;
const unsigned int DP_POSITIVE_INFINITY = kMax_UInt;
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
        { for (unsigned int i=0; i<nBlocks; ++i) grid[i].resize(nResidues + 1); }
    ResidueRow& operator [] (unsigned int block) { return grid[block]; }
    const ResidueRow& operator [] (unsigned int block) const { return grid[block]; }
};

// checks to make sure frozen block positions are legal (borrowing my own code from blockalign.c)
int ValidateFrozenBlockPositions(const DP_BlockInfo *blocks,
    unsigned int queryFrom, unsigned int queryTo, bool checkGapSum)
{
    static const unsigned int NONE = kMax_UInt;
    unsigned int
        block,
        unfrozenBlocksLength = 0,
        prevFrozenBlock = NONE,
        maxGapsLength = 0;

    for (block=0; block<blocks->nBlocks; ++block) {

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
            if (checkGapSum &&
                blocks->freezeBlocks[block] > prevFrozenBlockEnd + 1 + unfrozenBlocksLength + maxGapsLength)
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
    for (block=0; block<=lastBlock; ++block) {
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
    for (block=0; block<=lastBlock; ++block) {
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
    for (residue=firstPos[0]; residue<=lastPos[0]; ++residue)
        matrix[0][residue - queryFrom].score = BlockScore(0, residue);

    // for each successive block, find the best allowed pairing of the block with the previous block
    bool blockScoreCalculated;
    for (block=1; block<=lastBlock; ++block) {
        for (residue=firstPos[block]; residue<=lastPos[block]; ++residue) {
            blockScoreCalculated = false;

            for (prevResidue=firstPos[block - 1]; prevResidue<=lastPos[block - 1]; ++prevResidue) {

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
int CalculateGlobalMatrixGeneric(Matrix& matrix,
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore, DP_LoopPenaltyFunction LoopScore,
    unsigned int queryFrom, unsigned int queryTo)
{
    unsigned int block, residue, prevResidue, lastBlock = blocks->nBlocks - 1;
    int blockScore = 0, sum;
    unsigned int loopPenalty;

    // find possible block positions, based purely on block lengths
    vector < unsigned int > firstPos(blocks->nBlocks), lastPos(blocks->nBlocks);
    for (block=0; block<=lastBlock; ++block) {
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
    for (block=0; block<=lastBlock; ++block) {
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
    for (residue=firstPos[0]; residue<=lastPos[0]; ++residue)
        matrix[0][residue - queryFrom].score = BlockScore(0, residue);

    // for each successive block, find the best allowed pairing of the block with the previous block
    bool blockScoreCalculated;
    for (block=1; block<=lastBlock; ++block) {
        for (residue=firstPos[block]; residue<=lastPos[block]; ++residue) {
            blockScoreCalculated = false;

            for (prevResidue=firstPos[block - 1]; prevResidue<=lastPos[block - 1]; ++prevResidue) {

                // current block must come after the previous block
                if (residue < prevResidue + blocks->blockSizes[block - 1])
                    break;

                // make sure previous block is at an allowed position
                if (matrix[block - 1][prevResidue - queryFrom].score == DP_NEGATIVE_INFINITY)
                    continue;

                // get loop score at this position; assume loop score zero if both frozen
                if (blocks->freezeBlocks[block] != DP_UNFROZEN_BLOCK &&
                    blocks->freezeBlocks[block - 1] != DP_UNFROZEN_BLOCK)
                {
                    loopPenalty = 0;
                } else {
                    loopPenalty = LoopScore(block - 1, residue - prevResidue - blocks->blockSizes[block - 1]);
                    if (loopPenalty == DP_POSITIVE_INFINITY)
                        continue;
                }

                // get score at this position
                if (!blockScoreCalculated) {
                    blockScore = BlockScore(block, residue);
                    if (blockScore == DP_NEGATIVE_INFINITY)
                        break;
                    blockScoreCalculated = true;
                }

                // find highest sum of scores + loop score for allowed pairing of this block with previous
                sum = blockScore + matrix[block - 1][prevResidue - queryFrom].score - loopPenalty;
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
    for (block=0; block<=lastBlock; ++block) {
        if (blocks->blockSizes[block] > queryTo - queryFrom + 1) {
            ERROR_MESSAGE("Block " << (block+1) << " too large for this query range");
            return STRUCT_DP_PARAMETER_ERROR;
        }
        lastPos[block] = queryTo - blocks->blockSizes[block] + 1;
    }

    // first row: positive scores of first block at all possible positions
    for (residue=queryFrom; residue<=lastPos[0]; ++residue) {
        score = BlockScore(0, residue);
        matrix[0][residue - queryFrom].score = (score > 0) ? score : 0;
    }

    // first column: positive scores of all blocks at first positions
    for (block=1; block<=lastBlock; ++block) {
        score = BlockScore(block, queryFrom);
        matrix[block][0].score = (score > 0) ? score : 0;
    }

    // for each successive block, find the best positive scoring with a previous block, if any
    for (block=1; block<=lastBlock; ++block) {
        for (residue=queryFrom+1; residue<=lastPos[block]; ++residue) {

            // get score at this position
            score = BlockScore(block, residue);
            if (score == DP_NEGATIVE_INFINITY)
                continue;

            // find max score of any allowed previous block
            bestPrevScore = DP_NEGATIVE_INFINITY;
            for (prevResidue=queryFrom; prevResidue<=lastPos[block - 1]; ++prevResidue) {

                // current block must come after the previous block
                if (residue < prevResidue + blocks->blockSizes[block - 1])
                    break;

                // cut off at max loop length from previous block
                if (residue > prevResidue + blocks->blockSizes[block - 1] + blocks->maxLoops[block - 1])
                    continue;

                // keep maximum score
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

// fill matrix values; return true on success. Matrix must have only default values when passed in.
int CalculateLocalMatrixGeneric(Matrix& matrix,
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore, DP_LoopPenaltyFunction LoopScore,
    unsigned int queryFrom, unsigned int queryTo)
{
    unsigned int block, residue, prevResidue, loopPenalty,
        lastBlock = blocks->nBlocks - 1, tracebackResidue = 0;
    int score, sum, bestPrevScore;

    // find last possible block positions, based purely on block lengths
    vector < unsigned int > lastPos(blocks->nBlocks);
    for (block=0; block<=lastBlock; ++block) {
        if (blocks->blockSizes[block] > queryTo - queryFrom + 1) {
            ERROR_MESSAGE("Block " << (block+1) << " too large for this query range");
            return STRUCT_DP_PARAMETER_ERROR;
        }
        lastPos[block] = queryTo - blocks->blockSizes[block] + 1;
    }

    // first row: positive scores of first block at all possible positions
    for (residue=queryFrom; residue<=lastPos[0]; ++residue) {
        score = BlockScore(0, residue);
        matrix[0][residue - queryFrom].score = (score > 0) ? score : 0;
    }

    // first column: positive scores of all blocks at first positions
    for (block=1; block<=lastBlock; ++block) {
        score = BlockScore(block, queryFrom);
        matrix[block][0].score = (score > 0) ? score : 0;
    }

    // for each successive block, find the best positive scoring with a previous block, if any
    for (block=1; block<=lastBlock; ++block) {
        for (residue=queryFrom+1; residue<=lastPos[block]; ++residue) {

            // get score at this position
            score = BlockScore(block, residue);
            if (score == DP_NEGATIVE_INFINITY)
                continue;

            // find max score of any allowed previous block
            bestPrevScore = DP_NEGATIVE_INFINITY;
            for (prevResidue=queryFrom; prevResidue<=lastPos[block - 1]; ++prevResidue) {

                // current block must come after the previous block
                if (residue < prevResidue + blocks->blockSizes[block - 1])
                    break;

                // make sure previous block is at an allowed position
                if (matrix[block - 1][prevResidue - queryFrom].score == DP_NEGATIVE_INFINITY)
                    continue;

                // get loop score
                loopPenalty = LoopScore(block - 1, residue - prevResidue - blocks->blockSizes[block - 1]);
                if (loopPenalty == DP_POSITIVE_INFINITY)
                    continue;

                // keep maximum score
                sum = matrix[block - 1][prevResidue - queryFrom].score - loopPenalty;
                if (sum > bestPrevScore) {
                    bestPrevScore = sum;
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
    unsigned int queryFrom, DP_AlignmentResult *alignment)
{
    // trace backwards from last block/pos
    vector < unsigned int > blockPositions;
    unsigned int block = lastBlock, pos = lastBlockPos;
    do {
        blockPositions.push_back(pos);  // list is backwards after this...
        pos = matrix[block][pos - queryFrom].tracebackResidue;
        --block;
    } while (pos != NO_TRACEBACK);
    unsigned int firstBlock = block + 1; // last block traced to == first block of the alignment

    // allocate and fill in alignment result structure
    alignment->score = matrix[lastBlock][lastBlockPos - queryFrom].score;
    alignment->firstBlock = firstBlock;
    alignment->nBlocks = blockPositions.size();
    alignment->blockPositions = new unsigned int[blockPositions.size()];
    for (block=0; block<blockPositions.size(); ++block)
        alignment->blockPositions[block] = blockPositions[lastBlock - firstBlock - block];

    return STRUCT_DP_FOUND_ALIGNMENT;
}

int TracebackGlobalAlignment(const Matrix& matrix,
    const DP_BlockInfo *blocks, unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    if (!alignment) {
        ERROR_MESSAGE("TracebackGlobalAlignment() - NULL alignment handle");
        return STRUCT_DP_PARAMETER_ERROR;
    }
    *alignment = NULL;

    // find max score (e.g., best-scoring position of last block)
    int score = DP_NEGATIVE_INFINITY;
    unsigned int residue, lastBlockPos = 0;
    for (residue=queryFrom; residue<=queryTo; ++residue) {
        if (matrix[blocks->nBlocks - 1][residue - queryFrom].score > score) {
            score = matrix[blocks->nBlocks - 1][residue - queryFrom].score;
            lastBlockPos = residue;
        }
    }

    if (score == DP_NEGATIVE_INFINITY) {
        ERROR_MESSAGE("TracebackGlobalAlignment() - somehow failed to find any allowed global alignment");
        return STRUCT_DP_ALGORITHM_ERROR;
    }

//    INFO_MESSAGE("Score of best global alignment: " << score);
    *alignment = new DP_AlignmentResult;
    return TracebackAlignment(matrix, blocks->nBlocks - 1, lastBlockPos, queryFrom, *alignment);
}

int TracebackLocalAlignment(const Matrix& matrix,
    const DP_BlockInfo *blocks, unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    if (!alignment) {
        ERROR_MESSAGE("TracebackLocalAlignment() - NULL alignment handle");
        return STRUCT_DP_PARAMETER_ERROR;
    }
    *alignment = NULL;

    // find max score (e.g., best-scoring position of any block)
    int score = DP_NEGATIVE_INFINITY;
    unsigned int block, residue, lastBlock = 0, lastBlockPos = 0;
    for (block=0; block<blocks->nBlocks; ++block) {
        for (residue=queryFrom; residue<=queryTo; ++residue) {
            if (matrix[block][residue - queryFrom].score > score) {
                score = matrix[block][residue - queryFrom].score;
                lastBlock = block;
                lastBlockPos = residue;
            }
        }
    }

    if (score <= 0) {
//        INFO_MESSAGE("No positive-scoring local alignment found.");
        return STRUCT_DP_NO_ALIGNMENT;
    }

//    INFO_MESSAGE("Score of best local alignment: " << score);
    *alignment = new DP_AlignmentResult;
    return TracebackAlignment(matrix, lastBlock, lastBlockPos, queryFrom, *alignment);
}

typedef struct {
    unsigned int block;
    unsigned int residue;
} Traceback;

int TracebackMultipleLocalAlignments(const Matrix& matrix,
    const DP_BlockInfo *blocks, unsigned int queryFrom, unsigned int queryTo,
    DP_MultipleAlignmentResults **alignments, unsigned int maxAlignments)
{
    if (!alignments) {
        ERROR_MESSAGE("TracebackMultipleLocalAlignments() - NULL alignments handle");
        return STRUCT_DP_PARAMETER_ERROR;
    }
    *alignments = NULL;

    unsigned int block, residue;
    vector < vector < bool > > usedCells(blocks->nBlocks);
    for (block=0; block<blocks->nBlocks; ++block)
        usedCells[block].resize(queryTo - queryFrom + 1, false);

    // find N max scores
    list < Traceback > tracebacks;
    while (maxAlignments == 0 || tracebacks.size() < maxAlignments) {

        // find next best scoring cell that's not part of an alignment already reported
        int score = DP_NEGATIVE_INFINITY;
        unsigned int lastBlock = 0, lastBlockPos = 0;
        for (block=0; block<blocks->nBlocks; ++block) {
            for (residue=queryFrom; residue<=queryTo; ++residue) {
                if (!usedCells[block][residue - queryFrom] &&
                    matrix[block][residue - queryFrom].score > score)
                {
                    score = matrix[block][residue - queryFrom].score;
                    lastBlock = block;
                    lastBlockPos = residue;
                }
            }
        }
        if (score <= 0)
            break;

        // mark cells of this alignment as used, and see if any part of this alignment
        // has been reported before
        block = lastBlock;
        residue = lastBlockPos;
        bool repeated = false;
        do {
            if (usedCells[block][residue - queryFrom]) {
                repeated = true;
                break;
            } else {
                usedCells[block][residue - queryFrom] = true;
            }
            residue = matrix[block][residue - queryFrom].tracebackResidue;
            --block;
        } while (residue != NO_TRACEBACK);
        if (repeated)
            continue;

        // add traceback start to list
        tracebacks.resize(tracebacks.size() + 1);
        tracebacks.back().block = lastBlock;
        tracebacks.back().residue = lastBlockPos;
    }

    if (tracebacks.size() == 0) {
//        INFO_MESSAGE("No positive-scoring local alignment found.");
        return STRUCT_DP_NO_ALIGNMENT;
    }

    // allocate result structure
    *alignments = new DP_MultipleAlignmentResults;
    (*alignments)->nAlignments = 0;
    (*alignments)->alignments = new DP_AlignmentResult[tracebacks.size()];
    unsigned int a;
    for (a=0; a<tracebacks.size(); ++a)
        (*alignments)->alignments[a].blockPositions = NULL;

    // fill in results from tracebacks
    list < Traceback >::const_iterator t, te = tracebacks.end();
    for (t=tracebacks.begin(), a=0; t!=te; ++t, ++a) {
        if (TracebackAlignment(matrix, t->block, t->residue, queryFrom, &((*alignments)->alignments[a]))
                == STRUCT_DP_FOUND_ALIGNMENT)
        {
            ++((*alignments)->nAlignments);
//            if (a == 0)
//                INFO_MESSAGE("Score of best local alignment: " << (*alignments)->alignments[a].score);
//            else
//                INFO_MESSAGE("Score of next local alignment: " << (*alignments)->alignments[a].score);
        } else
            return STRUCT_DP_ALGORITHM_ERROR;
    }

    return STRUCT_DP_FOUND_ALIGNMENT;
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
    for (i=0; i<blocks->nBlocks; ++i)
        sumBlockLen += blocks->blockSizes[i];
    if (sumBlockLen > queryTo - queryFrom + 1) {
        ERROR_MESSAGE("DP_GlobalBlockAlign() - sum of block lengths longer than query region");
        return STRUCT_DP_PARAMETER_ERROR;
    }

    int status = ValidateFrozenBlockPositions(blocks, queryFrom, queryTo, true);
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

int DP_GlobalBlockAlignGeneric(
    const DP_BlockInfo *blocks,
    DP_BlockScoreFunction BlockScore, DP_LoopPenaltyFunction LoopScore,
    unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    if (!blocks || blocks->nBlocks < 1 || !blocks->blockSizes || queryTo < queryFrom ||
            !BlockScore || !LoopScore) {
        ERROR_MESSAGE("DP_GlobalBlockAlignGeneric() - invalid parameters");
        return STRUCT_DP_PARAMETER_ERROR;
    }

    unsigned int i, sumBlockLen = 0;
    for (i=0; i<blocks->nBlocks; ++i)
        sumBlockLen += blocks->blockSizes[i];
    if (sumBlockLen > queryTo - queryFrom + 1) {
        ERROR_MESSAGE("DP_GlobalBlockAlignGeneric() - sum of block lengths longer than query region");
        return STRUCT_DP_PARAMETER_ERROR;
    }

    int status = ValidateFrozenBlockPositions(blocks, queryFrom, queryTo, false);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_GlobalBlockAlignGeneric() - ValidateFrozenBlockPositions() returned error");
        return status;
    }

    Matrix matrix(blocks->nBlocks, queryTo - queryFrom + 1);

    status = CalculateGlobalMatrixGeneric(matrix, blocks, BlockScore, LoopScore, queryFrom, queryTo);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_GlobalBlockAlignGeneric() - CalculateGlobalMatrixGeneric() failed");
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
    for (unsigned int block=0; block<blocks->nBlocks; ++block) {
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

int DP_LocalBlockAlignGeneric(
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore, DP_LoopPenaltyFunction LoopScore,
    unsigned int queryFrom, unsigned int queryTo,
    DP_AlignmentResult **alignment)
{
    if (!blocks || blocks->nBlocks < 1 || !blocks->blockSizes || !BlockScore || queryTo < queryFrom) {
        ERROR_MESSAGE("DP_LocalBlockAlignGeneric() - invalid parameters");
        return STRUCT_DP_PARAMETER_ERROR;
    }
    for (unsigned int block=0; block<blocks->nBlocks; ++block) {
        if (blocks->freezeBlocks[block] != DP_UNFROZEN_BLOCK) {
            WARNING_MESSAGE("DP_LocalBlockAlignGeneric() - frozen block specifications are ignored...");
            break;
        }
    }

    Matrix matrix(blocks->nBlocks, queryTo - queryFrom + 1);

    int status = CalculateLocalMatrixGeneric(matrix, blocks, BlockScore, LoopScore, queryFrom, queryTo);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_LocalBlockAlignGeneric() - CalculateLocalMatrixGeneric() failed");
        return status;
    }

    return TracebackLocalAlignment(matrix, blocks, queryFrom, queryTo, alignment);
}

int DP_MultipleLocalBlockAlign(
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore,
    unsigned int queryFrom, unsigned int queryTo,
    DP_MultipleAlignmentResults **alignments, unsigned int maxAlignments)
{
    if (!blocks || blocks->nBlocks < 1 || !blocks->blockSizes || !BlockScore || queryTo < queryFrom) {
        ERROR_MESSAGE("DP_MultipleLocalBlockAlign() - invalid parameters");
        return STRUCT_DP_PARAMETER_ERROR;
    }
    for (unsigned int block=0; block<blocks->nBlocks; ++block) {
        if (blocks->freezeBlocks[block] != DP_UNFROZEN_BLOCK) {
            WARNING_MESSAGE("DP_MultipleLocalBlockAlign() - frozen block specifications are ignored...");
            break;
        }
    }

    Matrix matrix(blocks->nBlocks, queryTo - queryFrom + 1);

    int status = CalculateLocalMatrix(matrix, blocks, BlockScore, queryFrom, queryTo);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_MultipleLocalBlockAlign() - CalculateLocalMatrix() failed");
        return status;
    }

    return TracebackMultipleLocalAlignments(matrix, blocks, queryFrom, queryTo, alignments, maxAlignments);
}

int DP_MultipleLocalBlockAlignGeneric(
    const DP_BlockInfo *blocks, DP_BlockScoreFunction BlockScore, DP_LoopPenaltyFunction LoopScore,
    unsigned int queryFrom, unsigned int queryTo,
    DP_MultipleAlignmentResults **alignments, unsigned int maxAlignments)
{
    if (!blocks || blocks->nBlocks < 1 || !blocks->blockSizes || !BlockScore || queryTo < queryFrom) {
        ERROR_MESSAGE("DP_MultipleLocalBlockAlignGeneric() - invalid parameters");
        return STRUCT_DP_PARAMETER_ERROR;
    }
    for (unsigned int block=0; block<blocks->nBlocks; ++block) {
        if (blocks->freezeBlocks[block] != DP_UNFROZEN_BLOCK) {
            WARNING_MESSAGE("DP_MultipleLocalBlockAlignGeneric() - frozen block specifications are ignored...");
            break;
        }
    }

    Matrix matrix(blocks->nBlocks, queryTo - queryFrom + 1);

    int status = CalculateLocalMatrixGeneric(matrix, blocks, BlockScore, LoopScore, queryFrom, queryTo);
    if (status != STRUCT_DP_OKAY) {
        ERROR_MESSAGE("DP_MultipleLocalBlockAlignGeneric() - CalculateLocalMatrixGeneric() failed");
        return status;
    }

    return TracebackMultipleLocalAlignments(matrix, blocks, queryFrom, queryTo, alignments, maxAlignments);
}

DP_BlockInfo * DP_CreateBlockInfo(unsigned int nBlocks)
{
    DP_BlockInfo *blocks = new DP_BlockInfo;
    blocks->nBlocks = nBlocks;
    blocks->blockPositions = new unsigned int[nBlocks];
    blocks->blockSizes = new unsigned int[nBlocks];
    blocks->maxLoops = new unsigned int[nBlocks - 1];
    blocks->freezeBlocks = new unsigned int[nBlocks];
    for (unsigned int i=0; i<nBlocks; ++i)
        blocks->freezeBlocks[i] = DP_UNFROZEN_BLOCK;
    return blocks;
}

void DP_DestroyBlockInfo(DP_BlockInfo *blocks)
{
    if (!blocks)
        return;
    delete[] blocks->blockPositions;
    delete[] blocks->blockSizes;
    delete[] blocks->maxLoops;
    delete[] blocks->freezeBlocks;
    delete blocks;
}

void DP_DestroyAlignmentResult(DP_AlignmentResult *alignment)
{
    if (!alignment)
        return;
    delete[] alignment->blockPositions;
    delete alignment;
}

void DP_DestroyMultipleAlignmentResults(DP_MultipleAlignmentResults *alignments)
{
    if (!alignments) return;
    for (unsigned int i=0; i<alignments->nAlignments; ++i)
        if (alignments->alignments[i].blockPositions)
            delete[] alignments->alignments[i].blockPositions;
    delete[] alignments->alignments;
    delete alignments;
}

unsigned int DP_CalculateMaxLoopLength(
        unsigned int nLoops, const unsigned int *loops,
        double percentile, unsigned int extension, unsigned int cutoff)
{
    vector < unsigned int > loopLengths(nLoops);
    unsigned int index, max;
    for (index=0; index<nLoops; ++index)
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
* Revision 1.20  2004/05/21 21:41:04  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.19  2004/03/15 18:54:57  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.18  2004/02/19 02:21:19  thiessen
* fix struct_dp paths
*
* Revision 1.17  2003/12/19 14:37:50  thiessen
* add local generic loop function alignment routines
*
* Revision 1.16  2003/12/08 16:33:58  thiessen
* fix signed/unsigned mix
*
* Revision 1.15  2003/12/08 16:21:35  thiessen
* add generic loop scoring function interface
*
* Revision 1.14  2003/09/07 01:11:25  thiessen
* fix small memory leak
*
* Revision 1.13  2003/09/07 00:06:19  thiessen
* add multiple tracebacks for local alignments
*
* Revision 1.12  2003/08/23 22:10:05  thiessen
* fix local alignment block edge bug
*
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
