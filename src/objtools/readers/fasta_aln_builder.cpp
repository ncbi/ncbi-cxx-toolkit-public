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
* Author:  Aaron Ucko
*
* File Description:
*   Helper class to build pairwise alignments, with double gaps
*   automatically spliced out.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "fasta_aln_builder.hpp"

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CFastaAlignmentBuilder::CFastaAlignmentBuilder(CRef<CSeq_id> reference_id,
                                               CRef<CSeq_id> other_id)
    : m_DS(new CDense_seg), m_LastAlignmentPos(0), m_LastReferencePos(kNoPos),
      m_LastOtherPos(kNoPos), m_LastState(eDoubleGap),
      m_LastNonDGState(eDoubleGap)
{
    m_DS->SetIds().push_back(reference_id);
    m_DS->SetIds().push_back(other_id);
}


CRef<CSeq_align> CFastaAlignmentBuilder::GetCompletedAlignment(void)
{
    CRef<CSeq_align> sa(new CSeq_align);
    sa->SetType(CSeq_align::eType_not_set);
    sa->SetDim(2);
    m_DS->SetNumseg(m_DS->GetLens().size());
    sa->SetSegs().SetDenseg(*m_DS);
    return sa;
}


inline
CFastaAlignmentBuilder::EState CFastaAlignmentBuilder::x_State
(TSignedSeqPos reference_pos, TSignedSeqPos other_pos)
{
    int state = eDoubleGap;
    if (reference_pos != kNoPos) {
        state |= eReferenceOnly;
    }
    if (other_pos != kNoPos) {
        state |= eOtherOnly;
    }
    return static_cast<EState>(state);
}


inline
void CFastaAlignmentBuilder::x_EnsurePos(TSignedSeqPos& pos,
                                         TSignedSeqPos  last_pos,
                                         TSeqPos        alignment_pos)
{
    if (pos == kContinued) {
        if (last_pos == kNoPos) {
            pos = kNoPos;
        } else {
            pos = last_pos + alignment_pos - m_LastAlignmentPos;
        }
    }
}


void CFastaAlignmentBuilder::AddData(TSeqPos       alignment_pos,
                                     TSignedSeqPos reference_pos,
                                     TSignedSeqPos other_pos)
{
    x_EnsurePos(reference_pos, m_LastReferencePos, alignment_pos);
    x_EnsurePos(other_pos, m_LastOtherPos, alignment_pos);

    EState state = x_State(reference_pos, other_pos);

    if (m_LastState != eDoubleGap) {
        m_DS->SetLens().back() += alignment_pos - m_LastAlignmentPos;
    }
    if (state != eDoubleGap  &&  state != m_LastNonDGState) {
        // new segment
        m_DS->SetStarts().push_back(reference_pos);
        m_DS->SetStarts().push_back(other_pos);
        m_DS->SetLens().push_back(0);
        m_LastNonDGState = state;
    }

    m_LastAlignmentPos = alignment_pos;
    m_LastReferencePos = reference_pos;
    m_LastOtherPos     = other_pos;
    m_LastState        = state;
}


END_SCOPE(objects)
END_NCBI_SCOPE
