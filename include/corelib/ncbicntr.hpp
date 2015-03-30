#ifndef NCBICNTR__HPP
#define NCBICNTR__HPP

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
* Author:  Aaron Ucko
*
*
*/

/// @file ncbictr.hpp
/// Efficient atomic counters (for CObject reference counts)
/// Note that the special value 0x3FFFFFFF is used to indicate
/// locked counters on some platforms.


#include <corelib/ncbistd.hpp>

#include <corelib/impl/ncbi_atomic_defs.h>

/** @addtogroup Counters
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CAtomicCounter --
///
/// Define a basic atomic counter.
///
/// Provide basic counter operations for an atomic counter represented
/// internally by TNCBIAtomicValue.
/// @note
///   CAtomicCounter has no constructor and is initialized only when
///   created as static object. In all other cases Set(0) must be called
///   to initialize the counter. CAtomicCounter_WithAutoInit can be used
///   instead of CAtomicCounter if the initialization is required.
/// @note
///   TNCBIAtomicValue does not imply any assumptions about the size and
///   the signedness of the value. It is at least as big as int datatype
///   and can be signed on some platforms and unsigned on others.

class CAtomicCounter
{
public:
    typedef TNCBIAtomicValue TValue;  ///< Alias TValue for TNCBIAtomicValue

    /// Get atomic counter value.
    TValue Get(void) const THROWS_NONE;

    /// Set atomic counter value.
    void   Set(TValue new_value) THROWS_NONE;

    /// Atomically add value (=delta), and return new counter value.
    TValue Add(int delta) THROWS_NONE;
    
    /// Define NCBI_COUNTER_ADD if one has not been defined.
#if defined(NCBI_COUNTER_USE_ASM)
    static TValue x_Add(volatile TValue* value, int delta) THROWS_NONE;
#  if !defined(NCBI_COUNTER_ADD)
#     define NCBI_COUNTER_ADD(value, delta) NCBI_NS_NCBI::CAtomicCounter::x_Add((value), (delta))
#  endif
#endif

private:
    typedef NCBI_ATOMIC_TYPE(TValue) TData;
    volatile TData m_Value;  ///< Internal counter value

    // CObject's constructor needs to read m_Value directly when checking
    // for the magic number left by operator new.
    friend class CObject;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CAtomicCounter_WithAutoInit --
///
/// Define an atomic counter with guaranteed initialization.
///
/// CAtomicCounter does not initialize its value if it's not
/// a static object. CAtomicCounter_WithAutoInit automatically
/// calls Set() in its constructor to set the initial value.

class CAtomicCounter_WithAutoInit : public CAtomicCounter
{
public:
    CAtomicCounter_WithAutoInit(TValue initial_value = 0)
    {
        Set(initial_value);
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CMutableAtomicCounter --
///
/// Define a mutable atomic counter.
///
/// Provide mutable counter operations for an atomic counter represented
/// internally by CAtomicCounter. 

class NCBI_XNCBI_EXPORT CMutableAtomicCounter
{
public:
    typedef CAtomicCounter::TValue TValue; ///< Alias TValue simplifies syntax

    /// Get atomic counter value.
    TValue Get(void) const THROWS_NONE
        { return m_Counter.Get(); }

    /// Set atomic counter value.
    void   Set(TValue new_value) const THROWS_NONE
        { m_Counter.Set(new_value); }

    /// Atomically add value (=delta), and return new counter value.
    TValue Add(int delta) const THROWS_NONE
        { return m_Counter.Add(delta); }

private:
    mutable CAtomicCounter m_Counter;      ///< Mutable atomic counter value
};


/* @} */


//////////////////////////////////////////////////////////////////////
// 
// Inline methods

