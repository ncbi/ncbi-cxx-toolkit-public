#ifndef NCBI_THREAD__HPP
#define NCBI_THREAD__HPP

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
 * Revision 1.1  2000/12/09 00:03:26  vakatov
 * First draft:  Mutex and RW-lock API
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Mutex
//


//
// Forward declaration of internal (platform-dependent) mutex representation
//

class CInternalMutex;


//
//  CMutex
//

class CMutex
{
public:
    // 'ctors
    CMutex(void);
    ~CMutex(void);

    // Acquire the mutex. If the mutex is already acquired by
    // another thread, then wait until it is released.
    void Lock(void);

    // Try to acquire the mutex. Return immediately.
    // Return FALSE if the mutex is already acquired by another thread.
    // Return TRUE  if the mutex has been successfully acquired.
    bool TryLock(void);

    // Release the mutex.
    void Unlock(void);

private:
    CInternalMutex& m_Mutex;  // platform-dependent mutex data

    // Disallow assignment and copy constructor
    CMutex(const CMutex&);
    CMutex& operator= (const CMutex&);
};



//
//  CMutexGuard
//

class CMutexGuard
{
public:
    // Acquire the mutex;  register it to be released by the guard destructor.
    CMutexGuard(CMutex& mtx) : m_Mutex(&mtx) { m_Mutex->Lock(); }

    // Release the mutex, if it was (and still is) successfully acquired.
    ~CMutexGuard(void)  { if (m_Mutex) m_Mutex->Unlock(); }

    // Register the mutex to be released by the guard destructor.
    enum EDoNotLock { eDoNotLock };
    CMutexGuard(CMutex& mtx, EDoNotLock) : m_Mutex(&mtx) {}

    // Release the mutex right now (do not release it in the guard destructor).
    void Release(void) { m_Mutex->Unlock(); }

private:
    CMutex* m_Mutex;  // the mutex (NULL if not acquired)

    // Disallow assignment and copy constructor
    CMutexGuard(const CMutexGuard&);
    CMutexGuard& operator= (const CMutexGuard&);
};



/////////////////////////////////////////////////////////////////////////////
//  RW-lock
//


//
// Forward declaration of internal (platform-dependent) RW-lock representation
//

class CInternalRWLock;


//
//  CRWLock
//

class CRWLock
{
public:
    // 'ctors
    CRWLock(void);
    ~CRWLock(void);

    // Acquire the R-lock. If W-lock is already acquired by
    // another thread, then wait until it is released.
    void ReadLock(void);

    // Acquire the W-lock. If R-lock or W-lock is already acquired by
    // another thread, then wait until it is released.
    void WriteLock(void);

    // Try to acquire R-lock or W-lock, respectively. Return immediately.
    // Return TRUE if the RW-lock has been successfully acquired.
    bool TryReadLock(void);
    bool TryWriteLock(void);

    // Release the RW-lock.
    void Unlock(void);

private:
    CInternalRWLock& m_RW;  // platform-dependent RW-lock data

    // Disallow assignment and copy constructor
    CRWLock(const CRWLock&);
    CRWLock& operator= (const CRWLock&);
};



//
//  CRWLockGuard
//

class CRWLockGuard
{
public:
    // Register the RW-lock to be released by the guard destructor.
    // Do NOT acquire the RW-lock though!
    CRWLockGuard(CRWLock& rw) : m_RW(&rw) {}

    // Release the RW-lock right now (do not release it in the guard desctruc).
    void Release(void) { m_RW->Unlock(); }

    // Release the R-lock, if it was successfully acquired and
    // not released already by Release().
    ~CRWLockGuard(void)  { if (m_RW) Release(); }

    // Get the RW-lock being guarded
    CRWLock* GetRW(void) { return m_RW; }

private:
    CRWLock* m_RW;  // the RW-lock (NULL if not acquired)

    // Disallow assignment and copy constructor
    CRWLockGuard(const CRWLockGuard&);
    CRWLockGuard& operator= (const CRWLockGuard&);
};



//
//  CReadLockGuard
//

class CReadLockGuard : public CRWLockGuard
{
public:
    // Acquire the R-lock;  register it to be released by the guard destructor.
    CReadLockGuard(CRWLock& rw) : CRWLockGuard(rw) { GetRW()->ReadLock(); }

private:
    // Disallow assignment and copy constructor
    CReadLockGuard(const CReadLockGuard&);
    CReadLockGuard& operator= (const CReadLockGuard&);
};



//
//  CWriteLockGuard
//

class CWriteLockGuard : public CRWLockGuard
{
public:
    // Acquire the W-lock;  register it to be released by the guard destructor.
    CWriteLockGuard(CRWLock& rw) : CRWLockGuard(rw) { GetRW()->WriteLock(); }

private:
    // Disallow assignment and copy constructor
    CWriteLockGuard(const CWriteLockGuard&);
    CWriteLockGuard& operator= (const CWriteLockGuard&);
};


END_NCBI_SCOPE

#endif  /* NCBI_THREAD__HPP */
