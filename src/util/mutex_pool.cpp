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


CRef<CInitMutexPool::TMutex> CInitMutexPool::AcquireMutex(CInitMutex_Base& init)
{
    CRef<TMutex> ret = init.m_Mutex;
    if ( !ret ) {
        CFastMutexGuard guard(m_Pool_Mtx);
        if ( !init.m_Mutex ) {
            if ( m_MutexList.empty() ) {
                init.m_Mutex.Reset(new TMutex(*this));
            }
            else {
                init.m_Mutex = m_MutexList.front();
                m_MutexList.pop_front();
            }
        }
        ret = init.m_Mutex;
    }
    return ret;
}


void CInitMutexPool::ReleaseMutex(CInitMutex_Base& init)
{
    _ASSERT(init);
    CRef<TMutex> mutex = init.m_Mutex;
    if ( mutex ) {
        CFastMutexGuard guard(m_Pool_Mtx);
        init.m_Mutex.Reset();
        if ( mutex->ReferencedOnlyOnce() ) {
            m_MutexList.push_back(mutex);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
*
* ===========================================================================
*/
