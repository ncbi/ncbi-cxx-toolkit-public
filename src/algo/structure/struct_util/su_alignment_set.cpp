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
*      Classes to hold sets of alignments
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbi_limits.hpp>

#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <map>
#include <memory>

#include "su_alignment_set.hpp"
#include "su_sequence_set.hpp"
#include "su_block_multiple_alignment.hpp"
#include "su_private.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(struct_util)

AlignmentSet::AlignmentSet(const SeqAnnotList& seqAnnots, const SequenceSet& sequenceSet)
{
    // screen alignments to make sure they're a type we can handle
    typedef list < CRef < CSeq_align > > SeqAlignList;
    SeqAlignList seqAligns;
    SeqAnnotList::const_iterator n, ne = seqAnnots.end();
    for (n=seqAnnots.begin(); n!=ne; ++n) {

        if (!n->GetObject().GetData().IsAlign())
            THROW_MESSAGE("AlignmentSet::AlignmentSet() - confused by Seq-annot data format");
        if (n != seqAnnots.begin())
            TRACE_MESSAGE("multiple Seq-annots");

        CSeq_annot::C_Data::TAlign::const_iterator
            a, ae = n->GetObject().GetData().GetAlign().end();
        for (a=n->GetObject().GetData().GetAlign().begin(); a!=ae; ++a) {

            if (!(a->GetObject().GetType() != CSeq_align::eType_partial ||
                    a->GetObject().GetType() != CSeq_align::eType_diags) ||
                    !a->GetObject().IsSetDim() || a->GetObject().GetDim() != 2 ||
                    (!a->GetObject().GetSegs().IsDendiag() && !a->GetObject().GetSegs().IsDenseg()))
                THROW_MESSAGE("AlignmentSet::AlignmentSet() - confused by alignment type");

            seqAligns.push_back(*a);
        }
    }
    if (seqAligns.size() == 0)
        THROW_MESSAGE("AlignmentSet::AlignmentSet() - must have at least one Seq-align");

    // we need to determine the identity of the master sequence; most rigorous way is to look
    // for a Seq-id that is present in all pairwise alignments
    m_master = NULL;
    const Sequence *seq1 = NULL, *seq2 = NULL;
    bool seq1PresentInAll = true, seq2PresentInAll = true;

    // first, find sequences for first pairwise alignment
    const CSeq_id& frontSid = seqAligns.front()->GetSegs().IsDendiag() ?
        seqAligns.front()->GetSegs().GetDendiag().front()->GetIds().front().GetObject() :
        seqAligns.front()->GetSegs().GetDenseg().GetIds().front().GetObject();
    const CSeq_id& backSid = seqAligns.front()->GetSegs().IsDendiag() ?
        seqAligns.front()->GetSegs().GetDendiag().front()->GetIds().back().GetObject() :
        seqAligns.front()->GetSegs().GetDenseg().GetIds().back().GetObject();
    SequenceSet::SequenceList::const_iterator s, se = sequenceSet.m_sequences.end();
    for (s=sequenceSet.m_sequences.begin(); s!=se; ++s) {
        if ((*s)->MatchesSeqId(frontSid)) seq1 = *s;
        if ((*s)->MatchesSeqId(backSid)) seq2 = *s;
        if (seq1 && seq2) break;
    }
    if (!(seq1 && seq2))
        THROW_MESSAGE("AlignmentSet::AlignmentSet() - can't match first pair of Seq-ids to Sequences");

    // now, make sure one of these sequences is present in all the other pairwise alignments
    SeqAlignList::const_iterator a = seqAligns.begin(), ae = seqAligns.end();
    for (++a; a!=ae; ++a) {
        const CSeq_id& frontSid2 = (*a)->GetSegs().IsDendiag() ?
            (*a)->GetSegs().GetDendiag().front()->GetIds().front().GetObject() :
            (*a)->GetSegs().GetDenseg().GetIds().front().GetObject();
        const CSeq_id& backSid2 = (*a)->GetSegs().IsDendiag() ?
            (*a)->GetSegs().GetDendiag().front()->GetIds().back().GetObject() :
            (*a)->GetSegs().GetDenseg().GetIds().back().GetObject();
        if (!seq1->MatchesSeqId(frontSid2) && !seq1->MatchesSeqId(backSid2))
            seq1PresentInAll = false;
        if (!seq2->MatchesSeqId(frontSid2) && !seq2->MatchesSeqId(backSid2))
            seq2PresentInAll = false;
    }
    if (!seq1PresentInAll && !seq2PresentInAll)
        THROW_MESSAGE("AlignmentSet::AlignmentSet() - "
            "all pairwise sequence alignments must have a common master sequence");
    else if (seq1PresentInAll && !seq2PresentInAll)
        m_master = seq1;
    else if (seq2PresentInAll && !seq1PresentInAll)
        m_master = seq2;
    else if (seq1PresentInAll && seq2PresentInAll && seq1 == seq2)
        m_master = seq1;

    // if still ambiguous, just use the first one for now
    if (!m_master) {
        WARNING_MESSAGE("alignment master sequence is ambiguous - using the first one ("
            << seq1->IdentifierString() << ')');
        m_master = seq1;
    }
    TRACE_MESSAGE("determined master sequence: " << m_master->IdentifierString());

    // parse the pairwise alignments
    SeqAlignList::const_iterator l, le = seqAligns.end();
    for (l=seqAligns.begin(); l!=le; ++l)
        m_alignments.push_back(
            CRef<MasterSlaveAlignment>(new MasterSlaveAlignment(**l, m_master, sequenceSet)));

    TRACE_MESSAGE("number of alignments: " << m_alignments.size());
}

