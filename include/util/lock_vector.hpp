#ifndef UTIL_LOCK_VECTOR__HPP
#define UTIL_LOCK_VECTOR__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Lock vector
 *                   
 */

#include <corelib/ncbimisc.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_system.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/error_codes.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup Threads
 *
 * @{
 */


/// Class implements bit-vector based lock storage registering millions 
/// of int numbered objects.
///
/// @sa CLockVectorGuard
///
template <class BV>
class CLockVector
{
public:
    typedef BV             TBitVector;
public:
    CLockVector();
    ~CLockVector();

    /// Try to acquire lock of specified id
    ///
    /// @return TRUE if lock was issued, 
    ///         FALSE if object has already being locked.
    ///
    bool TryLock(unsigned id);

    /// Unlock object.
    /// @return TRUE if object unlocked successfully, 
    ///         FALSE if it was not locked
    ///
    bool Unlock(unsigned id);

    /// Check if id is locked or not
    bool IsLocked(unsigned id) const;


    /// Reclaim unused memory 
    void FreeUnusedMem();
    
private:
    CLockVector(const CLockVector<BV>&);
    CLockVector& operator=(const CLockVector<BV>&);
protected:
    TBitVector          m_IdVector;      ///< vector of locked objs
    mutable CFastMutex  m_IdVector_Lock; ///< lock for m_LockVector
};

/// Lock guard for the CLockVector
/// 
/// @sa CLockVector
///
template<class TLockVect>
class CLockVectorGuard
{
public:
    typedef TLockVect  TLockVector;
public:

    /// Construct without locking
    CLockVectorGuard(TLockVector& lvect, 
                     unsigned     timeout_ms);

    /// Construct and lock
    /// 
    /// @param lvect
    ///     Lock vector storing all locks
    /// @param id
    ///     Object Id we are locking
    /// @param timeout_ms
    ///     Timeout in milliseconds for how long lock makes attempts to
    ///     acquire the id. If cannot lock it throws an exception.
    ///
    CLockVectorGuard(TLockVector& lvect, 
                     unsigned     id,
                     unsigned     timeout_ms);


    ~CLockVectorGuard();

    /// Acquire lock 
    void Lock(unsigned id);

    /// Unlocks the lock
    void Unlock();

    /// Forger lock
    void Release() { m_LockSet = false; }

    /// Get BLOB id
    unsigned GetId() const { return m_Id; }

    /// Assign Id (no locking)
    void SetId(unsigned id) 
    { 
        _ASSERT(m_Id == 0);
        _ASSERT(id);

        m_Id = id; 
    }

    /// Transfer lock ownership from another lock
    /// @param lg
    ///     Old lock guard (lock src)
    ///
    void TakeFrom(CLockVectorGuard& lg);

    /// Lock acquisition
    void DoLock();

    TLockVector& GetLockVector() const {return *m_LockVector; }
    unsigned GetTimeout() const { return m_Timeout; }

private:
    CLockVectorGuard(const CLockVectorGuard<TLockVect>&);
    CLockVectorGuard<TLockVect>& operator=(const CLockVectorGuard<TLockVect>&);
private:
    TLockVector*   m_LockVector;
    unsigned       m_Id;
    unsigned       m_Timeout;
    unsigned       m_Spins;
    bool           m_LockSet;
};

template<class TLockVect> 
CLockVectorGuard<TLockVect>::CLockVectorGuard(TLockVector& lvect, 
                                              unsigned     timeout_ms)
: m_LockVector(&lvect),
  m_Id(0),
  m_Timeout(timeout_ms),
  m_Spins(200),
  m_LockSet(false)
{
}


template<class TLockVect> 
CLockVectorGuard<TLockVect>::CLockVectorGuard(TLockVector& lvect, 
                                              unsigned     id,
                                              unsigned     timeout_ms)
: m_LockVector(&lvect),
  m_Id(id),
  m_Timeout(timeout_ms),
  m_Spins(200),
  m_LockSet(false)
{
    _ASSERT(id);
    DoLock();
}

template<class TLockVect> 
CLockVectorGuard<TLockVect>::~CLockVectorGuard()
{
    try {
        Unlock();
    } 
    catch (exception& ex)
    {
        ERR_POST_XX(Util_LVector, 1, ex.what());
    }
}

template<class TLockVect> 
void CLockVectorGuard<TLockVect>::DoLock()
{
    _ASSERT(m_LockSet == false);

    // Strategy implemented here is spin-and-lock
    // works fine if rate of contention is relatively low
    // in the future needs to be changed so lock vector returns semaphor to
    // wait until requested id is free
    //
    for (unsigned i = 0; i < m_Spins; ++i) {
        m_LockSet = m_LockVector->TryLock(m_Id);
        if (m_LockSet) {
            return;
        }
    } // for

    // plain spin lock did not work -- try to lock it gracefully
    //
    unsigned sleep_ms = 10;
    unsigned time_spent = 0;
    while (true) {
        m_LockSet = m_LockVector->TryLock(m_Id);
        if (m_LockSet) {
            return;
        }
        SleepMilliSec(sleep_ms);
        if (m_Timeout) {
            time_spent += sleep_ms;
            if (time_spent > m_Timeout) {
                string msg = "Lock vector timeout error on object id="
                    + NStr::UIntToString(m_Id);
                NCBI_THROW(CMutexException, eTryLock, msg);
            }
        }
    } // while
}


template<class TLockVect> 
void CLockVectorGuard<TLockVect>::Lock(unsigned id)
{
    Unlock();
    m_Id = id;
    DoLock();
}

template<class TLockVect> 
void CLockVectorGuard<TLockVect>::Unlock()
{
    if (!m_LockSet) {
        return;
    }
    bool unlocked = m_LockVector->Unlock(m_Id);
    _ASSERT(unlocked);
    if (!unlocked) {
        string msg = 
            "Double unlock on object id=" + NStr::UIntToString(m_Id);
        NCBI_THROW(CMutexException, eTryLock, msg);
    }
    m_LockSet = false;
}

template<class BV> 
void CLockVectorGuard<BV>::TakeFrom(CLockVectorGuard& lg)
{
    Unlock();
    m_LockVector = lg.m_LockVector; m_Id = lg.m_Id;
    m_LockSet = true;
    lg.Release();
}


template<class BV> 
CLockVector<BV>::CLockVector() 
: m_IdVector(bm::BM_GAP)
{
}

template<class BV> 
CLockVector<BV>::~CLockVector() 
{
    if (m_IdVector.any()) {
        ERR_POST_XX(Util_LVector, 2,
                    "::~CLockVector() detected live locks on destruction.");
    }
}

template<class BV> 
bool CLockVector<BV>::TryLock(unsigned id)
{
    CFastMutexGuard guard(m_IdVector_Lock);
    bool is_set = m_IdVector.set_bit_conditional(id, true, false);
    return is_set;
}

template<class BV> 
bool CLockVector<BV>::Unlock(unsigned id)
{
    CFastMutexGuard guard(m_IdVector_Lock);
    bool is_set = m_IdVector.set_bit_conditional(id, false, true);
    if (!is_set) {
        _ASSERT(m_IdVector[id] == false);
    }
    return is_set;
}

template<class BV> 
void CLockVector<BV>::FreeUnusedMem()
{
    CFastMutexGuard guard(m_IdVector_Lock);
    m_IdVector.optimize(0, TBitVector::opt_free_0);
}

template<class BV> 
bool CLockVector<BV>::IsLocked(unsigned id) const
{
    CFastMutexGuard guard(m_IdVector_Lock);
    return m_IdVector[id];
}


/* @} */

END_NCBI_SCOPE

#endif
