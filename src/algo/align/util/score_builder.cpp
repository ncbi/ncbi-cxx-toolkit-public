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
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_options.h>

#include <objtools/alnmgr/alnvec.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

void CScoreBuilder::x_Initialize(CBlastOptionsHandle& options)
{
    Int2 status;
    const CBlastOptions& opts = options.GetOptions();

    m_GapOpen = opts.GetGapOpeningCost();
    m_GapExtend = opts.GetGapExtensionCost();

    // alignments are either protein-protein or nucl-nucl

    m_BlastType = opts.GetProgram();
    if (m_BlastType == eBlastn || m_BlastType == eMegablast ||
                                m_BlastType == eDiscMegablast) {
        m_BlastType = eBlastn;
    } else {
        m_BlastType = eBlastp;
    }

    if (m_BlastType == eBlastn) {
        m_ScoreBlk = BlastScoreBlkNew(BLASTNA_SEQ_CODE, 1);
    } else {
        m_ScoreBlk = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
    }
    if (m_ScoreBlk == NULL) {
        NCBI_THROW(CException, eUnknown,
                   "Failed to initialize blast score block");
    }

    // fill in the score matrix

    EBlastProgramType core_type = EProgramToEBlastProgramType(m_BlastType);
    BlastScoringOptions *score_options;
    BlastScoringOptionsNew(core_type, &score_options);
    BLAST_FillScoringOptions(score_options, core_type, TRUE,
                            opts.GetMismatchPenalty(),
                            opts.GetMatchReward(),
                            opts.GetMatrixName(),
                            m_GapOpen, m_GapExtend);
    status = Blast_ScoreBlkMatrixInit(core_type, score_options,
                                      m_ScoreBlk, NULL);
    score_options = BlastScoringOptionsFree(score_options);
    if (status) {
        NCBI_THROW(CException, eUnknown,
                   "Failed to initialize score matrix");
    }

    // fill in Karlin blocks

    m_ScoreBlk->kbp_gap_std[0] = Blast_KarlinBlkNew();
    if (m_BlastType == eBlastn) {
        // the following computes the same Karlin blocks as blast
        // if the gap penalties are not large. When the penalties are
        // large the ungapped Karlin blocks are used, but these require
        // sequence data to be computed exactly. Instead we build an
        // ideal ungapped Karlin block to approximate the exact answer
        Blast_ScoreBlkKbpIdealCalc(m_ScoreBlk);
        status = Blast_KarlinBlkNuclGappedCalc(m_ScoreBlk->kbp_gap_std[0],
                                               m_GapOpen, m_GapExtend,
                                               m_ScoreBlk->reward,
                                               m_ScoreBlk->penalty,
                                               m_ScoreBlk->kbp_ideal,
                                               &(m_ScoreBlk->round_down),
                                               NULL);
    }
    else {
        status = Blast_KarlinBlkGappedCalc(m_ScoreBlk->kbp_gap_std[0],
                                           m_GapOpen, m_GapExtend,
                                           m_ScoreBlk->name, NULL);
    }
    if (status || m_ScoreBlk->kbp_gap_std[0] == NULL ||
        m_ScoreBlk->kbp_gap_std[0]->Lambda <= 0.0) {
        NCBI_THROW(CException, eUnknown,
                   "Failed to initialize Karlin blocks");
    }
}

/// Default constructor
CScoreBuilder::CScoreBuilder()
    : m_ScoreBlk(0),
      m_EffectiveSearchSpace(0)
{
}

/// Constructor (uses blast program defaults)
CScoreBuilder::CScoreBuilder(EProgram blast_program)
    : m_EffectiveSearchSpace(0)
{
    CRef<CBlastOptionsHandle>
                        options(CBlastOptionsFactory::Create(blast_program));
    x_Initialize(*options);
}

/// Constructor (uses previously configured blast options)
CScoreBuilder::CScoreBuilder(CBlastOptionsHandle& options)
    : m_EffectiveSearchSpace(0)
{
    x_Initialize(options);
}

/// Destructor
CScoreBuilder::~CScoreBuilder()
{
    m_ScoreBlk = BlastScoreBlkFree(m_ScoreBlk);
}


static const unsigned char reverse_4na[16] = {0, 8, 4, 0, 2, 0, 0, 0, 1};

