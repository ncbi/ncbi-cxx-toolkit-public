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
* File Description:
*   Efficient atomic counters (for CObject reference counts)
*   Note that the special value 0x3FFFFFFF is used to indicate
*   locked counters on some platforms.
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimtx.hpp>

#if defined(HAVE_SCHED_YIELD) && !defined(NCBI_NO_THREADS)
#  include <sched.h>
#endif

#ifdef NCBI_COMPILER_GCC
#  if NCBI_COMPILER_VERSION >= 300
#    define NCBI_COMPILER_GCC3 1
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

#if defined(NCBI_COMPILER_GCC) || defined(NCBI_COMPILER_WORKSHOP) || (defined(NCBI_COMPILER_KCC) && defined(NCBI_OS_LINUX))
#  define NCBI_COUNTER_ASM_OK
#endif

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
#  define NCBI_COUNTER_ADD(p, d) __exchange_and_add(p, d) + d
#elif defined(NCBI_COMPILER_COMPAQ)
#  include <machine/builtins.h>
   typedef int TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) __ATOMIC_ADD_LONG(p, d) + d
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
#  define NCBI_COUNTER_ADD(p, d) fetch_and_add(p, d) + d
#elif defined(NCBI_OS_DARWIN)
#  include <libkern/OSAtomic.h>
   typedef SInt32 TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) OSAddAtomic(p, d)
#elif defined(NCBI_OS_MAC)
#  include <OpenTransport.h> // Is this right?
   typedef SInt32 TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) OTAtomicAdd32(p, d)
#elif defined(NCBI_OS_MSWIN)
#  include <windows.h>
   typedef LONG TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) InterlockedExchangeAdd(p, d) + d
#else
   typedef unsigned int TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  if defined (NCBI_COUNTER_ASM_OK) && (defined(__i386) || defined(__sparc))
#    define NCBI_COUNTER_USE_ASM 1
#  else
#    define NCBI_COUNTER_NEED_MUTEX 1
#  endif
#endif

BEGIN_NCBI_SCOPE

class CAtomicCounter
{
public:
    typedef TNCBIAtomicValue TValue;

    TValue Get(void) const THROWS_NONE;
    void   Set(TValue new_value) THROWS_NONE;

    // atomically adds delta, returning new value
    TValue Add(int delta) THROWS_NONE;

private:
#ifdef NCBI_COUNTER_USE_ASM
    static TValue x_Add(volatile TValue* value, int delta) THROWS_NONE;
#endif

    volatile TValue     m_Value;
#ifdef NCBI_COUNTER_NEED_MUTEX
    static   CFastMutex sm_Mutex;
#endif
};


//////////////////////////////////////////////////////////////////////
// 
// Inline methods

inline
CAtomicCounter::TValue CAtomicCounter::Get(void) const THROWS_NONE
{
#ifdef NCBI_COUNTER_RESERVED_VALUE
    TValue value = m_Value;
    while (value == NCBI_COUNTER_RESERVED_VALUE) {
#  ifdef HAVE_SCHED_YIELD
        sched_yield(); // Be polite
#  endif
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
    TValue old_value;
    for (;;) {
        old_value = *nv_value_p;
        result = old_value + delta;
        // Atomic compare-and-swap: if *value_p == old_value, swap it
        // with result; otherwise, just put the current value in result.
#    ifdef NCBI_COMPILER_WORKSHOP
        result = NCBICORE_asm_cas(result, nv_value_p, old_value);
#    else
        asm volatile("cas [%3], %2, %1" : "+m" (*nv_value_p), "+r" (result)
                     : "r" (old_value), "r" (nv_value_p));
#    endif
        if (result == old_value) { // We win
            break;
        }
#    ifdef HAVE_SCHED_YIELD
        sched_yield();
#    endif
    }
    result += delta;
#  elif defined(__sparc)
    result = NCBI_COUNTER_RESERVED_VALUE;
    for (;;) {
#    ifdef NCBI_COMPILER_WORKSHOP
        result = NCBICORE_asm_swap(result, nv_value_p);
#    else
        asm volatile("swap [%2], %1" : "+m" (*nv_value_p), "+r" (result)
                     : "r" (nv_value_p));
#    endif
        if (result != NCBI_COUNTER_RESERVED_VALUE) {
            break;
        }
#    ifdef HAVE_SCHED_YIELD
        sched_yield();
#    endif
    }
    result += delta;
    *nv_value_p = result;
#  elif defined(__i386)
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


inline
CAtomicCounter::TValue CAtomicCounter::Add(int delta) THROWS_NONE
{
#ifdef NCBI_COUNTER_ADD
    TValue* nv_value_p = const_cast<TValue*>(&m_Value);
    return NCBI_COUNTER_ADD(nv_value_p, delta);
#elif defined(NCBI_COUNTER_USE_ASM)
    return x_Add(&m_Value, delta);
#else // Use the global lock; relatively inefficient, but at least safe.
    TValue* nv_value_p = const_cast<TValue*>(&m_Value);
    CFastMutexGuard LOCK(sm_Mutex);
    return (*nv_value_p) += delta;
#endif
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2002/05/24 18:43:34  ucko
* Use trivial implementation for single-threaded builds.
*
* Revision 1.3  2002/05/24 18:06:55  ucko
* Inline assembly leads to ICC compiler errors in some cases, so stop
* defining NCBI_COUNTER_ASM_OK for ICC.
*
* Revision 1.2  2002/05/24 14:20:04  ucko
* Fix Intel extended assembly.
* Drop use of FreeBSD <machine/atomic.h>; those functions return void.
* Fix name of "nv_value_p" in Add(), and declare it only when needed.
*
* Revision 1.1  2002/05/23 22:24:21  ucko
* Use low-level atomic operations for reference counts
*
*
* ===========================================================================
*/

#endif  /* NCBICNTR__HPP */
