/* $Id$
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
* Authors:  Yuri Kapustin
*
* File Description:  Base class for spliced aligners.
*                   
* ===========================================================================
*
*/


#include <ncbi_pch.hpp>

#include "messages.hpp"
#include <algo/align/nw/nw_formatter.hpp>
#include <algo/align/nw/nw_spliced_aligner.hpp>
#include <algo/align/nw/align_exception.hpp>

BEGIN_NCBI_SCOPE


CSplicedAligner::CSplicedAligner():
    m_IntronMinSize(GetDefaultIntronMinSize()),
    m_cds_start(0), m_cds_stop(0)
{
    SetEndSpaceFree(true, true, false, false);
}
    

CSplicedAligner::CSplicedAligner(const char* seq1, size_t len1,
                                 const char* seq2, size_t len2)
    : CBandAligner(seq1, len1, seq2, len2),
      m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}


CSplicedAligner::CSplicedAligner(const string& seq1, const string& seq2)
    : CBandAligner(seq1, seq2),
      m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}


void CSplicedAligner::SetWi  (unsigned char splice_type, TScore value)
{
    if(splice_type < GetSpliceTypeCount()) {
        x_GetSpliceScores()[splice_type]  = value;
    }
    else 
    {
        NCBI_THROW(CAlgoAlignException,
                   eInvalidSpliceTypeIndex,
                   g_msg_InvalidSpliceTypeIndex);
    }
}


CSplicedAligner::TScore CSplicedAligner::GetWi  (unsigned char splice_type)
{
    if(splice_type < GetSpliceTypeCount()) {
        return x_GetSpliceScores()[splice_type];
    }
    else 
    {
        NCBI_THROW(CAlgoAlignException,
                   eInvalidSpliceTypeIndex,
                   g_msg_InvalidSpliceTypeIndex);
    }
}


bool CSplicedAligner::x_CheckMemoryLimit()
{
    return CNWAligner::x_CheckMemoryLimit();
}


size_t GetSplicePriority(const  char * dnr, const char* acc)
{
    // GA-AG, GT-TG
    const size_t rv (dnr[0] == 'G' && acc[1] == 'G' && dnr[1] == acc[0] &&
                     (dnr[1] == 'A' || dnr[1] == 'T')? 1: 0);
    return rv;
}


// Prefer experimentally verified non-consensus
// splices among equally scoring alternatives
void CSplicedAligner::CheckPreferences(void)
{
    if(m_Transcript.size() == 0) {
        NCBI_THROW(CAlgoAlignException, eNoSeqData, g_msg_NoAlignment);
        
    }
    
    const char * p1 (GetSeq1()), * p2 (GetSeq2());
    for(int t (m_Transcript.size() - 1), csq_matches(0); t >= 0; --t) {
        
        switch(m_Transcript[t]) {
            
        case eTS_Match:
            ++csq_matches;
            ++p1;
            ++p2;
            break;
            
        case eTS_Replace:
            csq_matches = 0;
            ++p1;
            ++p2;
            break;
            
        case eTS_Insert:
#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY
        case eTS_SlackInsert:
#endif
            csq_matches = 0;
            ++p2;
            break;
            
        case eTS_Delete:
#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY
        case eTS_SlackDelete:
#endif
            csq_matches = 0;
            ++p1;
            break;
            
        case eTS_Intron: {
            int ti (t - 1);
            for(; ti >= 0 && m_Transcript[ti] == eTS_Intron; --ti);
            const int ilen (t - ti);
            const char * p2e (p2 + ilen);
            if(CNWFormatter::SSegment::s_IsConsensusSplice(p2, p2e - 2, true)) {
                p2 += ilen;
                t = ti + 1;
            }
            else {

                size_t maxpr (0);
                int jmaxpr (0);
                // explore the left of the splice
                int j;
                for(j = -1; j >= -csq_matches && *(p1 + j) == *(p2e + j); --j) {
                    const size_t pr (GetSplicePriority(p2 + j, p2e + j - 2));
                    if(pr > maxpr) {
                        maxpr = pr;
                        jmaxpr = j;
                    }
                }

                // explore the right of the splice
                for(j = 1; j <= ti + 1 && m_Transcript[ti - j + 1] == eTS_Match
                        && *(p1 + j - 1) == *(p2 + j - 1); ++j)
                    {
                        const size_t pr (GetSplicePriority(p2 + j, p2e + j - 2));
                        if(pr > maxpr) {
                            maxpr = pr;
                            jmaxpr = j;
                        }
                    }

                if(jmaxpr == 0) {
                    p2 += ilen;
                    t = ti + 1;
                }
                else {
                    // adjust the splice site
                    const int incr (jmaxpr < 0? -1: 1);
                    for(int k (0); k != jmaxpr; k += incr) {
                        swap(m_Transcript[t-k], m_Transcript[ti-k]);
                    }
                    t = ti - jmaxpr + 1;
                    p1 += jmaxpr;
                    p2 = p2 + ilen + jmaxpr;
                }
            }
            csq_matches = 0;
        }
        break;
            
        default:
            NCBI_THROW(CAlgoAlignException, eInternal, "Unexpected transcript symbol");
        }
    }
}

    
END_NCBI_SCOPE
