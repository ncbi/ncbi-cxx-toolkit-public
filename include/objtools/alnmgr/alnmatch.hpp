#ifndef OBJECTS_ALNMGR___ALNMATCH__HPP
#define OBJECTS_ALNMGR___ALNMATCH__HPP

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
*   Alignment matches
*
*/


#include <objtools/alnmgr/alnseq.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


class CAlnMixMatch;


class NCBI_XALNMGR_EXPORT CAlnMixMatches : public CObject
{
public:

    /// Typedefs
    typedef int (*TCalcScoreMethod)(const string& s1,
                                    const string& s2,
                                    bool s1_is_prot,
                                    bool s2_is_prot,
                                    int gene_code1,
                                    int gene_code2);

    typedef vector<CRef<CAlnMixMatch> > TMatches;

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


    /// Constructor
    CAlnMixMatches(CRef<CAlnMixSequences>& sequences,
                   TCalcScoreMethod calc_score = 0);


    /// Container accessors
    const TMatches& Get() const { return m_Matches; };
    TMatches&       Set() { return m_Matches; };


    /// "Add" a Dense-seg to the existing matches.  This would create
    /// and add new mathces that correspond to the relations in the
    /// given Dense-seg
    void            Add(const CDense_seg& ds, TAddFlags flags = 0);


    /// Modifying algorithms
    void           SortByScore();
    void           SortByChainScore();


private:

    friend class CAlnMixMerger;

    static bool x_CompareScores     (const CRef<CAlnMixMatch>& match1,
                                     const CRef<CAlnMixMatch>& match2);
    static bool x_CompareChainScores(const CRef<CAlnMixMatch>& match1,
                                     const CRef<CAlnMixMatch>& match2);
        
    
    size_t                      m_DsCnt;
    CRef<CScope>                m_Scope;
    TMatches                    m_Matches;
    CRef<CAlnMixSequences>      m_AlnMixSequences;
    CAlnMixSequences::TSeqs&    m_Seqs;
    TCalcScoreMethod            x_CalculateScore;
    TAddFlags                   m_AddFlags;
    bool&                       m_ContainsAA;
    bool&                       m_ContainsNA;
};



class CAlnMixMatch : public CObject
{
public:
    CAlnMixMatch(void)
        : m_Score(0), m_ChainScore(0),
          m_AlnSeq1(0), m_AlnSeq2(0),
          m_Start1(0), m_Start2(0),
          m_Len(0), m_StrandsDiffer(false), m_DsIdx(0)
    {}
    CAlnMixMatch(const CAlnMixMatch& match)
        : m_Score(0), m_ChainScore(0),
          m_AlnSeq1(0), m_AlnSeq2(0),
          m_Start1(0), m_Start2(0),
          m_Len(0), m_StrandsDiffer(false), m_DsIdx(0)
    {
        *this = match;
    }
    CAlnMixMatch& operator=(const CAlnMixMatch& match)
    {
        if ( this != &match ) {
            m_Score = match.m_Score;
            m_ChainScore = match.m_ChainScore;
            m_AlnSeq1 = match.m_AlnSeq1;
            m_AlnSeq2 = match.m_AlnSeq2;
            m_Start1 = match.m_Start1;
            m_Start2 = match.m_Start2;
            m_Len = match.m_Len;
            m_StrandsDiffer = match.m_StrandsDiffer;
            m_DsIdx = match.m_DsIdx;
            if ( m_AlnSeq1 )
                m_MatchIter1 = match.m_MatchIter1;
            if ( m_AlnSeq2 )
                m_MatchIter2 = match.m_MatchIter2;
        }
        return *this;
    }

    bool IsGood(const CAlnMixSeq::TMatchList& list,
                CAlnMixSeq::TMatchList::const_iterator iter) const
    {
        ITERATE ( CAlnMixSeq::TMatchList, it, list ) {
            if ( iter == it )
                return true;
        }
        return iter == list.end();
    }
    bool IsGood(void) const
    {
        if ( m_AlnSeq1 && !IsGood(m_AlnSeq1->m_MatchList, m_MatchIter1) )
            return false;
        if ( m_AlnSeq2 && !IsGood(m_AlnSeq2->m_MatchList, m_MatchIter2) )
            return false;
        return true;
    }
        
    int                              m_Score, m_ChainScore;
    CAlnMixSeq                       * m_AlnSeq1, * m_AlnSeq2;
    TSeqPos                          m_Start1, m_Start2, m_Len;
    bool                             m_StrandsDiffer;
    int                              m_DsIdx;
    CAlnMixSeq::TMatchList::iterator m_MatchIter1, m_MatchIter2;
};



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_ALNMGR___ALNMATCH__HPP
