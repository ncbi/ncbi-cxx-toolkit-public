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

#include <algo/structure/struct_util/struct_util.hpp>
#include "su_private.hpp"
#include "su_sequence_set.hpp"
#include "su_alignment_set.hpp"
#include "su_block_multiple_alignment.hpp"

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
}

static bool AlignedToAllSlaves(unsigned int masterResidue, const AlignmentSet::AlignmentList& alignments)
{
    AlignmentSet::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; ++a) {
        if ((*a)->m_masterToSlave[masterResidue] == MasterSlaveAlignment::UNALIGNED)
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
    if (!m_alignmentSet)
        return false;
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

    // update all internal data according to this new alignment
    TRACE_MESSAGE("replacing alignment data with IBM'ed alignment");
    m_currentMultiple = multipleAlignment.Release();
    delete m_alignmentSet;
    m_seqAnnots.clear();
    m_alignmentSet = AlignmentSet::CreateFromMultiple(m_currentMultiple, &m_seqAnnots, *m_sequenceSet);
    return true;
}

END_SCOPE(struct_util)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/05/25 15:52:17  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/

