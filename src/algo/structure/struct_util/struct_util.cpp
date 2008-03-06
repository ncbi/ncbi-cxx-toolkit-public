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
*      AlignmentUtility class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>

#include <set>

#include <algo/structure/struct_util/struct_util.hpp>
#include <algo/structure/struct_util/su_sequence_set.hpp>
#include <algo/structure/struct_util/su_block_multiple_alignment.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include "su_private.hpp"
#include "su_alignment_set.hpp"
#include <algo/structure/struct_util/su_pssm.hpp>

#include <algo/structure/struct_dp/struct_dp.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(struct_util)

AlignmentUtility::AlignmentUtility(const CSeq_entry& seqEntry, const SeqAnnotList& seqAnnots)
{
    CRef < CSeq_entry > seqEntryCopy(new CSeq_entry());
    seqEntryCopy->Assign(seqEntry);
    m_seqEntries.push_back(seqEntryCopy);

    SeqAnnotList::const_iterator a, ae = seqAnnots.end();
    for (a=seqAnnots.begin(); a!=ae; ++a) {
        CRef < CSeq_annot > seqAnnotCopy(new CSeq_annot());
        seqAnnotCopy->Assign(**a);
        m_seqAnnots.push_back(seqAnnotCopy);
    }

    Init();
}

AlignmentUtility::AlignmentUtility(const SeqEntryList& seqEntries, const SeqAnnotList& seqAnnots)
{
    SeqEntryList::const_iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; ++s) {
        CRef < CSeq_entry > seqEntryCopy(new CSeq_entry());
        seqEntryCopy->Assign(**s);
        m_seqEntries.push_back(seqEntryCopy);
    }

    SeqAnnotList::const_iterator a, ae = seqAnnots.end();
    for (a=seqAnnots.begin(); a!=ae; ++a) {
        CRef < CSeq_annot > seqAnnotCopy(new CSeq_annot());
        seqAnnotCopy->Assign(**a);
        m_seqAnnots.push_back(seqAnnotCopy);
    }

    Init();
}

void AlignmentUtility::Init(void)
{
    m_sequenceSet = NULL;
    m_alignmentSet = NULL;
    m_okay = true;
    m_currentMultiple = NULL;

    bool prevState = CException::EnableBackgroundReporting(false);

    try {
        m_sequenceSet = new SequenceSet(m_seqEntries);
        m_alignmentSet = new AlignmentSet(m_seqAnnots, *m_sequenceSet);
    } catch (CException& e) {
        ERROR_MESSAGE("exception during AlignmentUtility initialization: " << e.GetMsg());
        m_okay = false;
    }

    CException::EnableBackgroundReporting(prevState);
}

AlignmentUtility::~AlignmentUtility()
{
    if (m_sequenceSet)
        delete m_sequenceSet;
    if (m_alignmentSet)
        delete m_alignmentSet;
    if (m_currentMultiple)
        delete m_currentMultiple;
}

AlignmentUtility* AlignmentUtility::Clone() const {
    const SeqAnnotList& seqAnnots = const_cast<AlignmentUtility*>(this)->GetSeqAnnots();
    return new AlignmentUtility(m_seqEntries, seqAnnots);
}

static bool AlignedToAllSlaves(unsigned int masterResidue, const AlignmentSet::AlignmentList& alignments)
{
    AlignmentSet::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; ++a) {
        if ((*a)->m_masterToSlave[masterResidue] == MasterSlaveAlignment::eUnaligned)
            return false;
    }
    return true;
}

static bool NoSlaveInsertionsBetween(unsigned int masterFrom, unsigned int masterTo,
    const AlignmentSet::AlignmentList& alignments)
{
    AlignmentSet::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; ++a) {
        if (((*a)->m_masterToSlave[masterTo] - (*a)->m_masterToSlave[masterFrom]) != (masterTo - masterFrom))
            return false;
    }
    return true;
}

static bool NoBlockBoundariesBetween(unsigned int masterFrom, unsigned int masterTo,
    const AlignmentSet::AlignmentList& alignments)
{
    AlignmentSet::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; ++a) {
        if ((*a)->m_blockStructure[masterTo] != (*a)->m_blockStructure[masterFrom])
            return false;
    }
    return true;
}

