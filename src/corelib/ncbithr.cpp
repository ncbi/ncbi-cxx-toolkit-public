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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Multithreading:
 *      CMutex,   CMutexGuard
 *      CRWLock,  CRWLockGuard, CReadLockGuard, CWriteLockGuard, 
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2000/12/09 00:04:21  vakatov
 * First draft:  Fake implementation of Mutex and RW-lock API
 *
 * ===========================================================================
 */

#include <corelib/ncbithr.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  Mutex
//


/////////////////////////////////////////////////////////////////////////////
//  CInternalMutex::
//    ***  NOT REALLY IMPLEMENTED YET!  ***
//

class CInternalMutex
{
public:
    CInternalMutex(void) : m_Counter(0)  {}
    int m_Counter;
};




/////////////////////////////////////////////////////////////////////////////
//  CMutex::
//    ***  NOT REALLY IMPLEMENTED YET!  ***
//

CMutex::CMutex(void)
    : m_Mutex(*new CInternalMutex)
{
    return;
}


CMutex::~CMutex(void)
{
    if ( m_Mutex.m_Counter ) {
        throw runtime_error("~CMutex():  mutex is still locked");
    }
    delete &m_Mutex;
}


void CMutex::Lock(void)
{
    m_Mutex.m_Counter++;
}


bool CMutex::TryLock(void)
{
    Lock();
    return true;
}


void CMutex::Unlock(void)
{
    if ( !m_Mutex.m_Counter ) {
        throw runtime_error("CMutex::Unlock():  mutex is already unlocked");
    }
    m_Mutex.m_Counter++;
}


CMutex::CMutex(const CMutex&) : m_Mutex(*new CInternalMutex)  {}
CMutex& CMutex::operator= (const CMutex&)
{ return *this; }




/////////////////////////////////////////////////////////////////////////////
//  CMutexGuard::
//

CMutexGuard::CMutexGuard(const CMutexGuard&)  {}
CMutexGuard& CMutexGuard::operator= (const CMutexGuard&)
{ return *(CMutexGuard*)0; }




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//  RW-lock
//


/////////////////////////////////////////////////////////////////////////////
//  CInternalRWLock::
//    ***  NOT REALLY IMPLEMENTED YET!  ***
//

class CInternalRWLock
{
public:
    CInternalRWLock(void) : m_Counter(0)  {}
    int m_Counter;
};



/////////////////////////////////////////////////////////////////////////////
//  CRWLock::
//    ***  NOT REALLY IMPLEMENTED YET!  ***
//

CRWLock::CRWLock(void)
    : m_RW(*new CInternalRWLock)
{
    return;
}


CRWLock::~CRWLock(void)
{
    if ( m_RW.m_Counter ) {
        throw runtime_error("~CRWLock():  RW-lock is still locked");
    }
    delete &m_RW;
}


void CRWLock::ReadLock(void)
{
    if (m_RW.m_Counter >= 0)
        m_RW.m_Counter++;
    else
        m_RW.m_Counter--;
}


void CRWLock::WriteLock(void)
{
    if (m_RW.m_Counter >= 0) 
        throw runtime_error("CRWLock::WriteLock():  W-lock after R-lock");
    else
        m_RW.m_Counter--;
}


bool CRWLock::TryReadLock(void)
{
    ReadLock();
    return true;
}


bool CRWLock::TryWriteLock(void)
{
    WriteLock();
    return true;
}


void CRWLock::Unlock(void)
{
    if (m_RW.m_Counter == 0) {
        throw runtime_error("CRWLock::Unlock():  RW-lock is already unlocked");
    }

    if (m_RW.m_Counter > 0)
        m_RW.m_Counter--;
    else
        m_RW.m_Counter++;

}


CRWLock::CRWLock(const CRWLock&) : m_RW(*new CInternalRWLock)  {}
CRWLock& CRWLock::operator= (const CRWLock&)
{ return *this; }



/////////////////////////////////////////////////////////////////////////////
//  CRWLockGuard::
//

CRWLockGuard::CRWLockGuard(const CRWLockGuard&)  {}
CRWLockGuard& CRWLockGuard::operator= (const CRWLockGuard&)
{ return *(CRWLockGuard*)0; }




/////////////////////////////////////////////////////////////////////////////
//  CReadLockGuard::
//

CReadLockGuard::CReadLockGuard(const CReadLockGuard&)
    : CRWLockGuard(*(CRWLock*)0)  {}
CReadLockGuard& CReadLockGuard::operator= (const CReadLockGuard&)
{ return *(CReadLockGuard*)0; }




/////////////////////////////////////////////////////////////////////////////
//  CWriteLockGuard::
//

CWriteLockGuard::CWriteLockGuard(const CWriteLockGuard&)
    : CRWLockGuard(*(CRWLock*)0)  {}
CWriteLockGuard& CWriteLockGuard::operator= (const CWriteLockGuard&)
{ return *(CWriteLockGuard*)0; }



END_NCBI_SCOPE