AlignmentSet * AlignmentSet::CreateFromMultiple(
    const BlockMultipleAlignment *multiple, SeqAnnotList *newAsnAlignmentData,
    const SequenceSet& sequenceSet, const vector < unsigned int > *rowOrder)
{
    newAsnAlignmentData->clear();

    // sanity check on the row order map
    if (rowOrder) {
        map < unsigned int, unsigned int > rowCheck;
        for (unsigned int i=0; i<rowOrder->size(); ++i)
            rowCheck[(*rowOrder)[i]] = i;
        if (rowOrder->size() != multiple->NRows() || rowCheck.size() != multiple->NRows() || (*rowOrder)[0] != 0) {
            ERROR_MESSAGE("AlignmentSet::CreateFromMultiple() - bad row order vector");
            return NULL;
        }
    }

    // create a single Seq-annot, with 'align' data that holds one Seq-align per slave
    SeqAnnotList newSeqAnnots;
    CRef < CSeq_annot > seqAnnot(new CSeq_annot());
    newSeqAnnots.push_back(seqAnnot);

    CSeq_annot::C_Data::TAlign& seqAligns = seqAnnot->SetData().SetAlign();
    seqAligns.resize((multiple->NRows() == 1) ? 1 : multiple->NRows() - 1);
    CSeq_annot::C_Data::TAlign::iterator sa = seqAligns.begin();

    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);

    // create Seq-aligns; if there's only one row (the master), then cheat and create an alignment
    // of the master with itself, because asn data doesn't take well to single-row "alignment"
    if (multiple->NRows() > 1) {
        for (unsigned int row=1; row<multiple->NRows(); ++row, ++sa) {
            sa->Reset(CreatePairwiseSeqAlignFromMultipleRow(multiple, blocks,
                (rowOrder ? (*rowOrder)[row] : row)));
        }
    } else
        sa->Reset(CreatePairwiseSeqAlignFromMultipleRow(multiple, blocks, 0));

    auto_ptr<AlignmentSet> newAlignmentSet;
    try {
        newAlignmentSet.reset(new AlignmentSet(newSeqAnnots, sequenceSet));
    } catch (CException& e) {
        ERROR_MESSAGE(
            "AlignmentSet::CreateFromMultiple() - failed to create AlignmentSet from new asn object: "
            << e.GetMsg());
        return NULL;
    }

    *newAsnAlignmentData = newSeqAnnots;
    return newAlignmentSet.release();
}


///// MasterSlaveAlignment methods /////

const unsigned int MasterSlaveAlignment::UNALIGNED = kMax_UInt;

