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

#include <algo/structure/struct_dp/struct_dp.h>

#include <algo/structure/struct_util/struct_util.hpp>
#include "su_private.hpp"
#include "su_sequence_set.hpp"
#include "su_alignment_set.hpp"
#include "su_block_multiple_alignment.hpp"

// for PSSM (BLAST_Matrix)
#include <blastkar.h>

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
    for (s=seqEntries.begin(); s!=se; s++) {
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

// implemented in su_block_multiple_alignment.cpp
extern int LookupBLASTResidueNumberFromCharacter(unsigned char r);

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
    for (unsigned i=0; i<g_dpBlocks->blockSizes[block]; ++i)
        score += g_dpPSSM->matrix[masterPos + i]
            [LookupBLASTResidueNumberFromCharacter(g_dpQuery->m_sequenceString[queryPos + i])];

    return score;
}
}

bool AlignmentUtility::DoLeaveOneOut(
    unsigned int rowToRealign, const std::vector < unsigned int >& blocksToRealign, // what to realign
    double percentile, unsigned int extension, unsigned int cutoff)                 // to calculate max loop lengths
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

    bool status = false;
    DP_BlockInfo *dpBlocks = NULL;
    DP_AlignmentResult *dpResult = NULL;

    bool prevState = CException::EnableBackgroundReporting(false);

    try {
        // pull out the row we're realigning
        if (rowToRealign < 1 || rowToRealign >= m_currentMultiple->NRows())
            THROW_MESSAGE("invalid row number");
        vector < unsigned int > rowsToRemove(1, rowToRealign);
        BlockMultipleAlignment::AlignmentList toRealign;
        TRACE_MESSAGE("extracting for realignment: "
            << m_currentMultiple->GetSequenceOfRow(rowToRealign)->IdentifierString());
        if (!m_currentMultiple->ExtractRows(rowsToRemove, &toRealign))
            THROW_MESSAGE("ExtractRows() failed");

        // fill out DP_BlockInfo structure
        dpBlocks = DP_CreateBlockInfo(blocks.size());
        unsigned int b, r;
        const Block::Range *range, *nextRange;
        unsigned int *loopLengths = new unsigned int[m_currentMultiple->NRows()];
        for (b=0; b<blocks.size(); ++b) {
            range = blocks[b]->GetRangeOfRow(0);
            dpBlocks->blockPositions[b] = range->from;
            dpBlocks->blockSizes[b] = range->to - range->from + 1;
            // calculate max loop lengths for default square well scoring
            if (b < blocks.size() - 1) {
                for (r=0; r<m_currentMultiple->NRows(); ++r) {
                    range = blocks[b]->GetRangeOfRow(r);
                    nextRange = blocks[b + 1]->GetRangeOfRow(r);
                    loopLengths[r] = nextRange->from - range->to - 1;
                }
                dpBlocks->maxLoops[b] = DP_CalculateMaxLoopLength(
                    m_currentMultiple->NRows(), loopLengths, percentile, extension, cutoff);
            }
        }
        delete[] loopLengths;

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
        toRealign.front()->GetUngappedAlignedBlocks(&blocks);
        for (b=0; b<blocks.size(); b++) {
            if (!realignBlock[b])
                dpBlocks->freezeBlocks[b] = blocks[b]->GetRangeOfRow(1)->from;
            TRACE_MESSAGE("block " << (b+1) << " is " << (realignBlock[b] ? "to be realigned" : "frozen"));
        }

        // set up PSSM
        g_dpPSSM = m_currentMultiple->GetPSSM();
        g_dpBlocks = dpBlocks;
        g_dpQuery = toRealign.front()->GetSequenceOfRow(1);

        // call the block aligner
        if (DP_GlobalBlockAlign(dpBlocks, ScoreByPSSM,
                                0, toRealign.front()->GetSequenceOfRow(1)->Length() - 1,    // realign on whole query
                                &dpResult)
                    != STRUCT_DP_FOUND_ALIGNMENT ||
                dpResult->nBlocks != blocks.size() ||
                dpResult->firstBlock != 0)
            THROW_MESSAGE("DP_GlobalBlockAlign() failed");
        INFO_MESSAGE("score of new alignment of extracted row with PSSM: " << dpResult->score);

        // adjust new alignment according to dp result
        BlockMultipleAlignment::ModifiableUngappedAlignedBlockList modBlocks;
        toRealign.front()->GetModifiableUngappedAlignedBlocks(&modBlocks);
        for (b=0; b<modBlocks.size(); b++) {
            if (realignBlock[b]) {
                modBlocks[b]->SetRangeOfRow(1,
                    dpResult->blockPositions[b], dpResult->blockPositions[b] + dpBlocks->blockSizes[b] - 1);
            } else {
                if (dpResult->blockPositions[b] != modBlocks[b]->GetRangeOfRow(1)->from)    // just to check...
                    THROW_MESSAGE("dpResult block doesn't match frozen position on slave");
            }
        }

        // merge realigned row back onto the end of the multiple alignment
        TRACE_MESSAGE("merging DP results back into multiple alignment");
        if (!m_currentMultiple->MergeAlignment(toRealign.front()))
            THROW_MESSAGE("MergeAlignment() failed");

        // move extracted row back to where it was
        vector < unsigned int > rowOrder(m_currentMultiple->NRows());
        for (r=0; r<m_currentMultiple->NRows(); r++) {
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

END_SCOPE(struct_util)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2004/05/28 10:22:13  thiessen
* use delete[] for array
*
* Revision 1.8  2004/05/28 09:46:57  thiessen
* restructure C-toolkit header usage ; move C Bioseq storage into su_sequence_set
*
* Revision 1.7  2004/05/27 21:34:08  thiessen
* add PSSM calculation (requires C-toolkit)
*
* Revision 1.6  2004/05/26 14:49:59  thiessen
* UNDEFINED -> eUndefined
*
* Revision 1.5  2004/05/26 14:45:11  gorelenk
* UNALIGNED->eUnaligned
*
* Revision 1.4  2004/05/26 14:30:16  thiessen
* adjust handling of alingment data ; add row ordering
*
* Revision 1.3  2004/05/26 02:40:24  thiessen
* progress towards LOO - all but PSSM and row ordering
*
* Revision 1.2  2004/05/25 15:52:17  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/