bool AlignmentUtility::DoIBM(void)
{
    if (m_currentMultiple) {
        WARNING_MESSAGE("DoIBM() - already have a blocked alignment");
        return true;
    }
    if (!m_alignmentSet || m_seqAnnots.size() == 0) {
        ERROR_MESSAGE("DoIBM() - no alignment data present");
        return false;
    }

    TRACE_MESSAGE("doing IBM");
    const AlignmentSet::AlignmentList& alignments = m_alignmentSet->m_alignments;
    AlignmentSet::AlignmentList::const_iterator a, ae = alignments.end();

    // create sequence list; fill with sequences of master + slaves
    BlockMultipleAlignment::SequenceList sequenceList(alignments.size() + 1);
    BlockMultipleAlignment::SequenceList::iterator s = sequenceList.begin();
    *(s++) = alignments.front()->m_master;
    for (a=alignments.begin(); a!=ae; ++a) {
        *(s++) = (*a)->m_slave;
        if ((*a)->m_master != sequenceList.front()) {
            ERROR_MESSAGE("AlignmentUtility::DoIBM() - all pairwise alignments must have the same master sequence");
            return false;
        }
    }
    CRef < BlockMultipleAlignment > multipleAlignment(new BlockMultipleAlignment(sequenceList));

    // each block is a continuous region on the master, over which each master
    // residue is aligned to a residue of each slave, and where there are no
    // insertions relative to the master in any of the slaves
    unsigned int masterFrom = 0, masterTo, row;
    UngappedAlignedBlock *newBlock;

    while (masterFrom < multipleAlignment->GetMaster()->Length()) {

        // look for first all-aligned residue
        if (!AlignedToAllSlaves(masterFrom, alignments)) {
            ++masterFrom;
            continue;
        }

        // find all next continuous all-aligned residues, but checking for
        // block boundaries from the original master-slave pairs, so that
        // blocks don't get merged
        for (masterTo=masterFrom+1;
                masterTo < multipleAlignment->GetMaster()->Length() &&
                AlignedToAllSlaves(masterTo, alignments) &&
                NoSlaveInsertionsBetween(masterFrom, masterTo, alignments) &&
                NoBlockBoundariesBetween(masterFrom, masterTo, alignments);
             ++masterTo) ;
        --masterTo; // after loop, masterTo = first residue past block

        // create new block with ranges from master and all slaves
        newBlock = new UngappedAlignedBlock(multipleAlignment);
        newBlock->SetRangeOfRow(0, masterFrom, masterTo);
        newBlock->m_width = masterTo - masterFrom + 1;

        for (a=alignments.begin(), row=1; a!=ae; ++a, ++row) {
            newBlock->SetRangeOfRow(row,
                (*a)->m_masterToSlave[masterFrom],
                (*a)->m_masterToSlave[masterTo]);
        }

        // copy new block into alignment
        multipleAlignment->AddAlignedBlockAtEnd(newBlock);

        // start looking for next block
        masterFrom = masterTo + 1;
    }

    if (!multipleAlignment->AddUnalignedBlocks() || !multipleAlignment->UpdateBlockMap()) {
        ERROR_MESSAGE("AlignmentUtility::DoIBM() - error finalizing alignment");
        return false;
    }

    // switch data to the new multiple alignment
    m_currentMultiple = multipleAlignment.Release();
    RemoveAlignAnnot();
    return true;
}

// global stuff for DP block aligner score callback
DP_BlockInfo *g_dpBlocks = NULL;
const BLAST_Matrix *g_dpPSSM = NULL;
const Sequence *g_dpQuery = NULL;

// sum of scores for residue vs. PSSM
extern "C" {
int ScoreByPSSM(unsigned int block, unsigned int queryPos)
{
    if (!g_dpBlocks || !g_dpPSSM || !g_dpQuery || block >= g_dpBlocks->nBlocks ||
        queryPos > g_dpQuery->Length() - g_dpBlocks->blockSizes[block])
    {
        ERROR_MESSAGE("ScoreByPSSM() - bad parameters");
        return DP_NEGATIVE_INFINITY;
    }

    int masterPos = g_dpBlocks->blockPositions[block], score = 0;
    for (unsigned int i=0; i<g_dpBlocks->blockSizes[block]; ++i)
        score += GetPSSMScoreOfCharWithAverageOfBZ(g_dpPSSM, masterPos + i, g_dpQuery->m_sequenceString[queryPos + i]);

    return score;
}
}

