#ifndef OBJECTS_ALNMGR___ALNMIX__HPP
#define OBJECTS_ALNMGR___ALNMIX__HPP

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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment merger
*
*/

#include <objtools/alnmgr/alnvec.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// forward declarations
class CScope;
class CSeq_id;

class CAlnMixSegment;
class CAlnMixSeq;
class CAlnMixMatch;

class NCBI_XALNMGR_EXPORT CAlnMix : public CSeq_align::SSeqIdChooser
// Note that SSeqIdChooser derives from CObject, so CAlnMix *is* also a CObject.
{
public:

    // constructor
    CAlnMix(void);

    typedef int (*TCalcScoreMethod)(const string& s1,
                                    const string& s2,
                                    bool s1_is_prot,
                                    bool s2_is_prot);

    CAlnMix(CScope& scope,
            TCalcScoreMethod calc_score = 0);
                 

    // destructor
    ~CAlnMix(void);

    enum EAddFlags {
        // Determine score of each aligned segment in the process of mixing
        // (only makes sense if scope was provided at construction time)
        fCalcScore            = 0x01,

        // Force translation of nucleotide rows
        // This will result in an output Dense-seg that has Widths,
        // no matter if the whole alignment consists of nucleotides only.
        fForceTranslation     = 0x02
    };
    typedef int TAddFlags; // binary OR of EMergeFlags

    void Add(const CDense_seg& ds, TAddFlags flags = 0);
    void Add(const CSeq_align& aln, TAddFlags flags = 0);

    enum EMergeFlags {
        fGen2EST              = 0x0001, // otherwise Nucl2Nucl
        fTruncateOverlaps     = 0x0002, // otherwise put on separate rows
        fNegativeStrand       = 0x0004,
        fTryOtherMethodOnFail = 0x0008,
        fGapJoin              = 0x0010, // join equal len segs gapped on refseq
        fMinGap               = 0x0020, // minimize segs gapped on refseq
        fSortSeqsByScore      = 0x0040, // Seqs with better scoring aligns on top
        fQuerySeqMergeOnly    = 0x0080, // Only put the query seq on same row, 
                                        // other seqs from diff densegs go to diff rows
        fPreserveRows         = 0x0100, // Used for mapping sequence to itself
        fFillUnalignedRegions = 0x0200
    };
    typedef int TMergeFlags; // binary OR of EMergeFlags

    void Merge(TMergeFlags flags = 0);

    typedef vector<CConstRef<CDense_seg> >         TConstDSs;
    typedef vector<CConstRef<CSeq_align> >         TConstAlns;

    CScope&            GetScope         (void) const;
    const TConstDSs&   GetInputDensegs  (void) const;
    const TConstAlns&  GetInputSeqAligns(void) const;
    const CDense_seg&  GetDenseg        (void) const;
    const CSeq_align&  GetSeqAlign      (void) const;

private:
    // Prohibit copy constructor and assignment operator
    CAlnMix(const CAlnMix& value);
    CAlnMix& operator=(const CAlnMix& value);

    // CRef<Seq-id> comparison predicate
    struct SSeqIds {
        bool 
        operator() (const CRef<CSeq_id>& id1, const CRef<CSeq_id>& id2) const {
            return (*id1 < *id2);
        }
    };


    typedef map<void *, CConstRef<CDense_seg> >           TConstDSsMap;
    typedef map<void *, CConstRef<CSeq_align> >           TConstAlnsMap;
    typedef vector<CRef<CAlnMixSeq> >                     TSeqs;
    typedef map<CBioseq_Handle, CRef<CAlnMixSeq> >        TBioseqHandleMap;
    typedef map<CRef<CSeq_id>, CRef<CAlnMixSeq>, SSeqIds> TSeqIdMap;
    typedef vector<CRef<CAlnMixMatch> >                   TMatches;
    typedef list<CAlnMixSegment*>                         TSegments;
    typedef list<CRef<CAlnMixSegment> >                   TSegmentsContainer;

    enum ESecondRowFits {
        eSecondRowFitsOk,
        eForceSeparateRow,
        eInconsistentStrand,
        eInconsistentFrame,
        eFirstRowOverlapBelow,
        eFirstRowOverlapAbove,
        eInconsistentOverlap,
        eSecondRowOverlap,
        eSecondRowInconsistency,
        eIgnoreMatch
    };
    typedef int TSecondRowFits;

    TSecondRowFits x_SecondRowFits(CAlnMixMatch * match) const;


    void x_Reset               (void);
    void x_InitBlosum62Map     (void);
    void x_Merge               (void);
    void x_CreateRowsVector    (void);
    void x_CreateSegmentsVector(void);
    void x_CreateDenseg        (void);
    void x_ConsolidateGaps     (TSegmentsContainer& gapped_segs);
    void x_MinimizeGaps        (TSegmentsContainer& gapped_segs);
    void x_IdentifyAlnMixSeq   (CRef<CAlnMixSeq>& aln_seq, const CSeq_id& seq_id);
    void x_SetSeqFrame         (CAlnMixMatch* match, CAlnMixSeq*& seq);

    // SChooseSeqId implementation
    virtual void ChooseSeqId(CSeq_id& id1, const CSeq_id& id2);


    CRef<CDense_seg> x_ExtendDSWithWidths(const CDense_seg& ds);


    static bool x_CompareAlnSeqScores  (const CRef<CAlnMixSeq>& aln_seq1,
                                        const CRef<CAlnMixSeq>& aln_seq2);
    static bool x_CompareAlnMatchScores(const CRef<CAlnMixMatch>& aln_match1,
                                        const CRef<CAlnMixMatch>& aln_match2);
        
    
    void x_SegmentStartItsConsistencyCheck(const CAlnMixSegment& seg,
                                           const CAlnMixSeq&     seq,
                                           const TSeqPos&        start);


    mutable CRef<CScope>        m_Scope;
    TConstDSs                   m_InputDSs;
    TConstAlns                  m_InputAlns;
    TConstDSsMap                m_InputDSsMap;
    TConstAlnsMap               m_InputAlnsMap;
    CRef<CDense_seg>            m_DS;
    CRef<CSeq_align>            m_Aln;
    TAddFlags                   m_AddFlags;
    TMergeFlags                 m_MergeFlags;
    TSeqs                       m_Seqs;
    TMatches                    m_Matches;
    TSegments                   m_Segments;
    TSeqs                       m_Rows;
    list<CRef<CAlnMixSeq> >     m_ExtraRows;
    bool                        m_SingleRefseq;
    bool                        m_IndependentDSs;
    TBioseqHandleMap            m_BioseqHandles;
    TSeqIdMap                   m_SeqIds;
    bool                        m_ContainsAA;
    bool                        m_ContainsNA;
    size_t                      m_MatchIdx;
    TCalcScoreMethod            x_CalculateScore;
};


///////////////////////////////////////////////////////////
///////////////////// Helper Classes //////////////////////
///////////////////////////////////////////////////////////

class CAlnMixSegment : public CObject
{
public:
    // TStarts really belongs in CAlnMixSeq, but had to move here as
    // part of a workaround for Compaq's compiler's bogus behavior
    typedef map<TSeqPos, CRef<CAlnMixSegment> > TStarts;
    typedef map<CAlnMixSeq*, TStarts::iterator> TStartIterators;
        
    TSeqPos         m_Len;
    TStartIterators m_StartIts;
    int             m_DsIdx; // used by the truncate algorithm
};


class CAlnMixSeq : public CObject
{
public:
    CAlnMixSeq(void) 
        : m_DsCnt(0),
          m_Score(0),
          m_StrandScore(0),
          m_Width(1),
          m_Frame(-1),
          m_RefBy(0),
          m_ExtraRow(0),
          m_ExtraRowIdx(0),
          m_AnotherRow(0),
          m_DsIdx(0),
          m_RowIdx(-1)
    {};

    typedef CAlnMixSegment::TStarts TStarts;
    typedef list<CAlnMixMatch *>    TMatchList;

    int                   m_DsCnt;
    const CBioseq_Handle* m_BioseqHandle;
    CRef<CSeq_id>         m_SeqId;
    int                   m_Score;
    int                   m_StrandScore;
    bool                  m_IsAA;
    unsigned              m_Width;
    int                   m_Frame;
    bool                  m_PositiveStrand;
    TStarts               m_Starts;
    CAlnMixSeq *          m_RefBy;
    CAlnMixSeq *          m_ExtraRow;
    int                   m_ExtraRowIdx;
    CAlnMixSeq *          m_AnotherRow;
    int                   m_SeqIdx;
    int                   m_DsIdx;
    int                   m_RowIdx;
    TStarts::iterator     m_StartIt;
    TMatchList            m_MatchList;

    CSeqVector& GetSeqVector(void) {
        if ( !m_SeqVector ) {
            m_SeqVector = new CSeqVector
                (m_BioseqHandle->GetSeqVector(CBioseq_Handle::eCoding_Iupac));
        }
        return *m_SeqVector;
    }
private:
    CRef<CSeqVector> m_SeqVector;
};


class CAlnMixMatch : public CObject
{
public:
    CAlnMixMatch(void)
        : m_Score(0), m_Start1(0), m_Start2(0),
          m_Len(0), m_StrandsDiffer(false), m_DsIdx(0)
    {};
        
    int                              m_Score;
    CAlnMixSeq                       * m_AlnSeq1, * m_AlnSeq2;
    TSeqPos                          m_Start1, m_Start2, m_Len;
    bool                             m_StrandsDiffer;
    int                              m_DsIdx;
    CAlnMixSeq::TMatchList::iterator m_MatchIter1, m_MatchIter2;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CScope& CAlnMix::GetScope() const
{
    return const_cast<CScope&>(*m_Scope);
}


inline
const CAlnMix::TConstDSs& CAlnMix::GetInputDensegs() const
{
    return m_InputDSs;
}


inline
const CAlnMix::TConstAlns& CAlnMix::GetInputSeqAligns() const
{
    return m_InputAlns;
}


inline
const CDense_seg& CAlnMix::GetDenseg() const
{
    if (!m_DS) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::GetDenseg(): "
                   "Dense_seg is not available until after Merge()");
    }
    return *m_DS;
}


inline
const CSeq_align& CAlnMix::GetSeqAlign() const
{
    if (!m_Aln) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::GetSeqAlign(): "
                   "Seq_align is not available until after Merge()");
    }
    return *m_Aln;
}



///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.49  2005/02/16 21:27:48  todorov
* Abstracted the CalculateScore method so that it could be delegated to
* the caller.
* Fixed a few signed/unsigned comparison warnings.
*
* Revision 1.48  2004/11/08 13:36:13  todorov
* Usage of OM now only depend on whether scope was provided at construction time
*
* Revision 1.47  2004/11/02 18:32:15  todorov
* Changed (mostly shortened) a few internal members' names
* for convenience and consistency
*
* Revision 1.46  2004/11/02 18:02:34  todorov
* CAlnMixSeq += m_ExtraRowIdx
*
* Revision 1.45  2004/10/13 17:50:56  todorov
* rm contitional compilation logic
*
* Revision 1.44  2004/10/13 16:30:33  todorov
* + conditionally compiled x_SegmentStartItsConsistencyCheck and m_MatchIdx
*
* Revision 1.43  2004/10/12 19:44:29  rsmith
* make x_CompareAlnSeqScores arguments match the container it compares on.
*
* Revision 1.42  2004/09/27 16:18:17  todorov
* + truncate segments of sequences on multiple frames
*
* Revision 1.41  2004/09/22 17:00:53  todorov
* +CAlnMix::fPreserveRows
*
* Revision 1.40  2004/09/22 14:26:43  todorov
* changed TSegments & TSegmentsContainer from vectors to lists
*
* Revision 1.39  2004/06/23 18:31:20  todorov
* Calculate the prevailing strand per row (using scores)
*
* Revision 1.38  2004/05/25 16:00:20  todorov
* remade truncation of overlaps
*
* Revision 1.37  2004/03/30 23:27:32  todorov
* Switch from CAlnMix::x_RankSeqId() to CSeq_id::BestRank()
*
* Revision 1.36  2004/03/15 17:46:03  todorov
* Avoid multiple inheritance since Workshop has problems placing the vtables.
* Now CAlnMix inherits from SSeqIdChooser which inherits from CObject
*
* Revision 1.35  2004/03/09 17:15:46  todorov
* + SSeqIdChooser implementation
*
* Revision 1.34  2003/12/22 18:30:38  todorov
* ObjMgr is no longer created internally. Scope should be passed as a reference in the ctor
*
* Revision 1.33  2003/12/08 21:28:04  todorov
* Forced Translation of Nucleotide Sequences
*
* Revision 1.32  2003/11/03 14:43:52  todorov
* Use CDense_seg::Validate()
*
* Revision 1.31  2003/08/20 19:35:41  todorov
* Added x_ValidateDenseg
*
* Revision 1.30  2003/08/20 15:06:25  todorov
* include path fixed
*
* Revision 1.29  2003/08/20 14:35:14  todorov
* Support for NA2AA Densegs
*
* Revision 1.28  2003/06/26 21:35:53  todorov
* + fFillUnalignedRegions
*
* Revision 1.27  2003/06/25 15:17:26  todorov
* truncation consistent for the whole segment now
*
* Revision 1.26  2003/06/24 15:24:28  todorov
* added optional truncation of overlaps
*
* Revision 1.25  2003/06/09 20:54:12  todorov
* Use of ObjMgr is now optional
*
* Revision 1.24  2003/06/02 16:01:38  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.23  2003/05/30 17:42:45  todorov
* x_CreateSegmentsVector now uses a stack to order the segs
*
* Revision 1.22  2003/05/09 16:41:46  todorov
* Optional mixing of the query sequence only
*
* Revision 1.21  2003/03/28 16:47:22  todorov
* Introduced TAddFlags (fCalcScore for now)
*
* Revision 1.20  2003/03/26 16:38:31  todorov
* mix independent densegs
*
* Revision 1.19  2003/03/10 22:12:10  todorov
* fixed x_CompareAlnMatchScores callback
*
* Revision 1.18  2003/03/05 17:42:28  todorov
* Allowing multiple mixes + general case speed optimization
*
* Revision 1.17  2003/02/11 21:32:37  todorov
* fMinGap optional merging algorithm
*
* Revision 1.16  2003/01/23 16:30:36  todorov
* Moved calc score to alnvec
*
* Revision 1.15  2003/01/22 20:13:02  vasilche
* Changed use of CBioseqHandle::GetSeqVector() method.
*
* Revision 1.14  2003/01/10 00:42:42  todorov
* Optional sorting of seqs by score
*
* Revision 1.13  2003/01/02 16:39:57  todorov
* Added accessors to the input data
*
* Revision 1.12  2002/12/26 12:38:08  dicuccio
* Added Win32 export specifiers
*
* Revision 1.11  2002/12/24 16:20:57  todorov
* Fixed initializing of a CTypeConstIterator
*
* Revision 1.10  2002/12/23 18:03:41  todorov
* Support for putting in and getting out Seq-aligns
*
* Revision 1.9  2002/12/19 15:09:07  ucko
* Reorder some definitions to make Compaq's C++ compiler happy.
*
* Revision 1.8  2002/12/19 00:06:55  todorov
* Added optional consolidation of segments that are gapped on the query.
*
* Revision 1.7  2002/12/18 03:47:05  todorov
* created an algorithm for mixing alignments that share a single sequence.
*
* Revision 1.6  2002/11/08 19:43:33  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.5  2002/11/04 21:28:57  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/10/25 20:02:20  todorov
* new fTryOtherMethodOnFail flag
*
* Revision 1.3  2002/10/24 21:31:33  todorov
* adding Dense-segs instead of Seq-aligns
*
* Revision 1.2  2002/10/08 18:02:09  todorov
* changed the aln lst input param
*
* Revision 1.1  2002/08/23 14:43:50  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNMIX__HPP
