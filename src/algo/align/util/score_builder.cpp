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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/align/util/score_builder.hpp>

#include <objmgr/scope.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


static void s_CalcIdentityCount(CScope& scope, const CSeq_align& align,
                                int* identities)
{
    if ( !align.GetSegs().IsDenseg() ) {
        NCBI_THROW(CException, eUnknown,
                   "s_CalcPercentIdentity() requires a denseg alignment");
    }

    const CDense_seg& ds = align.GetSegs().GetDenseg();
    if (ds.GetIds().size() != 2) {
        NCBI_THROW(CException, eUnknown,
            "s_CalcPercentIdentity() requires a pairwise alignment");
    }

    int count_identities = 0;
    CAlnVec vec(ds, scope);
    CBioseq_Handle bsh1 = vec.GetBioseqHandle(0);
    CBioseq_Handle bsh2 = vec.GetBioseqHandle(1);
    CSeqVector vec1(bsh1);
    CSeqVector vec2(bsh2);
    for (CAlnVec::TNumseg seg_idx = 0;  seg_idx < vec.GetNumSegs();  ++seg_idx) {
        if (vec.GetStart(0, seg_idx) == -1) {
            continue;
        }

        if (vec.GetStart(1, seg_idx) == -1) {
            continue;
        }
        for (TSeqPos pos = 0;  pos < vec.GetLen(seg_idx);  ++pos) {
            if (vec1[vec.GetStart(0, seg_idx) + pos] == vec2[vec.GetStart(1, seg_idx) + pos]) {
                ++count_identities;
            }
        }
    }

    *identities = count_identities;
}


static void s_CalcPercentIdentity(CScope& scope, const CSeq_align& align,
                                  int* identities,
                                  double* pct_identity)
{
    if ( !align.GetSegs().IsDenseg() ) {
        NCBI_THROW(CException, eUnknown,
                   "s_CalcPercentIdentity() requires a denseg alignment");
    }

    const CDense_seg& ds = align.GetSegs().GetDenseg();
    if (ds.GetIds().size() != 2) {
        NCBI_THROW(CException, eUnknown,
            "s_CalcPercentIdentity() requires a pairwise alignment");
    }

    int count_identities = 0;
    int count_aligned    = 0;

    CAlnVec vec(ds, scope);
    if (align.GetNamedScore(CSeq_align::eScore_IdentityCount, count_identities)) {
        /// num_ident already exists
        /// we only need to count the aligned length
        for (CAlnVec::TNumseg seg_idx = 0;  seg_idx < vec.GetNumSegs();  ++seg_idx) {
            if (vec.GetStart(0, seg_idx) == -1) {
                continue;
            }

            if (vec.GetStart(1, seg_idx) == -1) {
                continue;
            }

            count_aligned += vec.GetLen(seg_idx);
        }
    } else {
        CBioseq_Handle bsh1 = vec.GetBioseqHandle(0);
        CBioseq_Handle bsh2 = vec.GetBioseqHandle(1);
        CSeqVector vec1(bsh1);
        CSeqVector vec2(bsh2);
        for (CAlnVec::TNumseg seg_idx = 0;  seg_idx < vec.GetNumSegs();  ++seg_idx) {
            if (vec.GetStart(0, seg_idx) == -1) {
                continue;
            }

            if (vec.GetStart(1, seg_idx) == -1) {
                continue;
            }

            count_aligned += vec.GetLen(seg_idx);
            for (TSeqPos pos = 0;  pos < vec.GetLen(seg_idx);  ++pos) {
                if (vec1[vec.GetStart(0, seg_idx) + pos] == vec2[vec.GetStart(1, seg_idx) + pos]) {
                    ++count_identities;
                }
            }
        }
    }

    *identities = count_identities;
    *pct_identity = 100.0f * double(count_identities) / double(count_aligned);
}


void CScoreBuilder::AddScore(CScope& scope, CSeq_align& align,
                             EScoreType score)
{
    if (align.GetSegs().IsDisc()) {
        NON_CONST_ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter, align.SetSegs().SetDisc().Set()) {
            AddScore(scope, **iter, score);
        }
        return;
    } else if ( !align.GetSegs().IsDenseg() ) {
        NCBI_THROW(CException, eUnknown,
            "CScoreBuilder::AddScore(): only dense-seg alignments are supported");
    }

    switch (score) {
    case eScore_Blast:
    case eScore_Blast_BitScore:
    case eScore_Blast_EValue:
        break;

    case eScore_IdentityCount:
        {{
            int identities = 0;
            s_CalcIdentityCount(scope, align, &identities);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount, identities);
        }}
        break;

    case eScore_PercentIdentity:
        {{
            int identities      = 0;
            double pct_identity = 0;
            s_CalcPercentIdentity(scope, align,
                                  &identities, &pct_identity);
            align.SetNamedScore(CSeq_align::eScore_PercentIdentity, pct_identity);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount,   identities);
        }}
        break;
    }
}


void CScoreBuilder::AddScore(CScope& scope,
                             list< CRef<CSeq_align> >& aligns, EScoreType score)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        try {
            AddScore(scope, **iter, score);
        }
        catch (CException& e) {
            LOG_POST(Error
                << "CScoreBuilder::AddScore(): error computing score: "
                << e.GetMsg());
        }
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/07/24 13:16:45  dicuccio
 * Added CScoreBuilder, a general class to provide support for generating scores
 * for alignments
 *
 * ===========================================================================
 */