bool AlignmentUtility::DoLeaveOneOut(
    unsigned int rowToRealign, const std::vector < unsigned int >& blocksToRealign, // what to realign
    double percentile, unsigned int extension, unsigned int cutoff,                 // to calculate max loop lengths
    unsigned int queryFrom, unsigned int queryTo)                                   // range on realigned row to search
{
    // first we need to do IBM -> BlockMultipleAlignment
    if (!m_currentMultiple && !DoIBM())
            return false;

    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    m_currentMultiple->GetUngappedAlignedBlocks(&blocks);
    if (blocks.size() == 0) {
        ERROR_MESSAGE("need at least one aligned block for LOO");
        return false;
    }

    BlockMultipleAlignment::AlignmentList toRealign;

    bool status = false;
    DP_BlockInfo *dpBlocks = NULL;
    DP_AlignmentResult *dpResult = NULL;

    bool prevState = CException::EnableBackgroundReporting(false);

    try {
        // first calculate max loop lengths for default square well scoring; include the row to be realigned,
        // in case it contains an unusually long loop that we don't want to disallow (still depends on loop
        // calculation params, though)
        dpBlocks = DP_CreateBlockInfo(blocks.size());
        unsigned int b, r;
        const Block::Range *range, *nextRange;
        unsigned int *loopLengths = new unsigned int[m_currentMultiple->NRows()];
        for (b=0; b<blocks.size()-1; ++b) {
            for (r=0; r<m_currentMultiple->NRows(); ++r) {
                range = blocks[b]->GetRangeOfRow(r);
                nextRange = blocks[b + 1]->GetRangeOfRow(r);
                loopLengths[r] = nextRange->from - range->to - 1;
            }
            dpBlocks->maxLoops[b] = DP_CalculateMaxLoopLength(
                m_currentMultiple->NRows(), loopLengths, percentile, extension, cutoff);
        }
        delete[] loopLengths;

        // now pull out the row we're realigning
        if (rowToRealign < 1 || rowToRealign >= m_currentMultiple->NRows())
            THROW_MESSAGE("invalid row number");
        vector < unsigned int > rowsToRemove(1, rowToRealign);
        TRACE_MESSAGE("extracting for realignment: "
            << m_currentMultiple->GetSequenceOfRow(rowToRealign)->IdentifierString());
        if (!m_currentMultiple->ExtractRows(rowsToRemove, &toRealign))
            THROW_MESSAGE("ExtractRows() failed");

        // fill out the rest of the DP_BlockInfo structure
        for (b=0; b<blocks.size(); ++b) {
            range = blocks[b]->GetRangeOfRow(0);
            dpBlocks->blockPositions[b] = range->from;
            dpBlocks->blockSizes[b] = range->to - range->from + 1;
        }

        // if we're not realigning, freeze blocks to original slave position
        if (blocksToRealign.size() == 0)
            THROW_MESSAGE("need at least one block to realign");
        vector < bool > realignBlock(blocks.size(), false);
        for (r=0; r<blocksToRealign.size(); ++r) {
            if (blocksToRealign[r] < blocks.size())
                realignBlock[blocksToRealign[r]] = true;
            else
                THROW_MESSAGE("block to realign is out of range");
        }
        toRealign.front()->GetUngappedAlignedBlocks(&blocks);
        for (b=0; b<blocks.size(); ++b) {
            if (!realignBlock[b])
                dpBlocks->freezeBlocks[b] = blocks[b]->GetRangeOfRow(1)->from;
//            TRACE_MESSAGE("block " << (b+1) << " is " << (realignBlock[b] ? "to be realigned" : "frozen"));
        }

        // verify query range
        r = toRealign.front()->GetSequenceOfRow(1)->Length();
        if (queryTo == kMax_UInt)
            queryTo = r - 1;
        if (queryFrom >= r || queryTo >= r || queryFrom > queryTo)
            THROW_MESSAGE("bad queryFrom/To range");
        TRACE_MESSAGE("queryFrom " << queryFrom << ", queryTo " << queryTo);

        // set up PSSM
        g_dpPSSM = m_currentMultiple->GetPSSM();
        g_dpBlocks = dpBlocks;
        g_dpQuery = toRealign.front()->GetSequenceOfRow(1);

        // show score before alignment
        int originalScore = 0;
        BlockMultipleAlignment::UngappedAlignedBlockList ob;
        toRealign.front()->GetUngappedAlignedBlocks(&ob);
        BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator l, o, le = blocks.end();
        for (l=blocks.begin(), o=ob.begin(); l!=le; ++l, ++o) {
            const Block::Range
                *masterRange = (*l)->GetRangeOfRow(0),
                *range = (*o)->GetRangeOfRow(1);
            for (unsigned int i=0; i<(*l)->m_width; ++i)
                originalScore += GetPSSMScoreOfCharWithAverageOfBZ(
                    m_currentMultiple->GetPSSM(), masterRange->from + i, g_dpQuery->m_sequenceString[range->from + i]);
        }
        INFO_MESSAGE("score of extracted row with PSSM(N-1) before realignment: " << originalScore);

        // call the block aligner
        if (DP_GlobalBlockAlign(dpBlocks, ScoreByPSSM, queryFrom, queryTo, &dpResult) != STRUCT_DP_FOUND_ALIGNMENT ||
                dpResult->nBlocks != blocks.size() || dpResult->firstBlock != 0)
            THROW_MESSAGE("DP_GlobalBlockAlign() failed");
        INFO_MESSAGE("score of new alignment of extracted row with PSSM(N-1): " << dpResult->score);

        // adjust new alignment according to dp result
        BlockMultipleAlignment::ModifiableUngappedAlignedBlockList modBlocks;
        toRealign.front()->GetModifiableUngappedAlignedBlocks(&modBlocks);
        for (b=0; b<modBlocks.size(); ++b) {
            if (realignBlock[b]) {
                modBlocks[b]->SetRangeOfRow(1,
                    dpResult->blockPositions[b], dpResult->blockPositions[b] + dpBlocks->blockSizes[b] - 1);
            } else {
                if ((int)dpResult->blockPositions[b] != modBlocks[b]->GetRangeOfRow(1)->from)    // just to check...
                    THROW_MESSAGE("dpResult block doesn't match frozen position on slave");
            }
        }

        // merge realigned row back onto the end of the multiple alignment
        TRACE_MESSAGE("merging DP results back into multiple alignment");
        if (!m_currentMultiple->MergeAlignment(toRealign.front()))
            THROW_MESSAGE("MergeAlignment() failed");

        // move extracted row back to where it was
        vector < unsigned int > rowOrder(m_currentMultiple->NRows());
        for (r=0; r<m_currentMultiple->NRows(); ++r) {
            if (r < rowToRealign)
                rowOrder[r] = r;
            else if (r == rowToRealign)
                rowOrder[r] = m_currentMultiple->NRows() - 1;
            else
                rowOrder[r] = r - 1;
        }
        if (!m_currentMultiple->ReorderRows(rowOrder))
            THROW_MESSAGE("ReorderRows() failed");

        // remove other alignment data, since it no longer matches the multiple
        RemoveAlignAnnot();

        status = true;

    } catch (CException& e) {
        ERROR_MESSAGE("DoLeaveOneOut(): exception: " << e.GetMsg());
    }

    CException::EnableBackgroundReporting(prevState);

    DELETE_ALL_AND_CLEAR(toRealign, BlockMultipleAlignment::AlignmentList);

    DP_DestroyBlockInfo(dpBlocks);
    DP_DestroyAlignmentResult(dpResult);

    g_dpPSSM = NULL;
    g_dpBlocks = NULL;
    g_dpQuery = NULL;

    return status;
}