int CScoreBuilder::GetBlastScore(CScope& scope,
                                 const CSeq_align& align)
{
    if ( !align.GetSegs().IsDenseg() ) {
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CScoreBuilder::GetBlastScore(): "
                   "only dense-seg alignments are supported");
    }

    if (m_ScoreBlk == 0) {
        NCBI_THROW(CSeqalignException, eInvalidInputData,
                   "Blast scoring parameters have not been specified");
    }

    int computed_score = 0;
    const CDense_seg& ds = align.GetSegs().GetDenseg();
    CAlnVec vec(ds, scope);
    CBioseq_Handle bsh1 = vec.GetBioseqHandle(0);
    CBioseq_Handle bsh2 = vec.GetBioseqHandle(1);
    CSeqVector vec1(bsh1);
    CSeqVector vec2(bsh2);
    CSeq_inst::TMol mol1 = vec1.GetSequenceType();
    CSeq_inst::TMol mol2 = vec2.GetSequenceType();

    int gap_open = m_GapOpen;
    int gap_extend = m_GapExtend;

    if (mol1 == CSeq_inst::eMol_aa && mol2 == CSeq_inst::eMol_aa) {

        Int4 **matrix = m_ScoreBlk->matrix->data;

        if (m_BlastType != eBlastp) {
            NCBI_THROW(CSeqalignException, eUnsupported,
                        "Protein scoring parameters required");
        }

        for (CAlnVec::TNumseg seg_idx = 0;
                        seg_idx < vec.GetNumSegs();  ++seg_idx) {

            TSignedSeqPos start1 = vec.GetStart(0, seg_idx);
            TSignedSeqPos start2 = vec.GetStart(1, seg_idx);
            TSeqPos seg_len = vec.GetLen(seg_idx);

            if (start1 == -1 || start2 == -1) {
                computed_score -= gap_open + gap_extend * seg_len;
                continue;
            }

            // @todo FIXME the following assumes ncbistdaa format

            for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                unsigned char c1 = vec1[start1 + pos];
                unsigned char c2 = vec2[start2 + pos];
                computed_score += matrix[c1][c2];
            }
        }
    }
    else if (CSeq_inst::IsNa(mol1) && CSeq_inst::IsNa(mol2)) {

        int match = m_ScoreBlk->reward;
        int mismatch = m_ScoreBlk->penalty; // assumed negative

        if (m_BlastType != eBlastn) {
            NCBI_THROW(CSeqalignException, eUnsupported,
                        "Nucleotide scoring parameters required");
        }

        bool scaled_up = false;
        if (gap_open == 0 && gap_extend == 0) { // possible with megablast
            match *= 2;
            mismatch *= 2;
            gap_extend = match / 2 - mismatch;
            scaled_up = true;
        }

        int strand1 = vec.StrandSign(0);
        int strand2 = vec.StrandSign(1);

        for (CAlnVec::TNumseg seg_idx = 0;
                        seg_idx < vec.GetNumSegs();  ++seg_idx) {

            TSignedSeqPos start1 = vec.GetStart(0, seg_idx);
            TSignedSeqPos start2 = vec.GetStart(1, seg_idx);
            TSeqPos seg_len = vec.GetLen(seg_idx);

            if (start1 == -1 || start2 == -1) {
                computed_score -= gap_open + gap_extend * seg_len;
                continue;
            }

            // @todo FIXME encoding assumed to be ncbi4na, without
            // ambiguity charaters

            if (strand1 > strand2) {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + pos];
                    unsigned char c2 = vec2[start2 + seg_len - 1 - pos];
                    computed_score +=  (c1 == reverse_4na[c2]) ?
                                        match : mismatch;
                }
            }
            else if (strand1 < strand2) {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + seg_len - 1 - pos];
                    unsigned char c2 = vec2[start2 + pos];
                    computed_score += (reverse_4na[c1] == c2) ?
                                        match : mismatch;
                }
            }
            else {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + pos];
                    unsigned char c2 = vec2[start2 + pos];
                    computed_score += (c1 == c2) ? match : mismatch;
                }
            }
        }

        if (scaled_up)
            computed_score /= 2;
    }
    else {
        NCBI_THROW(CSeqalignException, eUnsupported,
                    "pairwise alignment contains unsupported "
                    "or mismatched molecule types");
    }

    computed_score = max(0, computed_score);
    return computed_score;
}


double CScoreBuilder::GetBlastBitScore(CScope& scope,
                                       const CSeq_align& align)
{
    int raw_score = GetBlastScore(scope, align);
    Blast_KarlinBlk *kbp = m_ScoreBlk->kbp_gap_std[0];

    if (m_BlastType == eBlastn && m_ScoreBlk->round_down)
        raw_score &= ~1;

    return (raw_score * kbp->Lambda - kbp->logK) / NCBIMATH_LN2;
}


