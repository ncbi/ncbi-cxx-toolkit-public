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
 * File Description:  CNWAligner thread classes implementation
 *                   
 * ===========================================================================
 *
 */

#include <ncbi_pch.hpp>
#include "nw_aligner_threads.hpp"


BEGIN_NCBI_SCOPE

unsigned int g_nwnw_thread_count = 1;

DEFINE_STATIC_FAST_MUTEX(thread_count_mutex_nw);

bool NW_RequestNewThread(const unsigned int max_threads)
{
    CFastMutexGuard guard(thread_count_mutex_nw);
    if(g_nwnw_thread_count < max_threads) {
        ++g_nwnw_thread_count;
        return true;
    }
    else
        return false;
}

void* CNWAlignerThread_Align::Main()
{
    m_exception.reset(0);

    try {
        m_aligner->x_Align(m_data);
    }

    catch(CException& e) {

        m_exception.reset(new CException(e));
    }

    catch(...) {

        m_exception.reset(new CException (0, 0, 0, CException::eUnknown,
                                          "Unregistered exception caught from "
                                          "CNWAligner::x_Align()"));
    }

    return m_exception.get();
}


void CNWAlignerThread_Align::OnExit()
{
    CFastMutexGuard guard(thread_count_mutex_nw);
    --g_nwnw_thread_count;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/06/29 20:28:48  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