bool AlignmentUtility::DoLeaveNOut(
    std::vector<unsigned int>& rowsToRealign, const std::vector < unsigned int >& blocksToRealign, // what to realign
    double percentile, unsigned int extension, unsigned int cutoff,                 // to calculate max loop lengths
    std::vector<unsigned int>& queryFromVec, std::vector<unsigned int>& queryToVec)                                   // range on realigned row to search
{
    static const unsigned int LNO_MIN_NROWS_REMAINING = 3;

    unsigned int nRows, row;
    unsigned int queryFrom, queryTo;
    unsigned int nToRealign = rowsToRealign.size(), nFailed = 0;
    set<unsigned int> failed;
    string failedMsg, rowStr;

    // first we need to do IBM -> BlockMultipleAlignment
    if (!m_currentMultiple && !DoIBM())
            return false;

    // Set is used only to remove duplicates; since we don't
    // recompute PSSM between block aligner calls, the order the rows
    // are left out is not important when put back into 'sortedRowsToRealign'.
    unsigned int nDifferentToRealign;
    vector<unsigned int> sortedRowsToRealign;
    set<unsigned int> setOfRows;
    setOfRows.insert(rowsToRealign.begin(), rowsToRealign.end());      
    set<unsigned int>::iterator setIt, setEnd = setOfRows.end();

    nRows = m_currentMultiple->NRows();
    nDifferentToRealign = setOfRows.size();
    if (nToRealign != nDifferentToRealign) {
        WARNING_MESSAGE("Duplicate rows found among list of rows to leave out skipped\n(repeated applications of LNO on a single row has no affect)");
//        return false;
    }

    // now pull out the valid rows we can realigning
    for (setIt = setOfRows.begin(); setIt != setEnd; ++setIt) {
        if (*setIt < 1 || *setIt >= nRows) {
            WARNING_MESSAGE("Skipping invalid row number " << *setIt);
        } else {
            sortedRowsToRealign.push_back(*setIt);
            TRACE_MESSAGE("extracting for realignment: "
                          << m_currentMultiple->GetSequenceOfRow(*setIt)->IdentifierString());
        }

    }
    nDifferentToRealign = sortedRowsToRealign.size();


    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    m_currentMultiple->GetUngappedAlignedBlocks(&blocks);

//    if (nToRealign > nRows - LNO_MIN_NROWS_REMAINING) {
    if (nDifferentToRealign > nRows - LNO_MIN_NROWS_REMAINING) {
        ERROR_MESSAGE("need at least " << nRows - LNO_MIN_NROWS_REMAINING << " alignment rows to leave out " << nDifferentToRealign << " different rows.");
        return false;
    }
    if (blocks.size() == 0) {
        ERROR_MESSAGE("need at least one aligned block for LNO");
        return false;
    }
    if (nToRealign != queryFromVec.size() || nToRealign != queryToVec.size()) {
        ERROR_MESSAGE("inconsistent sizes between row list and query terminii lists for LNO");
        return false;
    }

    BlockMultipleAlignment::AlignmentList toRealign;

    bool status = false;
    DP_BlockInfo *dpBlocks = NULL;
    DP_AlignmentResult *dpResult = NULL;

    bool prevState = CException::EnableBackgroundReporting(false);

    try {
        // first calculate max loop lengths for default square well scoring; include the row to be realigned,
        // in case it contains an unusually long loop that we don't want to disallow (still depends on loop
        // calculation params, though)
        dpBlocks = DP_CreateBlockInfo(blocks.size());
        unsigned int b, r, i;
        const Block::Range *range, *nextRange;
        unsigned int *loopLengths = new unsigned int[nRows];
        for (b=0; b<blocks.size()-1; ++b) {
            for (r=0; r<nRows; ++r) {
                range = blocks[b]->GetRangeOfRow(r);
                nextRange = blocks[b + 1]->GetRangeOfRow(r);
                loopLengths[r] = nextRange->from - range->to - 1;
            }
            dpBlocks->maxLoops[b] = DP_CalculateMaxLoopLength(
                nRows, loopLengths, percentile, extension, cutoff);
        }
        delete[] loopLengths;

        //  Extract rows in decreasing row-number order.
        BlockMultipleAlignment::AlignmentList::iterator toRealignIt, toRealignEnd;
        if (!m_currentMultiple->ExtractRows(sortedRowsToRealign, &toRealign))
            THROW_MESSAGE("ExtractRows() failed");
        if (nDifferentToRealign != toRealign.size()) {
            THROW_MESSAGE("Not as many entries in toRealign as number of different rows to realign");
        }

        // fill out the rest of the DP_BlockInfo structure
        for (b=0; b<blocks.size(); ++b) {
            range = blocks[b]->GetRangeOfRow(0);
            dpBlocks->blockPositions[b] = range->from;
            dpBlocks->blockSizes[b] = range->to - range->from + 1;
        }

        // if we're not realigning, freeze blocks to original slave position
        if (blocksToRealign.size() == 0)
            THROW_MESSAGE("need at least one block to realign");

        vector < bool > realignBlock(blocks.size(), false);
        for (r=0; r<blocksToRealign.size(); ++r) {
            if (blocksToRealign[r] >= 0 && blocksToRealign[r] < blocks.size())
                realignBlock[blocksToRealign[r]] = true;
            else
                THROW_MESSAGE("block to realign is out of range");
        }

        //  Block-align each of the extracted rows.
        toRealignIt = toRealign.begin();
        toRealignEnd = toRealign.end();
        for (i=0; i < nDifferentToRealign; ++i, ++toRealignIt) {
            row = sortedRowsToRealign[i];
            rowStr = NStr::UIntToString(row) + " (" + GetSeqIdStringForRow(row) + ")\n";

            //  find correct from/to in the original unsorted vectors
            r = 0;
            while (r < nToRealign && rowsToRealign[r] != row) {
                ++r;
            }
            if (r < nToRealign) {
                queryFrom = queryFromVec[r];
                queryTo   = queryToVec[r];
            } else {  //  should never happen...
                ++nFailed;
                failed.insert(row);
                failedMsg.append("in original sorted list could not find expected row " + rowStr);
                continue;
            }

            (*toRealignIt)->GetUngappedAlignedBlocks(&blocks);
            for (b=0; b<blocks.size(); ++b) {
                if (!realignBlock[b])
                    dpBlocks->freezeBlocks[b] = blocks[b]->GetRangeOfRow(1)->from;
//                TRACE_MESSAGE("block " << (b+1) << " is " << (realignBlock[b] ? "to be realigned" : "frozen"));
            }

            // verify query range
            r = (*toRealignIt)->GetSequenceOfRow(1)->Length();
            if (queryTo == kMax_UInt)
                queryTo = r - 1;
            if (queryFrom >= r || queryTo >= r || queryFrom > queryTo) {
                ++nFailed;
                failed.insert(row);
                failedMsg.append("bad queryFrom/To range for row " + rowStr);
                continue;
            }
            TRACE_MESSAGE("row " << row << " length = " << r << ";  queryFrom " << queryFrom << ", queryTo " << queryTo);

            // set up PSSM
            g_dpPSSM = m_currentMultiple->GetPSSM();
            g_dpBlocks = dpBlocks;
            g_dpQuery = (*toRealignIt)->GetSequenceOfRow(1);

            // show score before alignment
            int originalScore = 0;
            BlockMultipleAlignment::UngappedAlignedBlockList ob;
            (*toRealignIt)->GetUngappedAlignedBlocks(&ob);
            BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator l, o, le = blocks.end();
            for (l=blocks.begin(), o=ob.begin(); l!=le; ++l, ++o) {
                const Block::Range
                    *masterRange = (*l)->GetRangeOfRow(0),
                    *range = (*o)->GetRangeOfRow(1);
                for (unsigned int i=0; i<(*l)->m_width; ++i)
                    originalScore += GetPSSMScoreOfCharWithAverageOfBZ(
                        m_currentMultiple->GetPSSM(), masterRange->from + i, g_dpQuery->m_sequenceString[range->from + i]);
            }
            INFO_MESSAGE("score of extracted row with PSSM(N-" << nDifferentToRealign << ") before realignment: " << originalScore);

            // call the block aligner
            if (DP_GlobalBlockAlign(dpBlocks, ScoreByPSSM, queryFrom, queryTo, &dpResult) != STRUCT_DP_FOUND_ALIGNMENT ||
                dpResult->nBlocks != blocks.size() || dpResult->firstBlock != 0) {
                ++nFailed;
                failed.insert(row);
                failedMsg.append("DP_GlobalBlockAlign() failed for row " + rowStr);
            } else {

                INFO_MESSAGE("score of new alignment of extracted row with PSSM(N-" << nDifferentToRealign << "): " << dpResult->score);

                // Adjust new alignment according to dp result; first check if any of the
                // blocks will present a problem so we don't modify N-terminal blocks if an
                // error is encountered in a more C-terminal block.
                BlockMultipleAlignment::ModifiableUngappedAlignedBlockList modBlocks;
                (*toRealignIt)->GetModifiableUngappedAlignedBlocks(&modBlocks);
                for (b=0; b<modBlocks.size(); ++b) {
                    if (!realignBlock[b]) {
                        // just to check...
                        if ((int)dpResult->blockPositions[b] != modBlocks[b]->GetRangeOfRow(1)->from) {
                            ++nFailed;
                            failed.insert(row);
                            failedMsg.append("dpResult block doesn't match frozen position on slave for row " + rowStr);
                            break;
                        }
                    }
                }
                if (failed.count(row) == 0) {
                    for (b=0; b<modBlocks.size(); ++b) {
                        if (realignBlock[b]) {
                            modBlocks[b]->SetRangeOfRow(1,
                                                        dpResult->blockPositions[b], dpResult->blockPositions[b] + dpBlocks->blockSizes[b] - 1);

                        }
                    }
                }
            }

            DP_DestroyAlignmentResult(dpResult);
            dpResult = NULL;
        }


        //  The toRealign container will hold the orignial, unmodified alignment
        //  obtained by ExtractRows above.
        if (nFailed > 0) {
            ERR_POST(ncbi::Error << "Encountered problems realigning some rows; those implicated are unchanged:\n       " + failedMsg + "\n\n");
        }

        // merge realigned rows back onto the end of the multiple alignment,
        // in order of increasing row number
        TRACE_MESSAGE("merging DP results back into multiple alignment");
        toRealignIt = toRealign.end();
        --toRealignIt;

        //  Failures here are fatal as the re-aligned sequence cannot be replaced
        //  back in its original location in the alignment.
        for (int j=nDifferentToRealign-1; j >=0; --j, --toRealignIt) {
            row = sortedRowsToRealign[j];
            rowStr = NStr::UIntToString(row) + " (" + GetSeqIdStringForRow(row) + ")";
            if (!m_currentMultiple->MergeAlignment(*toRealignIt))
                THROW_MESSAGE("MergeAlignment() failed for row " + rowStr);

            // move extracted row back to where it was
            vector < unsigned int > rowOrder(m_currentMultiple->NRows());
            for (r=0; r<m_currentMultiple->NRows(); ++r) {
                if (r < row) 
                    rowOrder[r] = r;
                else if (r == row)
                    rowOrder[r] = m_currentMultiple->NRows() - 1;
                else
                    rowOrder[r] = r - 1;
            }
            if (!m_currentMultiple->ReorderRows(rowOrder)) 
                THROW_MESSAGE("ReorderRows() failed when reinserting row " + rowStr);
        }

        // remove other alignment data, since it no longer matches the multiple
        RemoveAlignAnnot();

        status = true;

    } catch (CException& e) {
        ERROR_MESSAGE("DoLeaveNOut(): exception: " << e.GetMsg());
    }

    CException::EnableBackgroundReporting(prevState);

    DELETE_ALL_AND_CLEAR(toRealign, BlockMultipleAlignment::AlignmentList);

    DP_DestroyBlockInfo(dpBlocks);
    DP_DestroyAlignmentResult(dpResult);

    g_dpPSSM = NULL;
    g_dpBlocks = NULL;
    g_dpQuery = NULL;

    return status;
}

