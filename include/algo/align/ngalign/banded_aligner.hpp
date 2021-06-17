#ifndef NGALIGN_BANDED_ALIGNER__HPP
#define NGALIGN_BANDED_ALIGNER__HPP

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
 * Authors:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Na_strand.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
    class CDense_seg;
END_SCOPE(objects)




class CInstance : public CObject {
public:

    CInstance(const CRef<objects::CSeq_align> Align);
    CInstance(const objects::CSeq_align_set& AlignSet);

    void MergeIn(CRef<objects::CSeq_align> Align);

    bool IsAlignmentContained(const objects::CSeq_align& Align) const;
    int GapDistance(const objects::CSeq_align& Align) const;

    double SubjToQueryRatio() const;
    TSeqPos QueryLength() const;

    objects::CSeq_interval  Query;
    objects::CSeq_interval  Subject;
    objects::CSeq_align_set Alignments;
};


class CInstancedAligner : public IAlignmentFactory
{
public:

    CInstancedAligner(int TimeOutSeconds, float MaxRatio, float MinPctCoverage, int Threshold)
        : m_TimeOutSeconds(TimeOutSeconds), m_MaxRatio(MaxRatio),
          m_MinPctCoverage(MinPctCoverage), m_Threshold(Threshold),
          m_Match(2), m_Mismatch(-3), m_GapOpen(-100), m_GapExtend(-1) { ; }

    // Defaults to +2, -3, -100, -1
    void SetPathValues(int Match, int Mismatch, int GapOpen, int GapExtend)
    {
        m_Match = Match;
        m_Mismatch = Mismatch;
        m_GapOpen = GapOpen;
        m_GapExtend = GapExtend;
    }

    string GetName() const { return "instanced_mm_aligner"; }

    TAlignResultsRef GenerateAlignments(objects::CScope& Scope,
                                        ISequenceSet* QuerySet,
                                        ISequenceSet* SubjectSet,
                                        TAlignResultsRef AccumResults);

protected:


private:

    int m_TimeOutSeconds;
    float m_MaxRatio;
    float m_MinPctCoverage;
    int m_Threshold;

    int m_Match, m_Mismatch, m_GapOpen, m_GapExtend;


    void x_RunAligner(objects::CScope& Scope,
                      CQuerySet& QueryAligns,
                      TAlignResultsRef Results);

    CRef<objects::CDense_seg> x_RunMMGlobal(const objects::CSeq_id& QueryId,
                                            const objects::CSeq_id& SubjectId,
                                            objects::ENa_strand Strand,
                                            TSeqPos QueryStart,
                                            TSeqPos QueryStop,
                                            TSeqPos SubjectStart,
                                            TSeqPos SubjectStop,
                                            objects::CScope& Scope);

    CRef<objects::CSeq_align_set> x_RunCleanup(const objects::CSeq_align_set& AlignSet,
                                               objects::CScope& Scope);

    void x_GetCleanupInstances(CQuerySet& QueryAligns, objects::CScope& Scope,
                        vector<CRef<CInstance> >& Instances);
    void x_GetDistanceInstances(CQuerySet& QueryAligns, objects::CScope& Scope,
                        vector<CRef<CInstance> >& Instances);

    void x_FilterInstances(vector<CRef<CInstance> >& Instances, double MaxRatio);

    bool x_MinCoverageCheck(const CQuerySet& QueryAligns);
};




//TSeqPos x_CalcQueryCoverage(TAlignSetRef Alignments, int Row, objects::CScope& Scope);





END_NCBI_SCOPE

#endif
