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
#include <objmgr/impl/mutex_pool.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

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
    _ASSERT(local);
    mutex = local;
    _ASSERT(mutex);
    return true;
}


void CInitMutexPool::ReleaseMutex(CInitMutex_Base& init, CRef<TMutex>& mutex)
{
    _ASSERT(mutex);
    if ( !init ) {
        return;
    }
    CRef<TMutex> local(mutex);
    mutex.Reset();
    _ASSERT(local);
    if ( init.m_Mutex ) {
        CFastMutexGuard guard(m_Pool_Mtx);
        init.m_Mutex.Reset();
        if ( local->ReferencedOnlyOnce() ) {
            m_MutexList.push_back(local);
        }
    }
    else {
        if ( local->ReferencedOnlyOnce() ) {
            CFastMutexGuard guard(m_Pool_Mtx);
            m_MutexList.push_back(local);
        }
    }
    _ASSERT(!mutex);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2004/08/31 14:25:36  vasilche
* Keep mutex in place if object is not initialized yet.
*
* Revision 1.5  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2003/07/01 18:02:37  vasilche
* Removed invalid assert.
* Moved asserts from .hpp to .cpp file.
*
* Revision 1.3  2003/06/25 17:09:28  vasilche
* Fixed locking in CInitMutexPool.
*
* Revision 1.2  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.1  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
*
* ===========================================================================
*/