void AlignmentUtility::RemoveMultiple(void)
{
    if (m_currentMultiple) {
        delete m_currentMultiple;
        m_currentMultiple = NULL;
    }
}

void AlignmentUtility::RemoveAlignAnnot(void)
{
    m_seqAnnots.clear();
    if (m_alignmentSet) {
        delete m_alignmentSet;
        m_alignmentSet = NULL;
    }
}

const AlignmentUtility::SeqAnnotList& AlignmentUtility::GetSeqAnnots(void)
{
    if (!m_alignmentSet || m_seqAnnots.size() == 0) {
        if (m_alignmentSet)
            ERROR_MESSAGE("ack - shouldn't have m_alignmentSet but empty m_seqAnnots");
        m_alignmentSet = AlignmentSet::CreateFromMultiple(
            m_currentMultiple, &m_seqAnnots, *m_sequenceSet);
    }
    return m_seqAnnots;
}

const BLAST_Matrix * AlignmentUtility::GetPSSM(void)
{
    // first we need to do IBM -> BlockMultipleAlignment
    if (!m_currentMultiple && !DoIBM())
            return NULL;

	return m_currentMultiple->GetPSSM();
}

const BlockMultipleAlignment * AlignmentUtility::GetBlockMultipleAlignment(void)
{
    // first we need to do IBM -> BlockMultipleAlignment
    if (!m_currentMultiple && !DoIBM())
            return NULL;

    return m_currentMultiple;
}

