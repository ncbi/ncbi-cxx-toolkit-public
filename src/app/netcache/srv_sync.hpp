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
 */


#include <corelib/guard.hpp>


BEGIN_NCBI_SCOPE


class CSrvTime;


#ifdef NCBI_COMPILER_GCC

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

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



class CFutex
{
public:
    CFutex(void);

    int GetValue(void);
    bool ChangeValue(int old_value, int new_value);
    int AddValue(int cnt_to_add);

    void SetValueNonAtomic(int new_value);

    enum EWaitResult {
        eWaitWokenUp,
        eValueChanged,
        eTimedOut
    };

    EWaitResult WaitValueChange(int old_value);
    EWaitResult WaitValueChange(int old_value, const CSrvTime& timeout);
    int WakeUpWaiters(int cnt_to_wake);

private:
    CFutex(const CFutex&);
    CFutex& operator= (const CFutex&);

    volatile int m_Value;
};


class CMiniMutex
{
public:
    CMiniMutex(void);
    ~CMiniMutex(void);

    void Lock(void);
    bool TryLock(void);
    void Unlock(void);

private:
    /// Prohibit copying of the object
    CMiniMutex(const CMiniMutex&);
    CMiniMutex& operator= (const CMiniMutex&);


    CFutex m_Futex;
};

typedef CGuard<CMiniMutex> CMiniMutexGuard;



//////////////////////////////////////////////////////////////////////////
//  Inline methods
//////////////////////////////////////////////////////////////////////////

inline
CFutex::CFutex(void)
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
