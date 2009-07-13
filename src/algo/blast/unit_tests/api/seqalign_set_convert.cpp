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
 * Author: Christiam Camacho
 *
 */

/** @file seqalign_set_convert.cpp
 * Converts a Seq-align-set into a neutral seqalign for use with the
 * CSeqAlignCmp class
 */

#include <ncbi_pch.hpp>
#include "seqalign_set_convert.hpp"

// Object includes
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
BEGIN_SCOPE(blast)
BEGIN_SCOPE(qa)

template <class T>
void s_PrintTextAsnObject(const string fname, const T& obj)
{
#if defined(VERBOSE_DEBUG)
    ofstream out(fname.c_str());
    if (!out) {
        throw runtime_error("Failed to open" + fname);
    }
    out << MSerial_AsnText << obj;
#endif
}

void SetScores(const CSeq_align::TScore& scores, SeqAlign& retval)
{
    ITERATE(CSeq_align::TScore, s, scores) {
        if ( !(*s)->CanGetId() ) {
            continue;
        }

        _ASSERT((*s)->GetId().IsStr());
        const string score_type = (*s)->GetId().GetStr();
        if (score_type == "score") {
            retval.score = (*s)->GetValue().GetInt();
        } else if (score_type == "e_value") {
            retval.evalue = (*s)->GetValue().GetReal();
        } else if (score_type == "bit_score") {
            retval.bit_score = (*s)->GetValue().GetReal();
        } else if (score_type == "num_ident") {
            retval.num_ident = (*s)->GetValue().GetInt();
        } 
    }
}

void DensegConvert(const objects::CDense_seg& denseg, SeqAlign& retval)
{
    string fname("densegconvert.asn");
    s_PrintTextAsnObject(fname, denseg);

    if (denseg.CanGetDim() && denseg.GetDim() != SeqAlign::kNumDimensions) {
        throw runtime_error("Invalid number of dimensions");
    }

    retval.query_strand = denseg.GetSeqStrand(0);
    retval.subject_strand = denseg.GetSeqStrand(1);

    copy(denseg.GetStarts().begin(),
         denseg.GetStarts().end(),
         back_inserter(retval.starts));

    copy(denseg.GetLens().begin(),
         denseg.GetLens().end(),
         back_inserter(retval.lengths));
    _ASSERT(retval.lengths.size() == (size_t)denseg.GetNumseg());

    _ASSERT(denseg.CanGetIds());
    const CDense_seg::TIds& ids = denseg.GetIds();
    _ASSERT(ids.size() == (size_t)denseg.GetDim());

    CRef<CSeq_id> query_id = ids.front();
    if (query_id->IsGi()) {
        retval.sequence_gis.SetQuery(query_id->GetGi());
    }

    CRef<CSeq_id> subj_id = ids.back();
    if (subj_id->IsGi()) {
        retval.sequence_gis.SetSubject(subj_id->GetGi());
    }

}

void SeqAlignConvert(const objects::CSeq_align& sa, SeqAlign& retval)
{
    string fname("seqalignconvert.asn");
    s_PrintTextAsnObject(fname, sa);

    if (sa.GetType() != CSeq_align::eType_partial) {
        throw runtime_error("Seq-align is not of partial type");
    }
    if (sa.CanGetDim() && sa.GetDim() != SeqAlign::kNumDimensions) {
        throw runtime_error("Invalid number of dimensions");
    }

    SetScores(sa.GetScore(), retval);

    const CSeq_align::C_Segs& segs = sa.GetSegs();

    switch (segs.Which()) {
    case CSeq_align::C_Segs::e_Denseg:
        _ASSERT(segs.IsDenseg());
        DensegConvert(segs.GetDenseg(), retval);
        break;

    case CSeq_align::C_Segs::e_Std:
        _ASSERT(segs.IsStd());
        //StdsegConvert(segs.GetStd(), retval);
        throw runtime_error("Std-seg support is not implemented");
        break;

    default:
        throw runtime_error("Unsupported alignment data type");
    }
}

void BlastSeqAlignSetConvert(const objects::CSeq_align& sa,
                             std::vector<SeqAlign>& retval)
{
    string fname("blastseqalignconvert.asn");
    s_PrintTextAsnObject(fname, sa);

        SeqAlign neutral_seqalign;
        SeqAlignConvert(sa, neutral_seqalign);
        retval.push_back(neutral_seqalign);
}

void SeqAlignSetConvert(const objects::CSeq_align_set& ss, 
                        std::vector<SeqAlign>& retval)
{
    if ( !ss.CanGet() ) {
        throw runtime_error("Empty Seq-align-set");
    }

    retval.clear();
    ITERATE(CSeq_align_set::Tdata, seqalign_set, ss.Get()) {
        BlastSeqAlignSetConvert(**seqalign_set, retval);
    }
}

END_SCOPE(qa)
END_SCOPE(blast)
END_SCOPE(ncbi)