void AlignmentUtility::SetBlockMultipleAlignment(const BlockMultipleAlignment* bma) {
    if (bma) {
        RemoveMultiple();
        RemoveAlignAnnot();
        m_currentMultiple = bma->Clone();
    }
}

int AlignmentUtility::ScoreRowByPSSM(unsigned int row)
{
    // first we need to do IBM -> BlockMultipleAlignment
    if (!m_currentMultiple && !DoIBM())
            return kMin_Int;

    if (row >= m_currentMultiple->NRows()) {
        ERROR_MESSAGE("AlignmentUtility::ScoreRowByPSSM() - row out of range");
        return kMin_Int;
    }

    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    m_currentMultiple->GetUngappedAlignedBlocks(&blocks);
    if (blocks.size() == 0) {
        WARNING_MESSAGE("AlignmentUtility::ScoreRowByPSSM() - alignment has no blocks");
        return kMin_Int;
    }

    const Sequence *sequence = m_currentMultiple->GetSequenceOfRow(row);

    int score = 0;
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        const Block::Range
            *masterRange = (*b)->GetRangeOfRow(0),
            *range = (*b)->GetRangeOfRow(row);
        for (unsigned int i=0; i<(*b)->m_width; ++i)
            score += GetPSSMScoreOfCharWithAverageOfBZ(
                m_currentMultiple->GetPSSM(), masterRange->from + i, sequence->m_sequenceString[range->from + i]);
    }

    return score;
}

