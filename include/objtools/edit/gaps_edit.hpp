#ifndef OBJTOOLS_EDIT_GAPS_EDITOR_HPP_INCLUDED
#define OBJTOOLS_EDIT_GAPS_EDITOR_HPP_INCLUDED

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
* Authors:  Sergiy Gotvyanskyy, NCBI
*
*/

/// @file fasta.hpp
/// Operators to edit gaps in sequences

#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/Bioseq.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDelta_seq;

class NCBI_XOBJEDIT_EXPORT CGapsEditor
{
public:
    // this will convert runs of Ns into Gaps
    // runs of Ns shorter than gapMin will be leaved as is
    // sequences may have any packing and will be optimally packed in result
    // runs of Ns with exactly length of gap_Unknown_length will be marked appropriate
    // optionally specify linkage evidence or leave it default
    //CLinkage_evidence::EType
    typedef set<int> TEvidenceSet;
    CGapsEditor(CSeq_gap::EType gap_type, const TEvidenceSet& evidences,
        TSeqPos gapNmin, TSeqPos gap_Unknown_length);

    void ConvertNs2Gaps(CSeq_entry& entry);

    void ConvertNs2Gaps(CBioseq& bioseq);

    void ConvertNs2Gaps(CBioseq::TInst& inst);
    void ConvertNs2Gaps(const CSeq_data& data, TSeqPos len, CDelta_ext& ext);
    CRef<CDelta_seq> CreateGap(CBioseq& bioseq, TSeqPos gap_start, TSeqPos gap_length);

    void ConvertBioseqToDelta(CBioseq& bioseq);
    void AppendGap(CBioseq& bioseq);
    void AddBioseqAsLiteral(CBioseq& parent, CBioseq& bioseq);

private:
    void x_SetGapParameters(CDelta_seq& gap);

    CSeq_gap::EType m_gap_type;
    TEvidenceSet    m_evidences;
    TSeqPos         m_gapNmin;
    TSeqPos         m_gap_Unknown_length;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
