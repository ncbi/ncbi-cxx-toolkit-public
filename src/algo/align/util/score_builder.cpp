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

///
/// calculate mismatches and identites in a seq-align
///
static void s_GetCountIdentityMismatch(CScope& scope, const CSeq_align& align,
                                       int* identities, int* mismatches)
{
    _ASSERT(identities  &&  mismatches);
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
            const CDense_seg& ds = align.GetSegs().GetDenseg();
            CAlnVec vec(ds, scope);
            for (int seg = 0;  seg < vec.GetNumSegs();  ++seg) {
                vector<string> data;
                for (int i = 0;  i < vec.GetNumRows();  ++i) {
                    data.push_back(string());
                    TSeqPos start = vec.GetStart(i, seg);
                    if (start == -1) {
                        /// we compute ungapped identities
                        /// gap on at least one row, so we skip this segment
                        break;
                    }
                    TSeqPos stop  = vec.GetStop(i, seg);
                    if (start == stop) {
                        break;
                    }

                    vec.GetSeqString(data.back(), i, start, stop);
                }

                if (data.size() == ds.GetDim()) {
                    for (size_t a = 0;  a < data[0].size();  ++a) {
                        bool is_mismatch = false;
                        for (size_t b = 1;  b < data.size();  ++b) {
                            size_t data_b_len = data[b].size();
                            if (data[b][a] != data[0][a]) {
                                is_mismatch = true;
                                break;
                            }
                        }

                        if (is_mismatch) {
                            ++(*mismatches);
                        } else {
                            ++(*identities);
                        }
                    }
                }
            }
        }}
        break;

    case CSeq_align::TSegs::e_Disc:
        {{
            ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter, align.GetSegs().GetDisc().Get()) {
                s_GetCountIdentityMismatch(scope, **iter, identities, mismatches);
            }
        }}
        break;

    case CSeq_align::TSegs::e_Std:
        _ASSERT(false);
        break;

    default:
        _ASSERT(false);
        break;
    }
}


///
/// calculate the number of gaps in our alignment
///
static int s_GetGapCount(const CSeq_align& align)
{
    int gap_count = 0;
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
            const CDense_seg& ds = align.GetSegs().GetDenseg();
            for (CDense_seg::TNumseg i = 0;  i < ds.GetNumseg();  ++i) {
                bool is_gapped = false;
                for (CDense_seg::TDim j = 0;  j < ds.GetDim();  ++j) {
                    if (ds.GetStarts()[i * ds.GetDim() + j] == -1) {
                        is_gapped = true;
                        break;
                    }
                }
                if (is_gapped) {
                    ++gap_count;
                }
            }
        }}
        break;

    case CSeq_align::TSegs::e_Disc:
        {{
            ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter, align.GetSegs().GetDisc().Get()) {
                gap_count += s_GetGapCount(**iter);
            }
        }}
        break;

    case CSeq_align::TSegs::e_Std:
        break;

    default:
        break;
    }

    return gap_count;
}


///
/// calculate the length of our alignment
///
static size_t s_GetAlignmentLength(const CSeq_align& align,
                                   bool ungapped)
{
    size_t len = 0;
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
            const CDense_seg& ds = align.GetSegs().GetDenseg();
            for (CDense_seg::TNumseg i = 0;  i < ds.GetNumseg();  ++i) {
                //int this_len = ds.GetLens()[i];
                CDense_seg::TDim count_gapped = 0;
                for (CDense_seg::TDim j = 0;  j < ds.GetDim();  ++j) {
                    //int start = ds.GetStarts()[i * ds.GetDim() + j];
                    if (ds.GetStarts()[i * ds.GetDim() + j] == -1) {
                        ++count_gapped;
                    }
                }
                if (ungapped) {
                    if (count_gapped == 0) {
                        len += ds.GetLens()[i];
                    }
                } else if (ds.GetDim() - count_gapped > 1) {
                    /// we have at least one row of sequence
                    len += ds.GetLens()[i];
                }
            }
        }}
        break;

    case CSeq_align::TSegs::e_Disc:
        {{
            ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter, align.GetSegs().GetDisc().Get()) {
                len += s_GetAlignmentLength(**iter, ungapped);
            }
        }}
        break;

    case CSeq_align::TSegs::e_Std:
        break;

    default:
        break;
    }

    return len;
}


///
/// calculate the percent identity
/// we also return the count of identities and mismatches
///
static void s_GetPercentIdentity(CScope& scope, const CSeq_align& align,
                                 int* identities,
                                 int* mismatches,
                                 double* pct_identity)
{
    double count_aligned = s_GetAlignmentLength(align, true /* ungapped */);
    s_GetCountIdentityMismatch(scope, align, identities, mismatches);
    *pct_identity = 100.0f * double(*identities) / count_aligned;
}


/////////////////////////////////////////////////////////////////////////////


int CScoreBuilder::GetIdentityCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities,&mismatches);
    return identities;
}


int CScoreBuilder::GetMismatchCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities,&mismatches);
    return mismatches;
}


int CScoreBuilder::GetGapCount(const CSeq_align& align)
{
    return s_GetGapCount(align);
}


TSeqPos CScoreBuilder::GetAlignLength(const CSeq_align& align)
{
    return s_GetAlignmentLength(align, false);
}


/////////////////////////////////////////////////////////////////////////////


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
            int mismatches = 0;
            s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount, identities);
        }}
        break;

    case eScore_PercentIdentity:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity);
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
