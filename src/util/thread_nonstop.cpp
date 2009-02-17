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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB libarary types implementations.
 *
 */

#include <ncbi_pch.hpp>
#include <util/thread_nonstop.hpp>
#include <corelib/ncbi_system.hpp>


BEGIN_NCBI_SCOPE


CThreadNonStop::CThreadNonStop(unsigned run_delay,
                               unsigned)
: m_RunInterval(run_delay),
  m_StopSignal(0, 10000000)
{
}

bool CThreadNonStop::IsStopRequested() const
{
    return m_StopFlag.Get() != 0;
}

void CThreadNonStop::RequestStop()
{
    m_StopFlag.Add(1);
    m_StopSignal.Post();
}

void CThreadNonStop::RequestDoJob()
{
    m_StopSignal.Post();
}

void* CThreadNonStop::Main(void)
{
    while (1) {
        
        DoJob();

        bool flag = m_StopSignal.TryWait(m_RunInterval, 0);
        if (flag) {
            if (m_StopFlag.Get() != 0) {
                break; 
            }
        }       
    
    } // for flag

    return 0;
}


END_NCBI_SCOPE