inline
CAtomicCounter::TValue CAtomicCounter::Get(void) const THROWS_NONE
{
#ifdef NCBI_COUNTER_RESERVED_VALUE
    TValue value = m_Value;
    NCBI_SCHED_SPIN_INIT();
    while (value == NCBI_COUNTER_RESERVED_VALUE) {
        NCBI_SCHED_SPIN_YIELD();
        value = m_Value;
    }
    return value;
#else
    return m_Value;
#endif
}


inline
void CAtomicCounter::Set(CAtomicCounter::TValue new_value) THROWS_NONE
{
    m_Value = new_value;
}


// With WorkShop, sanely inlining assembly requires the use of ".il" files.
// In order to keep the toolkit's external interface sane, we therefore
// force this method out-of-line and into ncbicntr_workshop.o.
#if defined(NCBI_COUNTER_USE_ASM) && (!defined(NCBI_COMPILER_WORKSHOP) || defined(NCBI_COUNTER_IMPLEMENTATION))
#  ifndef NCBI_COMPILER_WORKSHOP
inline
#  endif
CAtomicCounter::TValue
CAtomicCounter::x_Add(volatile CAtomicCounter::TValue* value_p, int delta)
THROWS_NONE
{
    TValue result;
    TValue* nv_value_p = const_cast<TValue*>(value_p);
#  ifdef __sparcv9
    NCBI_SCHED_SPIN_INIT();
    for (;;) {
        TValue old_value = *value_p;
        result = old_value + delta;
        // Atomic compare-and-swap: if *value_p == old_value, swap it
        // with result; otherwise, just put the current value in result.
#    ifdef NCBI_COMPILER_WORKSHOP
        result = NCBICORE_asm_cas(result, nv_value_p, old_value);
#    else
        asm volatile("cas [%3], %2, %1" : "=m" (*nv_value_p), "+r" (result)
                     : "r" (old_value), "r" (nv_value_p), "m" (*nv_value_p));
#    endif
        if (result == old_value) { // We win
            break;
        }
        NCBI_SCHED_SPIN_YIELD();
    }
    result += delta;
#  elif defined(__sparc)
    result = NCBI_COUNTER_RESERVED_VALUE;
    NCBI_SCHED_SPIN_INIT();
    for (;;) {
#    ifdef NCBI_COMPILER_WORKSHOP
        result = NCBICORE_asm_swap(result, nv_value_p);
#    else
        asm volatile("swap [%2], %1" : "=m" (*nv_value_p), "+r" (result)
                     : "r" (nv_value_p), "m" (*nv_value_p));
#    endif
        if (result != NCBI_COUNTER_RESERVED_VALUE) {
            break;
        }
        NCBI_SCHED_SPIN_YIELD();
    }
    result += delta;
    *value_p = result;
#  elif defined(__i386) || defined(__x86_64)
    // Yay CISC. ;-)  (WorkShop already handled.)
#   ifdef NCBI_COUNTER_64_BIT
    asm volatile("lock; xaddq %1, %0" : "=m" (*nv_value_p), "=r" (result)
                 : "1" (Int8(delta)), "m" (*nv_value_p));
#   else
    asm volatile("lock; xaddl %1, %0" : "=m" (*nv_value_p), "=r" (result)
                 : "1" (delta), "m" (*nv_value_p));
#endif
    result += delta;
#  else
#    error "Unsupported processor type for assembly implementation!"
#  endif
    return result;
}
#endif

#if !defined(NCBI_COUNTER_NEED_MUTEX)  &&  (!defined(NCBI_COUNTER_USE_EXTERN_ASM)  ||  defined(NCBI_COUNTER_IMPLEMENTATION))
#  ifndef NCBI_COUNTER_USE_EXTERN_ASM
inline
#  endif
CAtomicCounter::TValue CAtomicCounter::Add(int delta) THROWS_NONE
{
    TData* nv_value_p = const_cast<TData*>(&m_Value);
    return NCBI_COUNTER_ADD(nv_value_p, delta);
}
#endif

END_NCBI_SCOPE

#endif  /* NCBICNTR__HPP */
