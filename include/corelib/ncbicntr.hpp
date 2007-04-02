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

#if defined(HAVE_SCHED_YIELD) && !defined(NCBI_NO_THREADS)
extern "C" {
#  include <sched.h>
}
#  define NCBI_SCHED_INIT() int spin_counter = 0
#  define NCBI_SCHED_YIELD() if ( !(++spin_counter & 3) ) sched_yield()
#else
#  define NCBI_SCHED_INIT()
#  define NCBI_SCHED_YIELD()
#endif

#ifdef NCBI_COMPILER_GCC
#  if NCBI_COMPILER_VERSION >= 300
#    define NCBI_COMPILER_GCC3 1
#  endif
#elif defined(NCBI_COMPILER_ICC)
#  if NCBI_COMPILER_VERSION >= 600
#    define NCBI_COMPILER_ICC6
#  endif
#endif

#undef NCBI_COUNTER_UNSIGNED
#undef NCBI_COUNTER_RESERVED_VALUE
#undef NCBI_COUNTER_ASM_OK
#undef NCBI_COUNTER_USE_ASM
#undef NCBI_COUNTER_ADD
#undef NCBI_COUNTER_NEED_MUTEX

// Messy system-specific details; they're mostly grouped together here
// to ensure consistency.

#if defined(NCBI_COMPILER_GCC) || defined(NCBI_COMPILER_WORKSHOP) || (defined(NCBI_COMPILER_KCC) && defined(NCBI_OS_LINUX)) || defined(NCBI_COMPILER_ICC6)
#  define NCBI_COUNTER_ASM_OK
#endif

/// Define platform specific counter-related macros/values.
///
/// TNCBIAtomicValue "type" is defined based on facilities available for a
/// compiler/platform. TNCBIAtomicValue is used in the CAtomicCounter class
/// for defining the internal represntation of the counter.
///
/// Where possible NCBI_COUNTER_ADD is defined in terms of compiler/platform
/// specific features.
#ifdef NCBI_NO_THREADS
   typedef unsigned int TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  define NCBI_COUNTER_ADD(p, d) ((*p) += d)
#elif defined(NCBI_COUNTER_ASM_OK) && defined(__sparc) && !defined(__sparcv9)
// Always use our own code on pre-V9 SPARC; GCC 3's implementation
// uses a global lock.
   typedef unsigned int TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  define NCBI_COUNTER_RESERVED_VALUE 0x3FFFFFFF
#  define NCBI_COUNTER_USE_ASM 1
#elif defined(NCBI_COMPILER_GCC3)
#  include <bits/atomicity.h>
   typedef _Atomic_word TNCBIAtomicValue;
#  if NCBI_COMPILER_VERSION >= 340
#    define NCBI_COUNTER_ADD(p, d) (__gnu_cxx::__exchange_and_add(p, d) + d)
#  else
#    define NCBI_COUNTER_ADD(p, d) (__exchange_and_add(p, d) + d)
#  endif
#elif defined(NCBI_COMPILER_COMPAQ)
#  include <machine/builtins.h>
   typedef int TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) (__ATOMIC_ADD_LONG(p, d) + d)
#elif defined(NCBI_OS_SOLARIS) && 0 // Kernel-only. :-/
#  include <sys/atomic.h>
   typedef uint32_t TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  define NCBI_COUNTER_ADD(p, d) atomic_add_32_nv(p, d)
#elif defined(NCBI_OS_IRIX)
#  include <mutex.h>
   typedef __uint32_t TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  define NCBI_COUNTER_ADD(p, d) add_then_test32(p, d)
#elif defined(NCBI_OS_AIX)
#  include <sys/atomic_op.h>
   typedef int TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) (fetch_and_add(p, d) + d)
#elif defined(NCBI_OS_DARWIN)
#  ifdef __MWERKS__
#    define __NOEXTENSIONS__
#  endif
#  include <CoreServices/CoreServices.h>
// Darwin's <AssertMacros.h> defines check as a variant of assert....
#  ifdef check
#    undef check
#  endif
   typedef SInt32 TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) (AddAtomic(d, p) + d)
#elif defined(NCBI_OS_MAC)
#  include <OpenTransport.h> // Is this right?
   typedef SInt32 TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) OTAtomicAdd32(d, p)
#elif defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
   typedef LONG TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) (InterlockedExchangeAdd(p, d) + d)
#else
   typedef unsigned int TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  if defined (NCBI_COUNTER_ASM_OK) && (defined(__i386) || defined(__sparc) || defined(__x86_64))
#    define NCBI_COUNTER_USE_ASM 1
#  else
#    define NCBI_COUNTER_NEED_MUTEX 1
#  endif
#endif


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
    volatile TValue m_Value;  ///< Internal counter value

    // CObject's constructor needs to read m_Value directly when checking
    // for the magic number left by operator new.
    friend class CObject;
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
    NCBI_SCHED_INIT();
    while (value == NCBI_COUNTER_RESERVED_VALUE) {
        NCBI_SCHED_YIELD();
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
#  ifdef NCBI_COMPILER_WORKSHOP
#    ifdef __sparcv9
extern "C"
CAtomicCounter::TValue NCBICORE_asm_cas(CAtomicCounter::TValue new_value,
                                        CAtomicCounter::TValue* address,
                                        CAtomicCounter::TValue old_value);
#    elif defined(__sparc)
extern "C"
CAtomicCounter::TValue NCBICORE_asm_swap(CAtomicCounter::TValue new_value,
                                         CAtomicCounter::TValue* address);
#    elif defined(__x86_64)
extern "C"
CAtomicCounter::TValue NCBICORE_asm_lock_xaddl_64
(CAtomicCounter::TValue* address, int delta);
#define NCBICORE_asm_lock_xaddl NCBICORE_asm_lock_xaddl_64
#    elif defined(__i386)
extern "C"
CAtomicCounter::TValue NCBICORE_asm_lock_xaddl(CAtomicCounter::TValue* address,
                                               int delta);
#    endif
#  else
inline
#  endif
CAtomicCounter::TValue
CAtomicCounter::x_Add(volatile CAtomicCounter::TValue* value_p, int delta)
THROWS_NONE
{
    TValue result;
    TValue* nv_value_p = const_cast<TValue*>(value_p);
#  ifdef __sparcv9
    NCBI_SCHED_INIT();
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
        NCBI_SCHED_YIELD();
    }
    result += delta;
#  elif defined(__sparc)
    result = NCBI_COUNTER_RESERVED_VALUE;
    NCBI_SCHED_INIT();
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
        NCBI_SCHED_YIELD();
    }
    result += delta;
    *value_p = result;
#  elif defined(__i386) || defined(__x86_64)
    // Yay CISC. ;-)
#    ifdef NCBI_COMPILER_WORKSHOP
    result = NCBICORE_asm_lock_xaddl(nv_value_p, delta) + delta;
#    else
    asm volatile("lock; xaddl %1, %0" : "=m" (*nv_value_p), "=r" (result)
                 : "1" (delta), "m" (*nv_value_p));
    result += delta;
#    endif
#  else
#    error "Unsupported processor type for assembly implementation!"
#  endif
    return result;
}
#endif

#if !defined(NCBI_COUNTER_NEED_MUTEX)
inline
CAtomicCounter::TValue CAtomicCounter::Add(int delta) THROWS_NONE
{
    TValue* nv_value_p = const_cast<TValue*>(&m_Value);
    return NCBI_COUNTER_ADD(nv_value_p, delta);
}
#endif

END_NCBI_SCOPE

#endif  /* NCBICNTR__HPP */
