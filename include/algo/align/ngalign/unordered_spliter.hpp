#ifndef UNORDERED_SPLITTER__HPP
#define UNORDERED_SPLITTER__HPP

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
 * File Description: A tool for aligning sequences that contain unordered pieces
 *                   (aka: phase 1 clones). It will split a sequence into parts,
 *                   put the parts into their own CBioseq, and into the given
 *                   Scope, and spit out the new Seq-ids. Other code can then
 *                   use the parts as their own sequences, and align them.
 *                   This tool can then remap objects that use those local IDs
 *                   back to their original Seq-id.
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Na_strand.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>
#include <algo/align/ngalign/sequence_set.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
    class CDense_seg;
    class CSeq_interval;
    class CBioseq_Handle;
END_SCOPE(objects)



class CUnorderedSplitter
{
public:

    CUnorderedSplitter(objects::CScope& Scope) : m_Scope(&Scope) { ; }

    typedef list<CRef<objects::CSeq_id> > TSeqIdList;
    void SplitId(const objects::CSeq_id& Id, TSeqIdList& SplitIds);
    void SplitLoc(const objects::CSeq_loc& Loc, TSeqIdList& SplitIds);

    typedef list<CRef<objects::CSeq_align> > TSeqAlignList;
    void CombineAlignments(const TSeqAlignList& SourceAligns, TSeqAlignList& MergedAligns);

    void GetSplitIdList(TSeqIdList& SplitIdList);

protected:


private:

    CRef<objects::CScope> m_Scope;

    typedef map<string, CRef<objects::CSeq_interval> >  TSplitIntervalsMap;
    TSplitIntervalsMap  m_PartsMap;

    typedef list< CRef<objects::CSeq_interval> > TIntervalList;
    typedef map<string, TIntervalList > TSplitIntMap;
    TSplitIntMap m_SplitsMap;

    void x_SplitDeltaExt(const objects::CSeq_id& Id,
                         objects::CBioseq_Handle OrigHandle,
                         TSeqIdList& SplitIds,
                         TSeqRange LimitRange = TSeqRange() );
    
    void x_SplitSeqData(const objects::CSeq_id& Id,
                        objects::CBioseq_Handle OrigHandle,
                        TSeqIdList& SplitIds,
                        TSeqRange LimitRange = TSeqRange() );

    CRef<objects::CSeq_align>
    x_FixAlignment(const objects::CSeq_align& SourceAlignment);

    static bool s_SortByQueryStart(const CRef<objects::CSeq_align>& A,
                            const CRef<objects::CSeq_align>& B);
    void x_SortAlignSet(TSeqAlignList& AlignSet);

    void x_MakeAlignmentsUnique(TSeqAlignList& Alignments);
    void x_MakeAlignmentPairUnique(CRef<objects::CSeq_align> First,
                                CRef<objects::CSeq_align> Second);
    void x_TrimRows(const objects::CDense_seg& DomSeg, objects::CDense_seg& NonSeg, int Row);
    bool x_IsAllGap(const objects::CDense_seg& Denseg);

    void x_StripDistantAlignments(TSeqAlignList& Alignments);

};



// For sequences that need to be split up, like Phase 1 clones.
class CSplitSeqIdListSet : public ISequenceSet
{
public:
    CSplitSeqIdListSet(CUnorderedSplitter* Splitter);

    void AddSeqId(CRef<objects::CSeq_id> Id);
    void SetSeqMasker(CSeqMasker* SeqMasker);

    bool Empty() { // either nothing was added, or everything that was added was pure gap
        return m_SeqIdListSet.SetIdList().empty();
    }

    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);
    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts,
            const CAlignResultsSet& Alignments, int Threshold);
    CRef<blast::CLocalDbAdapter> CreateLocalDbAdapter(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);

protected:
    list<CRef<objects::CSeq_id> > m_OrigSeqIdList;
    CSeqIdListSet m_SeqIdListSet;
    CUnorderedSplitter* m_Splitter;
};


// For sequence locations that need to be split up, like Phase 1 clones.
class CSplitSeqLocListSet : public ISequenceSet
{
public:
    CSplitSeqLocListSet(CUnorderedSplitter* Splitter);

    void AddSeqLoc(CRef<objects::CSeq_loc> Loc);
    void SetSeqMasker(CSeqMasker* SeqMasker);
    
    bool Empty() { // either nothing was added, or everything that was added was pure gap
        return m_SeqIdListSet.SetIdList().empty();
    }

    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);
    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts,
            const CAlignResultsSet& Alignments, int Threshold);
    CRef<blast::CLocalDbAdapter> CreateLocalDbAdapter(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);

protected:
    list<CRef<objects::CSeq_loc> > m_OrigSeqLocList;
    CSeqIdListSet m_SeqIdListSet;
    CUnorderedSplitter* m_Splitter;
};



class CSplitSeqAlignMerger : public IAlignmentFactory
{
public:

    CSplitSeqAlignMerger(CUnorderedSplitter* Splitter)
        : m_Splitter(Splitter) { ; }

    string GetName() const { return "split_seq_aligner"; }

    TAlignResultsRef GenerateAlignments(objects::CScope& Scope,
                                        ISequenceSet* Querys,
                                        ISequenceSet* Subjects,
                                        TAlignResultsRef AccumResults);

private:
    CUnorderedSplitter* m_Splitter;

};



END_NCBI_SCOPE

#endif
