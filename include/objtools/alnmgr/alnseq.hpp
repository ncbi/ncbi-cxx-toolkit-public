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
#include <objtools/alnmgr/alnexception.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


class CAlnMixSeq;
class CAlnMixStarts;
class CAlnMixSegment;
class CAlnMixMatch;
class CAlnMixMerger;
class CBioseq_Handle;
class CScope;
class CDense_seg;


class NCBI_XALNMGR_EXPORT CAlnMixSequences : public CObject
{
public:
    
    // Constructors
    CAlnMixSequences(void);
    CAlnMixSequences(CScope& scope);

    typedef vector<CRef<CAlnMixSeq> > TSeqs;

    const TSeqs& Get        () const { return m_Seqs; };
    TSeqs&       Set        () { return m_Seqs; };

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


    // Sorting algirithms
    void         SortByScore();
    void         SortByChainScore();


    // Rows-related methods
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

    static bool x_CompareScores     (const CRef<CAlnMixSeq>& seq1,
                                     const CRef<CAlnMixSeq>& seq2);
    static bool x_CompareChainScores(const CRef<CAlnMixSeq>& seq1,
                                     const CRef<CAlnMixSeq>& seq2);

    void x_IdentifyAlnMixSeq        (CRef<CAlnMixSeq>& aln_seq,
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



class NCBI_XALNMGR_EXPORT CAlnMixSeq : public CObject
{
public:
    CAlnMixSeq(void);
    ~CAlnMixSeq();

    typedef list<CAlnMixMatch *>          TMatchList;

    int                   m_DsCnt;
    const CBioseq_Handle* m_BioseqHandle;
    CRef<CSeq_id>         m_SeqId;
    int                   m_Score;
    int                   m_ChainScore;
    int                   m_StrandScore;
    bool                  m_IsAA;
    unsigned              m_Width;
    int                   m_Frame;
    bool                  m_PositiveStrand;
    CAlnMixSeq *          m_RefBy;
    CAlnMixSeq *          m_ExtraRow;
    int                   m_ExtraRowIdx;
    CAlnMixSeq *          m_AnotherRow;
    int                   m_DsIdx;
    int                   m_SeqIdx;
    int                   m_ChildIdx;
    int                   m_RowIdx;
    TMatchList            m_MatchList;

    const CAlnMixStarts& GetStarts() const { return *m_Starts; }
    CAlnMixStarts& SetStarts() { return *m_Starts; }

    CSeqVector& GetPlusStrandSeqVector(void)
    {
        if ( !m_PlusStrandSeqVector ) {
            m_PlusStrandSeqVector = new CSeqVector
                (m_BioseqHandle->GetSeqVector(CBioseq_Handle::eCoding_Iupac,
                                              CBioseq_Handle::eStrand_Plus));
        }
        return *m_PlusStrandSeqVector;
    }

    CSeqVector& GetMinusStrandSeqVector(void)
    {
        if ( !m_MinusStrandSeqVector ) {
            m_MinusStrandSeqVector = new CSeqVector
                (m_BioseqHandle->GetSeqVector(CBioseq_Handle::eCoding_Iupac,
                                              CBioseq_Handle::eStrand_Minus));
        }
        return *m_MinusStrandSeqVector;
    }

    void GetSeqString(string& s,
                      TSeqPos start, 
                      TSeqPos len,
                      bool    positive_strand = true)
    {
        if (positive_strand) {
            GetPlusStrandSeqVector().GetSeqData(start, start + len, s);
        } else {
            TSeqPos size = GetMinusStrandSeqVector().size();
            GetMinusStrandSeqVector().GetSeqData(size - (start + len),
                                                 size - start, 
                                                 s);
        }
        if (s.length() != len) {
            string errstr = "Unable to load data for seq-id=\"" + 
                m_SeqId->AsFastaString() + "\" "
                "start=" + NStr::IntToString(start) + " "
                "length=" + NStr::IntToString(len) + ".";
            NCBI_THROW(CAlnException, eInvalidSeqId,
                       errstr);
        }
    }

private:
    CRef<CSeqVector> m_PlusStrandSeqVector;
    CRef<CSeqVector> m_MinusStrandSeqVector;
    auto_ptr<CAlnMixStarts> m_Starts;

    /// forbidden
    CAlnMixSeq(const CAlnMixSeq&);
    CAlnMixSeq& operator=(const CAlnMixSeq&);
};



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_ALNMGR___ALNSEQ__HPP