double CScoreBuilder::GetBlastEValue(CScope& scope,
                                     const CSeq_align& align)
{
    if (m_EffectiveSearchSpace == 0) {
        NCBI_THROW(CSeqalignException, eInvalidInputData,
                   "E-value calculation requires search space "
                   "to be specified");
    }

    int raw_score = GetBlastScore(scope, align);
    Blast_KarlinBlk *kbp = m_ScoreBlk->kbp_gap_std[0];

    if (m_BlastType == eBlastn && m_ScoreBlk->round_down)
        raw_score &= ~1;

    return BLAST_KarlinStoE_simple(raw_score, kbp, m_EffectiveSearchSpace);
}


double CScoreBuilder::ComputeScore(CScope& scope, const CSeq_align& align,
                                   const CRangeCollection<TSeqPos> &ranges,
                                   CSeq_align::EScoreType score)
{
    // Override certain score computations in this subclass.
    switch (score) {
    case CSeq_align::eScore_Blast:
        return GetBlastScore(scope, align);

    case CSeq_align::eScore_BitScore:
        {{
             double d = GetBlastBitScore(scope, align);
             if (d == numeric_limits<double>::infinity()  ||
                 d == numeric_limits<double>::quiet_NaN()) {
                 d = 0;
             }
             if (d > 1e35  ||  d < -1e35) {
                 d = 0;
             }
             return d;
         }}

    case CSeq_align::eScore_EValue:
        {{
             double d = GetBlastEValue(scope, align);
             if (d == numeric_limits<double>::infinity()  ||
                 d == numeric_limits<double>::quiet_NaN()) {
                 d = 0;
             }
             if (d > 1e35  ||  d < -1e35) {
                 d = 0;
             }
             return d;
         }}

    case CSeq_align::eScore_SumEValue:
    case CSeq_align::eScore_CompAdjMethod:
        // FIXME TODO: Not implemented.

    // Fallback to superclass implementation of score computation.
    default:
        return CScoreBuilderBase::ComputeScore(scope, align, ranges, score);
    }
}



void CScoreBuilder::AddScore(CScope& scope, CSeq_align& align,
                             EScoreType score)
{
    switch (score) {
    case eScore_Blast:
        AddScore(scope, align, CSeq_align::eScore_Blast);
        break;

    case eScore_Blast_BitScore:
        AddScore(scope, align, CSeq_align::eScore_BitScore);
        break;

    case eScore_Blast_EValue:
        AddScore(scope, align, CSeq_align::eScore_EValue);
        break;

    case eScore_MismatchCount:
        AddScore(scope, align, CSeq_align::eScore_MismatchCount);
        break;

    case eScore_IdentityCount:
        AddScore(scope, align, CSeq_align::eScore_IdentityCount);
        break;

    case eScore_PercentIdentity:
        AddScore(scope, align, CSeq_align::eScore_PercentIdentity);
        break;
    case eScore_PercentCoverage:
        AddScore(scope, align, CSeq_align::eScore_PercentCoverage);
        break;
    }
}

void CScoreBuilder::AddScore(CScope& scope,
                             list< CRef<CSeq_align> >& aligns, EScoreType score)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        CSeq_align& align = **iter;
        try {
            switch (score) {
            case eScore_Blast:
                AddScore(scope, align, CSeq_align::eScore_Blast);
                break;

            case eScore_Blast_BitScore:
                AddScore(scope, align, CSeq_align::eScore_BitScore);
                break;

            case eScore_Blast_EValue:
                AddScore(scope, align, CSeq_align::eScore_EValue);
                break;

            case eScore_MismatchCount:
                AddScore(scope, align, CSeq_align::eScore_MismatchCount);
                break;

            case eScore_IdentityCount:
                AddScore(scope, align, CSeq_align::eScore_IdentityCount);
                break;

            case eScore_PercentIdentity:
                AddScore(scope, align, CSeq_align::eScore_PercentIdentity);
                break;
            case eScore_PercentCoverage:
                AddScore(scope, align, CSeq_align::eScore_PercentCoverage);
                break;
            }
        }
        catch (CException& e) {
            LOG_POST(Error
                << "CScoreBuilder::AddScore(): error computing score: "
                << e);
        }
    }
}

static inline char s_Complement(char residue)
{
    switch (residue) {
    case 'A':
        return 'T';
    case 'C':
        return 'G';
    case 'G':
        return 'C';
    case 'T':
        return 'A';
    case 'N':
        return 'N';
    default:
        NCBI_THROW(CException, eUnknown, string("Unexpected residue: ") + residue);
        return 0;
    }
}