MasterSlaveAlignment::MasterSlaveAlignment(const ncbi::objects::CSeq_align& seqAlign,
    const Sequence *masterSequence, const SequenceSet& sequenceSet) :
        m_master(masterSequence), m_slave(NULL)
{
    // resize alignment and block vector
    m_masterToSlave.resize(m_master->Length(), UNALIGNED);
    m_blockStructure.resize(m_master->Length(), UNALIGNED);

    // find slave sequence for this alignment, and order (master or slave first)
    const CSeq_id& frontSeqId = seqAlign.GetSegs().IsDendiag() ?
        seqAlign.GetSegs().GetDendiag().front()->GetIds().front().GetObject() :
        seqAlign.GetSegs().GetDenseg().GetIds().front().GetObject();
    const CSeq_id& backSeqId = seqAlign.GetSegs().IsDendiag() ?
        seqAlign.GetSegs().GetDendiag().front()->GetIds().back().GetObject() :
        seqAlign.GetSegs().GetDenseg().GetIds().back().GetObject();

    bool masterFirst = true;
    SequenceSet::SequenceList::const_iterator s, se = sequenceSet.m_sequences.end();
    for (s=sequenceSet.m_sequences.begin(); s!=se; ++s) {
        if (m_master->MatchesSeqId(frontSeqId) &&
            (*s)->MatchesSeqId(backSeqId)) {
            break;
        } else if ((*s)->MatchesSeqId(frontSeqId) &&
                   m_master->MatchesSeqId(backSeqId)) {
            masterFirst = false;
            break;
        }
    }
    if (s == se) {
        ERROR_MESSAGE("MasterSlaveAlignment::MasterSlaveAlignment() - couldn't find matching sequences; "
            << "both " << frontSeqId.AsFastaString() << " and "
            << backSeqId.AsFastaString() << " must be in the sequence list for this file!");
        THROW_MESSAGE("couldn't find matching sequences");
    } else {
        m_slave = *s;
    }

    int masterRes, slaveRes, blockNum = 0;
	unsigned int i;

    // unpack dendiag alignment
    if (seqAlign.GetSegs().IsDendiag()) {

        CSeq_align::C_Segs::TDendiag::const_iterator d , de = seqAlign.GetSegs().GetDendiag().end();
        for (d=seqAlign.GetSegs().GetDendiag().begin(); d!=de; ++d, ++blockNum) {
            const CDense_diag& block = d->GetObject();

            if (block.GetDim() != 2 || block.GetIds().size() != 2 || block.GetStarts().size() != 2)
                THROW_MESSAGE("MasterSlaveAlignment::MasterSlaveAlignment() - incorrect dendiag block dimensions");

            // make sure identities of master and slave sequences match in each block
            if ((masterFirst &&
                    (!m_master->MatchesSeqId(block.GetIds().front().GetObject()) ||
                     !m_slave->MatchesSeqId(block.GetIds().back().GetObject()))) ||
                (!masterFirst &&
                    (!m_master->MatchesSeqId(block.GetIds().back().GetObject()) ||
                     !m_slave->MatchesSeqId(block.GetIds().front().GetObject()))))
                THROW_MESSAGE("MasterSlaveAlignment::MasterSlaveAlignment() - mismatched Seq-id in dendiag block");

            // finally, actually unpack the data into the alignment vector
            for (i=0; i<block.GetLen(); ++i) {
                if (masterFirst) {
                    masterRes = block.GetStarts().front() + i;
                    slaveRes = block.GetStarts().back() + i;
                } else {
                    masterRes = block.GetStarts().back() + i;
                    slaveRes = block.GetStarts().front() + i;
                }
                if ((unsigned) masterRes >= m_master->Length() || (unsigned) slaveRes >= m_slave->Length())
                    THROW_MESSAGE("MasterSlaveAlignment::MasterSlaveAlignment() - seqloc in dendiag block > length of sequence!");
                m_masterToSlave[masterRes] = slaveRes;
                m_blockStructure[masterRes] = blockNum;
            }
        }
    }

    // unpack denseg alignment
    else if (seqAlign.GetSegs().IsDenseg()) {

        const CDense_seg& block = seqAlign.GetSegs().GetDenseg();

        if (!block.IsSetDim() && block.GetDim() != 2 ||
                block.GetIds().size() != 2 ||
                block.GetStarts().size() != ((unsigned int) 2 * block.GetNumseg()) ||
                block.GetLens().size() != ((unsigned int) block.GetNumseg()))
            THROW_MESSAGE("MasterSlaveAlignment::MasterSlaveAlignment() - incorrect denseg block dimension");

        // make sure identities of master and slave sequences match in each block
        if ((masterFirst &&
                (!m_master->MatchesSeqId(block.GetIds().front().GetObject()) ||
                 !m_slave->MatchesSeqId(block.GetIds().back().GetObject()))) ||
            (!masterFirst &&
                (!m_master->MatchesSeqId(block.GetIds().back().GetObject()) ||
                 !m_slave->MatchesSeqId(block.GetIds().front().GetObject()))))
            THROW_MESSAGE("MasterSlaveAlignment::MasterSlaveAlignment() - mismatched Seq-id in denseg block");

        // finally, actually unpack the data into the alignment vector
        CDense_seg::TStarts::const_iterator starts = block.GetStarts().begin();
        CDense_seg::TLens::const_iterator lens, le = block.GetLens().end();
        for (lens=block.GetLens().begin(); lens!=le; ++lens) {
            if (masterFirst) {
                masterRes = *(starts++);
                slaveRes = *(starts++);
            } else {
                slaveRes = *(starts++);
                masterRes = *(starts++);
            }
            if (masterRes != -1 && slaveRes != -1) { // skip gaps
                if ((masterRes + *lens - 1) >= m_master->Length() ||
                        (slaveRes + *lens - 1) >= m_slave->Length())
                    THROW_MESSAGE("MasterSlaveAlignment::MasterSlaveAlignment() - "
                        "seqloc in denseg block > length of sequence!");
                for (i=0; i<*lens; ++i) {
                    m_masterToSlave[masterRes + i] = slaveRes + i;
                    m_blockStructure[masterRes + i] = blockNum;
                }
                ++blockNum; // a "block" of a denseg is an aligned (non-gap) segment
            }
        }
    }
}

END_SCOPE(struct_util)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2004/05/25 16:12:30  thiessen
* fix GCC warnings
*
* Revision 1.2  2004/05/25 15:52:17  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/
