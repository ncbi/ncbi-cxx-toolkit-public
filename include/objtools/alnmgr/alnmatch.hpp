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


class CAlnMixMatches : public CObject
{
public:

    /// Typedefs
    typedef int (*TCalcScoreMethod)(const string& s1,
                                    const string& s2,
                                    bool s1_is_prot,
                                    bool s2_is_prot);

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


private:

    friend class CAlnMixMerger;

    static bool x_CompareAlnMatchScores(const CRef<CAlnMixMatch>& aln_match1,
                                        const CRef<CAlnMixMatch>& aln_match2);
        
    
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



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/03/01 17:28:49  todorov
* Rearranged CAlnMix classes
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNMATCH__HPP
