#ifndef NETCACHE__SRV_SYNC__HPP
#define NETCACHE__SRV_SYNC__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 *   Header defines several primitives to use for inter-thread synchronization.
 */


#include <corelib/guard.hpp>


BEGIN_NCBI_SCOPE


class CSrvTime;


#ifdef NCBI_COMPILER_GCC

/// Purpose of this macro is to force compiler to access variable exactly at
/// the place it's written (no moving around and changing places with other
/// variables reads or writes) and to avoid optimizations when several usages
/// of local variable are replaced with direct access to global variable which
/// should be read/written only once.
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

template <class T>
inline T
AtomicValCAS(T volatile& var, T old_value, T new_value)
{
    return __sync_val_compare_and_swap(&var, old_value, new_value);
}

template <class T>
inline bool
AtomicCAS(T volatile& var, T old_value, T new_value)
{
    return __sync_bool_compare_and_swap(&var, old_value, new_value);
}

template <class T>
inline T
AtomicAdd(T volatile& var, T add_value)
{
    return __sync_add_and_fetch(&var, add_value);
}

template <class T>
inline T
AtomicSub(T volatile& var, T sub_value)
{
    return __sync_sub_and_fetch(&var, sub_value);
}

#else

#define ACCESS_ONCE(x) x

template <class T>
inline T
AtomicValCAS(T volatile& var, T old_value, T new_value)
{
    return old_value;
}

template <class T>
inline bool
AtomicCAS(T volatile& var, T old_value, T new_value)
{
    return false;
}

template <class T>
inline T
AtomicAdd(T volatile& var, T add_value)
{
    return var;
}

template <class T>
inline T
AtomicSub(T volatile& var, T sub_value)
{
    return var;
}

#endif


template <class T1, class T2, class T3>
inline bool
AtomicCAS(T1 volatile& var, T2 old_value, T3 new_value)
{
    return AtomicCAS(var, (T1)old_value, (T1)new_value);
}

template <class T1, class T2>
inline T1
AtomicAdd(T1 volatile& var, T2 add_value)
{
    return AtomicAdd(var, (T1)add_value);
}

template <class T1, class T2>
inline T1
AtomicSub(T1 volatile& var, T2 sub_value)
{
    return AtomicSub(var, (T1)sub_value);
}



/// Wrapper around Linux's futex.
class CFutex
{
public:
    CFutex(void);

    /// Read value of the futex.
    int GetValue(void);
    /// Atomically change value of the futex. If current futex value was
    /// changed in another thread and doesn't match old_value then method
    /// returns FALSE and value is not changed. Otherwise value is changed to
    /// new_value and method returns TRUE.
    bool ChangeValue(int old_value, int new_value);
    /// Atomically add some amount to futex's value. Result of addition is
    /// returned.
    int AddValue(int cnt_to_add);

    /// Set futex's value non-atomically, i.e. caller should ensure that
    /// several threads don't race with each other with setting different
    /// values.
    void SetValueNonAtomic(int new_value);

    /// Type of result returned from WaitValueChange()
    enum EWaitResult {
        /// Thread was woken up by call to WakeUpWaiters() from another thread.
        eWaitWokenUp,
        /// Futex's value was changed in another thread before waiting was
        /// started.
        eValueChanged,
        /// Method returned because total waiting time exceeded given timeout.
        eTimedOut
    };

    /// Wait for futex's value to change (with and without timeout).
    /// Thread won't wake up automatically when value has changed -- thread
    /// changing it should call WakeUpWaiters().
    EWaitResult WaitValueChange(int old_value);
    EWaitResult WaitValueChange(int old_value, const CSrvTime& timeout);
    /// Wake up some threads waiting on this futex. cnt_to_wake is the maximum
    /// number of threads to wake.
    int WakeUpWaiters(int cnt_to_wake);

private:
    CFutex(const CFutex&);
    CFutex& operator= (const CFutex&);

    /// Value of the futex.
    volatile int m_Value;
};


/// Mutex created to have minimum possible size (its size is 4 bytes) and
/// to sleep using kernel capabilities (without busy-loops).
class CMiniMutex
{
public:
    CMiniMutex(void);
    ~CMiniMutex(void);

    /// Lock the mutex. Mutex doesn't support recursive locking. So if current
    /// thread has already locked this mutex second call will result in
    /// deadlock.
    void Lock(void);
    /// Quickly try to lock mutex (without any retries). Returns TRUE if lock
    /// was successful and Unlock() should be called, FALSE otherwise.
    bool TryLock(void);
    /// Unlock the mutex. Mutex doesn't check whether this thread actually
    /// locked it. If you call Unlock() without calling Lock() behavior is
    /// undefined.
    void Unlock(void);

private:
    /// Prohibit copying of the object
    CMiniMutex(const CMiniMutex&);
    CMiniMutex& operator= (const CMiniMutex&);

    /// Futex which is used as base for mutex implementation.
    CFutex m_Futex;
};

typedef CGuard<CMiniMutex> CMiniMutexGuard;
#if NCBI_DEVELOPMENT_VER > 20141106
inline
void CGuard_Base::ReportException(std::exception&) {
}
#endif



//////////////////////////////////////////////////////////////////////////
//  Inline methods
//////////////////////////////////////////////////////////////////////////

inline
CFutex::CFutex(void)
    : m_Value(0)
{}

inline int
CFutex::GetValue(void)
{
    return m_Value;
}

inline bool
CFutex::ChangeValue(int old_value, int new_value)
{
    return AtomicCAS(m_Value, old_value, new_value);
}

inline int
CFutex::AddValue(int cnt_to_add)
{
    return AtomicAdd(m_Value, cnt_to_add);
}

inline void
CFutex::SetValueNonAtomic(int new_value)
{
    m_Value = new_value;
}


inline
CMiniMutex::CMiniMutex(void)
{
    m_Futex.SetValueNonAtomic(0);
}

inline
CMiniMutex::~CMiniMutex(void)
{
    _ASSERT(m_Futex.GetValue() == 0);
}

inline bool
CMiniMutex::TryLock(void)
{
    return m_Futex.GetValue() == 0  &&  m_Futex.ChangeValue(0, 1);
}

END_NCBI_SCOPE

#endif /* NETCACHE__SRV_SYNC__HPP */