static inline void s_RecordMatch(size_t match, string &BTOP, string &flipped_BTOP)
{
    if (match) {
        string match_str = NStr::NumericToString(match);
        BTOP += match_str;
        flipped_BTOP.insert(0, match_str);
    }
}

static pair<string,string> s_ComputeTraceback(CScope& scope,
                                              const CSeq_align& align)
{
    if (!align.GetSegs().IsDenseg() || align.CheckNumRows() != 2) {
        NCBI_THROW(CException, eUnknown,
                   "Traceback strings can only be calculated for pairwise "
                   "Dense-seg alignments");
    }

    const CDense_seg& ds = align.GetSegs().GetDenseg();
    CAlnVec vec(ds, scope);
    string BTOP, flipped_BTOP;
    for (CDense_seg::TNumseg i = 0;  i < ds.GetNumseg();  ++i) {
        string query, subject;
        vec.GetSegSeqString(query, 0, i);
        vec.GetSegSeqString(subject, 1, i);
        if (query.empty()) {
            ITERATE (string, it, subject) {
                BTOP += '-';
                BTOP += *it;
                flipped_BTOP.insert(flipped_BTOP.begin(), s_Complement(*it));
                flipped_BTOP.insert(flipped_BTOP.begin(), '-');
            }
        } else if (subject.empty()) {
            ITERATE (string, it, query) {
                BTOP += *it;
                BTOP += '-';
                flipped_BTOP.insert(flipped_BTOP.begin(), '-');
                flipped_BTOP.insert(flipped_BTOP.begin(), s_Complement(*it));
            }
        } else {
            size_t match = 0;
            for (size_t idx = 0; idx < query.size(); ++idx) {
                NCBI_ASSERT(query.size() == subject.size(),
                            "inconsistent aligned segment length");
                if (query[idx] == subject[idx]) {
                    ++match;
                } else {
                    s_RecordMatch(match, BTOP, flipped_BTOP);
                    match = 0;
                    BTOP += query[idx];
                    BTOP += subject[idx];
                    flipped_BTOP.insert(flipped_BTOP.begin(),
                                        s_Complement(subject[idx]));
                    flipped_BTOP.insert(flipped_BTOP.begin(),
                                        s_Complement(query[idx]));
                }
            }
            s_RecordMatch(match, BTOP, flipped_BTOP);
        }
    }
    return pair<string,string>(
        BTOP, align.GetSeqStrand(0) == align.GetSeqStrand(1)
                  ? BTOP : flipped_BTOP);
}

void CScoreBuilder::AddTracebacks(CScope& scope, CSeq_align& align)
{
    CRef<CUser_object> tracebacks;
    ITERATE (CSeq_align::TExt, ext_it, align.SetExt()) {
        if ((*ext_it)->GetType().IsStr() &&
            (*ext_it)->GetType().GetStr() == "Tracebacks")
        {
            /// Tracebacks object already exists
            tracebacks = *ext_it;
            break;
        }
    }

    if (!tracebacks) {
        tracebacks.Reset(new CUser_object);
        tracebacks->SetType().SetStr("Tracebacks");
        align.SetExt().push_back(tracebacks);
    } else if (tracebacks->HasField("Query") && tracebacks->HasField("Subject"))
    {
        return;
    }
    pair<string,string> traceback_strings = s_ComputeTraceback(scope, align);
    tracebacks->SetField("Query").SetData().SetStr(traceback_strings.first);
    tracebacks->SetField("Subject").SetData().SetStr(traceback_strings.second);
}

void CScoreBuilder::AddTracebacks(CScope& scope,
                                  list< CRef<CSeq_align> >& aligns)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        AddTracebacks(scope, **iter);
    }
}

string CScoreBuilder::GetTraceback(CScope& scope, const CSeq_align& align,
                                   CSeq_align::TDim row)
{
    if (align.IsSetExt()) {
        ITERATE (CSeq_align::TExt, ext_it, align.GetExt()) {
            if ((*ext_it)->GetType().IsStr() &&
                (*ext_it)->GetType().GetStr() == "Tracebacks")
            {
                string field = row == 0 ? "Query" : "Subject";
                if ((*ext_it)->HasField(field)) {
                    return (*ext_it)->GetField(field).GetData().GetStr();
                }
                break;
            }
        }
    }

    /// Tracebacks user object not found; need to calculate on the fly
    pair<string,string> traceback_strings = s_ComputeTraceback(scope, align);
    return row == 0 ? traceback_strings.first : traceback_strings.second;
}

END_NCBI_SCOPE
