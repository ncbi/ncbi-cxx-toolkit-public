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
* File Description: cpg -- c++ cpg island finder based upon algorithm in
*     Takai, D. & Jones, PA.  "Comprehensive analysis of CpG islands in
*     human chromosomes 21 and 22."  PNAS, 2002.
*
* ===========================================================================*/

#include <ncbi_pch.hpp>
#include <algo/sequence/cpg.hpp>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
// PRE : const char* of iupac sequence (null-terminated), window size,
// minimum island length, min GC percentage, min observed/expected CpG ratio
// POST: cpg islands calculated for the given bioseq using the given parameters
CCpGIslands::CCpGIslands(const char *seq, TSeqPos length,
                         int window, int minLen, double GC, double CpG) :
    m_Seq(seq), m_SeqLength(length)
{
    Calc(window, minLen, GC, CpG);
}

///////////////////////////////////////////////////////////////////////////////
// PRE : windowsize, min length, %GC, obs/exp CpG ratio
// POST: islands recalculated for new params
void CCpGIslands::Calc(int window, int minLen, double GC, double CpG)
{
    m_Isles.clear();//clear old islands

    m_WindowSize = window;
    m_MinIsleLen = minLen;
    m_GC = (int) (GC * 100);
    m_CpG = (int) (CpG * 100);
    
    SCpGIsland isle;
    isle.m_Start = 0;

    while (x_SlideToHit(isle)) {
        if (x_ExtendHit(isle)) {
            m_Isles.push_back(isle);
        }
        isle.m_Start = isle.m_Stop + 1;
    }
}

///////////////////////////////////////////////////////////////////////////////
// PRE : position to be added, cgIsland information
// POST: cgIsle adjusted according to the nucleotide at position i & i-1
void CCpGIslands::x_AddPosition(TSeqPos i, SCpGIsland &isle)
{
    switch(m_Seq[i]) {
    case 'A': isle.m_A++; break;
    case 'C': isle.m_C++; break;
    case 'G': isle.m_G++;
        if (i > 0  &&  m_Seq[i-1] == 'C') {
            isle.m_CG++;
        }
        break;
    case 'T': isle.m_T++; break;
    case 'N': isle.m_N++; break;
    }
}

///////////////////////////////////////////////////////////////////////////////
// PRE : position to be removed, cgIsland information
// POST: cgIsle adjusted according to the nucleotide at position i & i-1
void CCpGIslands::x_RemovePosition(TSeqPos i, SCpGIsland &isle)
{
    switch(m_Seq[i]) {
    case 'A': isle.m_A--; break;
    case 'C': isle.m_C--; break;
    case 'G': isle.m_G--;
        if (i > 0  &&  m_Seq[i-1] == 'C') {
            isle.m_CG--;
        }
        break;
    case 'T': isle.m_T--; break;
    case 'N': isle.m_N--; break;
    }
}

///////////////////////////////////////////////////////////////////////////////
// PRE : cpg island structure w/ start & stop specified
// POST: isle filled with stats (#a's, c's, etc.) for window [start, stop]
void CCpGIslands::x_CalcWindowStats(SCpGIsland &isle)
{
    isle.m_A = isle.m_T = isle.m_G = isle.m_C = isle.m_N = isle.m_CG = 0;

    for (TSeqPos i = isle.m_Start; i <= isle.m_Stop; i++) {
        x_AddPosition(i, isle);
    }
}

///////////////////////////////////////////////////////////////////////////////
// PRE : win.m_Start field species where to start looking
// POST: whether or not we found a window further down the sequence that
// meets the mimimum criteria; if so, 'win' is set to that window
bool CCpGIslands::x_SlideToHit(SCpGIsland &win)
{
    bool inIsland, done;

    win.m_Stop = win.m_Start + m_WindowSize - 1;

    if (win.m_Stop >= m_SeqLength) {
        return false;
    }

    x_CalcWindowStats(win);

    inIsland = false;
    done = false;

    while (win.m_Stop < m_SeqLength  &&  !x_IsIsland(win)) {
        // remove 1 nt from left side
        x_RemovePosition(win.m_Start, win);

        // advance
        ++win.m_Start;
        ++win.m_Stop;

        if (win.m_Stop < m_SeqLength) {
            // add 1 nt onto right side
            x_AddPosition(win.m_Stop, win);
        }
    }
    
    return x_IsIsland(win);
}

///////////////////////////////////////////////////////////////////////////////
// PRE : window that meets the GC & CpG criteria
// POST: whether or not the island can be extended to reach at least the
// minimum length; if so, isle is set to that window
bool CCpGIslands::x_ExtendHit(SCpGIsland &isle)
{
    SCpGIsland win = isle;

    //jump by 200bp increments
    while (win.m_Stop + m_WindowSize < m_SeqLength  &&  x_IsIsland(win)) {
        win.m_Start += m_WindowSize;
        win.m_Stop += m_WindowSize;
        x_CalcWindowStats(win);
    }

    //if we overshot, slide back by 1bp increments
    while (!x_IsIsland(win)) {
        x_RemovePosition(win.m_Stop, win);
        --win.m_Start;
        --win.m_Stop;
        x_AddPosition(win.m_Start, win);
    }

    //trim ends of entire island until we're above criteria again
    isle.m_Stop = win.m_Stop;
    x_CalcWindowStats(isle);
    while(!x_IsIsland(isle)  &&  (isle.m_Start < isle.m_Stop)) {
        x_RemovePosition(isle.m_Stop, isle);
        x_RemovePosition(isle.m_Start, isle);
        --isle.m_Stop;
        ++isle.m_Start;
    }

    if (isle.m_Start >= isle.m_Stop) {//in case we trimmed to nothing
        isle.m_Stop = isle.m_Start;
        return false;
    }
    return (isle.m_Stop - isle.m_Start + 1 > m_MinIsleLen);
}

///////////////////////////////////////////////////////////////////////////////
// PRE : range in base pairs
// POST: any adjacent islands within the specified range are merged
void CCpGIslands::MergeIslesWithin(TSeqPos range)
{
    TIsles::iterator prev = m_Isles.end();

    NON_CONST_ITERATE(TIsles, i, m_Isles) {
        if (prev != m_Isles.end()  &&
            i->m_Start - prev->m_Stop <= range) {
            i->m_Start = prev->m_Start;
            x_CalcWindowStats(*i);
            m_Isles.erase(prev);
        }
        prev = i;
    }
}

END_NCBI_SCOPE

/*===========================================================================
* $Log$
* Revision 1.4  2004/05/21 21:41:04  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.3  2003/12/12 20:05:19  johnson
* refactoring to accommodate MSVC 7
*
* Revision 1.2  2003/07/21 15:53:35  johnson
* added reference in header comment
*
* Revision 1.1  2003/06/17 15:33:33  johnson
* initial revision
*
*============================================================================
*/
