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
* Author: Philip Johnson
*
* File Description: header file for c++ cpg-finding functions
*
* ===========================================================================*/
#ifndef ALGO_SEQUENCE___CPG__HPP
#define ALGO_SEQUENCE___CPG__HPP

#include <corelib/ncbistd.hpp>
#include <list>

BEGIN_NCBI_SCOPE

/*---------------------------------------------------------------------------*/

struct NCBI_XALGOSEQ_EXPORT SCpGIsland {
    TSeqPos m_Start;
    TSeqPos m_Stop;
    unsigned int m_CG;
    unsigned int m_A;
    unsigned int m_C;
    unsigned int m_G;
    unsigned int m_T;
    unsigned int m_N;
};

/*---------------------------------------------------------------------------*/

class NCBI_XALGOSEQ_EXPORT CCpGIslands
{
public:
    typedef list<SCpGIsland> TIsles;

    CCpGIslands(const char* seq, TSeqPos seqLength, int window = 200,
                int minLen = 200, unsigned int GC = 50, unsigned int CpG = 60);
    void Calc(int windowSize, int minLen, unsigned int GC, unsigned int CpG);
    void MergeIslesWithin(unsigned int range);

    const TIsles& GetIsles(void) const;
        
private:
    TIsles m_Isles;

    const char* m_Seq;
    TSeqPos m_SeqLength;

    unsigned int m_WindowSize;
    unsigned int m_MinIsleLen;
    unsigned int m_GC;
    unsigned int m_CpG;

    //prohibit copy constructor and assignment operator
    CCpGIslands(const CCpGIslands&);
    CCpGIslands& operator=(const CCpGIslands&);

    bool x_IsIsland(const SCpGIsland &isle) const;

    void x_AddPosition(TSeqPos i, SCpGIsland &isle);
    void x_RemovePosition(TSeqPos i, SCpGIsland &isle);
    void x_CalcWindowStats(SCpGIsland &isle);
    bool x_SlideToHit(SCpGIsland &isle);
    bool x_ExtendHit(SCpGIsland &isle);
};


///////////////////////////////////////////////////////////////////////////////
// PRE : none
// POST: list of islands
inline
const CCpGIslands::TIsles& CCpGIslands::GetIsles(void) const
{
    return m_Isles;
}

///////////////////////////////////////////////////////////////////////////////
// PRE : island structure filled
// POST: whether or not we consider this to be a CpG island
inline
bool CCpGIslands::x_IsIsland(const SCpGIsland &isle) const
{
    TSeqPos len = isle.m_Stop - isle.m_Start + 1;

    return ((100 * (isle.m_C + isle.m_G) > m_GC * len)  &&
            ((100 * isle.m_CG * len) > (m_CpG * isle.m_C * isle.m_G)));
}


END_NCBI_SCOPE

#endif /*ALGO_SEQUENCE___CPG__HPP*/
