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
 * File Description:  CMMAligner thread classes implementation
 *                   
 * ===========================================================================
 *
 */

#include <ncbi_pch.hpp>
#include "mm_aligner_threads.hpp"


BEGIN_NCBI_SCOPE

unsigned int g_nwmm_thread_count = 1;

DEFINE_STATIC_FAST_MUTEX(thread_count_mutex);

bool MM_RequestNewThread(const unsigned int max_threads)
{
    CFastMutexGuard guard(thread_count_mutex);
    if(g_nwmm_thread_count < max_threads) {
        ++g_nwmm_thread_count;
        return true;
    }
    else
        return false;
}


CThreadRunOnTop::CThreadRunOnTop (
    const CMMAligner* aligner, const SCoordRect* rect,
    vector<CNWAligner::TScore>* e,
    vector<CNWAligner::TScore>* f,
    vector<CNWAligner::TScore>* g,
    vector<unsigned char>* trace, bool free_corner_fgap ):

    m_aligner(aligner), m_rect(rect), m_E(e), m_F(f),
    m_G(g), m_trace(trace), m_free_corner_fgap(free_corner_fgap)
{
}


void* CThreadRunOnTop::Main()
{
    m_aligner->x_RunTop( *m_rect, *m_E, *m_F, *m_G,
                         *m_trace, m_free_corner_fgap );
 
    return 0;
}


void CThreadRunOnTop::OnExit()
{
    CFastMutexGuard guard(thread_count_mutex);
    --g_nwmm_thread_count;
}

//////////////////

CThreadDoSM::CThreadDoSM( CMMAligner* aligner, const SCoordRect* rect,
                list<CNWAligner::ETranscriptSymbol>::iterator translist_pos,
                bool free_lt_fgap, bool free_rb_fgap ):

    m_aligner(aligner), m_rect(rect),
    m_translist_pos(translist_pos),
    m_free_lt_fgap (free_lt_fgap),
    m_free_rb_fgap (free_rb_fgap)
{
}


void* CThreadDoSM::Main()
{
    m_aligner->x_DoSubmatrix( *m_rect, m_translist_pos,
                              m_free_lt_fgap, m_free_rb_fgap );
 
    return 0;
}

void CThreadDoSM::OnExit()
{
    CFastMutexGuard guard(thread_count_mutex);
    --g_nwmm_thread_count;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/10/14 18:41:31  kapustin
 * Dismiss static keyword as a local-to-compilation-unit flag. Use longer name since unnamed namespace are not everywhere supported
 *
 * Revision 1.2  2003/09/02 22:37:52  kapustin
 * Fix id header tag
 *
 * Revision 1.1  2003/01/22 13:31:33  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
