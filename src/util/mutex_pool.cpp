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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*           Pool of mutexes for initialization of objects
*
*/

#include <ncbi_pch.hpp>
#include <util/mutex_pool.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// CInitMutexPool
/////////////////////////////////////////////////////////////////////////////

CInitMutexPool::CInitMutexPool(void)
{
}


CInitMutexPool::~CInitMutexPool(void)
{
}


bool CInitMutexPool::AcquireMutex(CInitMutex_Base& init, CRef<TMutex>& mutex)
{
    _ASSERT(!mutex);
    CRef<TMutex> local(init.m_Mutex);
    if ( !local ) {
        CFastMutexGuard guard(m_Pool_Mtx);
        if ( init )
            return false;
        local = init.m_Mutex;
        if ( !local ) {
            if ( m_MutexList.empty() ) {
                local.Reset(new TMutex(*this));
                local->DoDeleteThisObject();
            }
            else {
                local = m_MutexList.front();
                m_MutexList.pop_front();
            }
            init.m_Mutex = local;
        }
    }
    local.Swap(mutex);
    _ASSERT(mutex);
    return true;
}


void CInitMutexPool::ReleaseMutex(CInitMutex_Base& init, CRef<TMutex>& mutex)
{
    _ASSERT(mutex);
    if ( !init ) {
        return;
    }
    CFastMutexGuard guard(m_Pool_Mtx);
    CRef<TMutex> local;
    local.Swap(mutex);
    _ASSERT(local);
    init.m_Mutex.Reset();
    if ( local->ReferencedOnlyOnce() ) {
        m_MutexList.push_back(local);
    }
    _ASSERT(!mutex);
}


END_NCBI_SCOPE
