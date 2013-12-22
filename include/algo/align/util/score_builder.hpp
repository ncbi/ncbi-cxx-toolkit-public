#ifndef ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP
#define ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP

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

#include <corelib/ncbiobj.hpp>
#include <util/range_coll.hpp>
#include <objtools/alnmgr/score_builder_base.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <objects/seqalign/Seq_align.hpp>

struct BlastScoreBlk; // C structure

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(blast)
    class CBlastOptionsHandle;
END_SCOPE(blast)

BEGIN_SCOPE(objects)

class CScope;

class NCBI_XALGOALIGN_EXPORT CScoreBuilder : public CScoreBuilderBase
{
public:

    CScoreBuilder();
    CScoreBuilder(enum blast::EProgram program_type);
    CScoreBuilder(blast::CBlastOptionsHandle& options);
    ~CScoreBuilder();


    void SetGapOpen(int gapopen)        { m_GapOpen = gapopen; }
    void SetGapExtend(int gapextend)    { m_GapExtend = gapextend; }

    /// @name Functions to add scores directly to Seq-aligns
    /// @{

    /// deprecated: use CSeq_align::EScoreType directly
    NCBI_DEPRECATED
    void AddScore(CScope& scope, CSeq_align& align,
                  EScoreType score);

    /// deprecated: use CSeq_align::EScoreType directly
    NCBI_DEPRECATED
    void AddScore(CScope& scope, list< CRef<CSeq_align> >& aligns,
                  EScoreType score);

    /// @}

    /// @name Functions to compute scores without adding
    /// @{

    double ComputeScore(CScope& scope, const CSeq_align& align,
                        const CRangeCollection<TSeqPos> &ranges,
                        CSeq_align::EScoreType score);

    double ComputeScore(CScope& scope, const CSeq_align& align,
                  CSeq_align::EScoreType score)
    {
        return CScoreBuilderBase::ComputeScore(scope, align, score);
    }

    double ComputeScore(CScope& scope, const CSeq_align& align,
                  const TSeqRange &range,
                  CSeq_align::EScoreType score)
    {
        return CScoreBuilderBase::ComputeScore(scope, align, range, score);
    }

    void AddScore(CScope& scope, CSeq_align& align,
                  CSeq_align::EScoreType score)
    {
        CScoreBuilderBase::AddScore(scope, align, score);
    }

    void AddScore(CScope& scope, list< CRef<CSeq_align> >& aligns,
                  CSeq_align::EScoreType score)
    {
        CScoreBuilderBase::AddScore(scope, aligns, score);
    }

    /// Compute the BLAST score of the alignment
    int GetBlastScore     (CScope& scope, const CSeq_align& align);

    /// Compute the BLAST bit score
    double GetBlastBitScore(CScope& scope, const CSeq_align& align);

    /// Compute the BLAST e-value
    double GetBlastEValue (CScope& scope, const CSeq_align& align);


    void AddTracebacks(CScope& scope, CSeq_align& align);

    void AddTracebacks(CScope& scope, list< CRef<CSeq_align> >& aligns);

    string GetTraceback(CScope& scope, const CSeq_align& align,
                        CSeq_align::TDim row);

    /// @}

    /// @name Functions for configuring blast scores
    /// @{

    void SetEffectiveSearchSpace(Int8 searchsp) // required for blast e-values
    {
        m_EffectiveSearchSpace = searchsp;
    }

    /// @}

private:

    struct BlastScoreBlk *m_ScoreBlk;
    enum blast::EProgram m_BlastType;
    int m_GapOpen;
    int m_GapExtend;
    Int8 m_EffectiveSearchSpace;

    void x_Initialize(blast::CBlastOptionsHandle& options);
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP
