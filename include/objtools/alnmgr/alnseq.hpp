#ifndef OBJECTS_ALNMGR___ALNSEQ__HPP
#define OBJECTS_ALNMGR___ALNSEQ__HPP

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
*   Alignment sequences
*
*/


#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/alnmgr/alnsegments.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


class CAlnMixSeq;
class CAlnMixSegment;
class CAlnMixMatch;
class CAlnMixMerger;
class CBioseq_Handle;
class CScope;
class CDense_seg;


class CAlnMixSequences : public CObject
{
public:
    
    // Constructors
    CAlnMixSequences(void);
    CAlnMixSequences(CScope& scope);

    typedef vector<CRef<CAlnMixSeq> > TSeqs;

    const TSeqs& Get        () const { return m_Seqs; };
    TSeqs&       Set        () { return m_Seqs; };

    void         SortByScore();

    enum EAddFlags {
        // Determine score of each aligned segment in the process of mixing
        // (only makes sense if scope was provided at construction time)
        fCalcScore            = 0x01,

        // Force translation of nucleotide rows
        // This will result in an output Dense-seg that has Widths,
        // no matter if the whole alignment consists of nucleotides only.
        fForceTranslation     = 0x02,

        // Used for mapping sequence to itself
        fPreserveRows         = 0x04 
    };
    typedef int TAddFlags; // binary OR of EMergeFlags

    void         Add        (const CDense_seg& ds, TAddFlags flags = 0);

    void         BuildRows();
    void         InitRowsStartIts();
    void         InitExtraRowsStartIts();
    void         RowsStartItsContsistencyCheck(size_t match_idx);

private:
    friend class CAlnMix;
    friend class CAlnMixMatches;
    friend class CAlnMixSegments;
    friend class CAlnMixMerger;

    typedef map<CBioseq_Handle, CRef<CAlnMixSeq> >        TBioseqHandleMap;

    // CRef<Seq-id> comparison predicate
    struct SSeqIds {
        bool 
        operator() (const CRef<CSeq_id>& id1, const CRef<CSeq_id>& id2) const {
            return (*id1 < *id2);
        }
    };
    typedef map<CRef<CSeq_id>, CRef<CAlnMixSeq>, SSeqIds> TSeqIdMap;

    static bool x_CompareAlnSeqScores  (const CRef<CAlnMixSeq>& aln_seq1,
                                        const CRef<CAlnMixSeq>& aln_seq2);

    void x_IdentifyAlnMixSeq           (CRef<CAlnMixSeq>& aln_seq,
                                        const CSeq_id& seq_id);

    size_t                          m_DsCnt;
    map<const CDense_seg*,
        vector<CRef<CAlnMixSeq> > > m_DsSeq;
    CRef<CScope>                    m_Scope;
    TSeqs                           m_Seqs;
    TSeqIdMap                       m_SeqIds;
    TBioseqHandleMap                m_BioseqHandles;
    bool                            m_ContainsAA;
    bool                            m_ContainsNA;
    vector<CRef<CAlnMixSeq> >       m_Rows;
    list<CRef<CAlnMixSeq> >         m_ExtraRows;
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
    typedef list<CAlnMixMatch *>                TMatchList;

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



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2005/03/10 19:33:00  todorov
* Moved a few routines out of the merger to their corresponding classes
*
* Revision 1.1  2005/03/01 17:28:49  todorov
* Rearranged CAlnMix classes
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNSEQ__HPP