unsigned int AlignmentUtility::GetNRows() {
    const BlockMultipleAlignment* bma = GetBlockMultipleAlignment();
    return (bma) ? bma->NRows() : 0;
}

unsigned int AlignmentUtility::GetNRows() const {
    unsigned int nRows = 0;
    if (m_currentMultiple) {
        nRows = m_currentMultiple->NRows();
    } else {
        AlignmentUtility* dummy = Clone();
        nRows = (dummy) ? dummy->GetNRows() : 0;
        delete dummy;
    }
    return nRows;
}

bool AlignmentUtility::IsRowPDB(unsigned int row) {
    bool result = false;
    const BlockMultipleAlignment* bma = GetBlockMultipleAlignment();
    const Sequence* sequence = (bma) ? bma->GetSequenceOfRow(row) : NULL;

    if (sequence) {
        result = sequence->GetPreferredIdentifier().IsPdb();
    }

    return result;
}

bool AlignmentUtility::IsRowPDB(unsigned int row) const {
    bool result = false;
    if (m_currentMultiple) {
        const Sequence* sequence = m_currentMultiple->GetSequenceOfRow(row);
        if (sequence) {
            result = sequence->GetPreferredIdentifier().IsPdb();
        }
    } else {
        AlignmentUtility* dummy = Clone();
        result = (dummy && dummy->IsRowPDB(row));
    }
    return result;
}

string SequenceIdToString(const Sequence& sequence) {

    static const string defStr = "Non-GI/PDB Sequence Type";
    string s = defStr;
    const CSeq_id& id = sequence.GetPreferredIdentifier();

    if (id.IsPdb()) {
        char chain = id.GetPdb().GetChain();
        s = "PDB " + id.GetPdb().GetMol().Get() + '_' + chain;
    } else if (id.IsGi()) {
        s = "GI " + NStr::IntToString(id.GetGi());
    }
    return s;
}

string AlignmentUtility::GetSeqIdStringForRow(unsigned int row) {

    static const string defStr = "<Could not find a sequence for row ";
    string idStr = defStr + NStr::IntToString(row + 1) + '>';
    const BlockMultipleAlignment* bma = GetBlockMultipleAlignment();
    const Sequence* sequence = (bma) ? bma->GetSequenceOfRow(row) : NULL;

    if (sequence) {
        idStr = SequenceIdToString(*sequence);
    }
    return idStr;
}

string AlignmentUtility::GetSeqIdStringForRow(unsigned int row) const {
    static const string defStr = "<Could not find a sequence for row ";
    string idStr = defStr + NStr::IntToString(row + 1) + '>';
    if (m_currentMultiple) {
        const Sequence* sequence = m_currentMultiple->GetSequenceOfRow(row);
        if (sequence) {
            idStr = SequenceIdToString(*sequence);
        }
    } else {
        AlignmentUtility* dummy = Clone();
        if (dummy) {
            idStr = dummy->GetSeqIdStringForRow(row);
        }
    }
    return idStr;
}

END_SCOPE(struct_util)
