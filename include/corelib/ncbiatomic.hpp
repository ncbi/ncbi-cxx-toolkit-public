#ifndef NCBIATOMIC__HPP
#define NCBIATOMIC__HPP

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

/// @file ncbiatomic.hpp
/// Multi-threading -- atomic pointer exchange function
///
/// MISC:
/// - SwapPointers     -- atomic pointer swap operation
///

#include <corelib/ncbicntr.hpp>

#if defined(NCBI_OS_DARWIN)  &&  !defined(NCBI_NO_THREADS)
// Needed for SwapPointers, even if not for CAtomicCounter
#  include <CoreServices/CoreServices.h>
// Darwin's <AssertMacros.h> defines check as a variant of assert....
#  ifdef check
#    undef check
#  endif
#endif

BEGIN_NCBI_SCOPE

/** @addtogroup Threads
 *
 * @{
 */

/// Out-of-line implementation; defined and used ifdef NCBI_SLOW_ATOMIC_SWAP.
void* x_SwapPointers(void * volatile * location, void* new_value);




/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



// Sigh... this is a big mess because WorkShop's handling of inline
// asm is awkward and a lot of platforms supply compare-and-swap but
// no suitable unconditional swap.  It also doesn't help that standard
// interfaces are typically for integral types.
#if defined(NCBI_COMPILER_WORKSHOP)  &&  !defined(NCBI_NO_THREADS)  &&  !defined(NCBI_COUNTER_IMPLEMENTATION)
/// Set *location to new_value, and return its immediately previous contents.
void* SwapPointers(void * volatile * location, void* new_value);
#else
#  if defined(NCBI_COMPILER_WORKSHOP)  &&  !defined(NCBI_NO_THREADS)
#    ifdef __sparcv9
extern "C"
void* NCBICORE_asm_casx(void* new_value, void** location, void* old_value);
#    elif defined(__i386)
extern "C"
void* NCBICORE_asm_xchg(void* new_value, void** location);
#    endif
#  else
inline
#  endif
void* SwapPointers(void * volatile * location, void* new_value)
{
    void** nv_loc = const_cast<void**>(location);
#  ifdef NCBI_NO_THREADS
    void* old_value = *nv_loc;
    *nv_loc = new_value;
    return old_value;
#  elif defined(NCBI_COMPILER_COMPAQ)
    return reinterpret_cast<void*>
        (__ATOMIC_EXCH_QUAD(nv_loc, reinterpret_cast<long>(new_value)));
#  elif defined(NCBI_OS_IRIX)
    return reinterpret_cast<void*>
        (test_and_set(reinterpret_cast<unsigned long*>(nv_loc),
                      reinterpret_cast<unsigned long>(new_value)));
#  elif defined(NCBI_OS_AIX)
    boolean_t swapped   = FALSE;
    void*     old_value;
    NCBI_SCHED_INIT();
    while (swapped == FALSE) {
        old_value = *location;
        swapped = compare_and_swap(reinterpret_cast<atomic_p>(nv_loc),
                                   reinterpret_cast<int*>(&old_value),
                                   reinterpret_cast<int>(new_value));
        NCBI_SCHED_YIELD();
    }
    return old_value;
#  elif defined(NCBI_OS_DARWIN)
    bool  swapped = false;
    void* old_value;
    NCBI_SCHED_INIT();
    while (swapped == false) {
        old_value = *location;
        swapped = CompareAndSwap(reinterpret_cast<UInt32>(old_value),
                                 reinterpret_cast<UInt32>(new_value),
                                 reinterpret_cast<UInt32*>(nv_loc));
        NCBI_SCHED_YIELD();
    }
    return old_value;
#  elif defined(NCBI_OS_MAC)
    Boolean swapped = FALSE;
    void*   old_value;
    NCBI_SCHED_INIT();
    while (swapped == FALSE) {
        old_value = *location;
        swapped = OTCompareAndSwapPtr(*location, new_value, nv_loc);
        NCBI_SCHED_YIELD();
    }
    return old_value;
#  elif defined(NCBI_OS_MSWIN)
    // InterlockedExchangePointer would be better, but older SDK versions
    // don't declare it. :-/
    return reinterpret_cast<void*>
        (InterlockedExchange(reinterpret_cast<LPLONG>(nv_loc),
                             reinterpret_cast<LONG>(new_value)));
#  elif defined(NCBI_COUNTER_ASM_OK)
#    if defined(__i386) || defined(__x86_64) // same (overloaded) opcode...
    void* old_value;
#      ifdef NCBI_COMPILER_WORKSHOP
    old_value = NCBICORE_asm_xchg(new_value, nv_loc);
#      else
    asm volatile("xchg %0, %1" : "=m" (*nv_loc), "=r" (old_value)
                 : "1" (new_value), "m" (*nv_loc));
#      endif
    return old_value;
#    elif defined(__sparcv9)
    void* old_value;
    NCBI_SCHED_INIT();
    for ( ;; ) {
        // repeatedly try to swap values
        old_value = *location;
        if ( old_value == new_value ) {
            // The new & old values are the same - we can assume them swapped.
            // Anyway, the following logic will not work in this case.
            break;
        }
        void* tmp = new_value;
#      ifdef NCBI_COMPILER_WORKSHOP
        tmp = NCBICORE_asm_casx(tmp, nv_loc, old_value);
#      else
        asm volatile("casx [%3], %2, %1" : "=m" (*nv_loc), "+r" (tmp)
                     : "r" (old_value), "r" (nv_loc), m (*nv_loc));
#      endif
        if (tmp == old_value) {
            // swap was successful
            break;
        }
        NCBI_SCHED_YIELD();
    }
    return old_value;
#    elif defined(__sparc)
    void* old_value;
#      ifdef NCBI_COMPILER_WORKSHOP
    old_value = reinterpret_cast<void*>
        (NCBICORE_asm_swap(reinterpret_cast<TNCBIAtomicValue>(new_value),
                           reinterpret_cast<TNCBIAtomicValue*>(nv_loc)));
#      else
    asm volatile("swap [%2], %1" : "=m" (*nv_loc), "=r" (old_value)
                 : "r" (nv_loc), "1" (new_value), "m" (*nv_loc));
#      endif
    return old_value;
#    else
#      define NCBI_SLOW_ATOMIC_SWAP
    return x_SwapPointers(location, new_value);
#    endif
#  else
#    define NCBI_SLOW_ATOMIC_SWAP
    return x_SwapPointers(location, new_value);
#  endif
}
#endif

/* @} */


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2004/04/26 14:31:11  ucko
 * Split up "+m" constraints for GCC assembly, as versions 3.4 and up
 * complain about them.
 *
 * Revision 1.9  2004/02/19 16:50:57  vasilche
 * Use spin counter before sched_yield().
 * Check if new_value == old_value before cacx - logic doesn't work in this case.
 *
 * Revision 1.8  2004/02/18 23:27:56  ucko
 * Fix logic errors, and call sched_yield() between tries rather than spinning.
 *
 * Revision 1.7  2004/02/04 00:38:02  ucko
 * Centralize undefinition of Darwin's check macro.
 *
 * Revision 1.6  2004/02/03 19:28:18  ucko
 * Darwin: include the master CoreServices header because
 * DriverSynchronization.h is officially internal, but limit its use to
 * threaded builds.
 *
 * Revision 1.5  2004/01/21 22:07:37  ucko
 * SwapPointers: use XCHG opcode on x86-64 in addition to vanilla x86.
 *
 * Revision 1.4  2003/10/09 21:02:48  ucko
 * Only predeclare SwapPointers when not inlining it, to avoid a warning.
 *
 * Revision 1.3  2003/10/02 12:35:08  ucko
 * Always define SwapPointers (as inline) for ST WorkShop builds.
 *
 * Revision 1.2  2003/09/23 19:46:41  rsmith
 * Get rid of old Boolean macros and use built in bool on Mac OSX
 *
 * Revision 1.1  2003/09/17 15:20:45  vasilche
 * Moved atomic counter swap functions to separate file.
 * Added CRef<>::AtomicResetFrom(), CRef<>::AtomicReleaseTo() methods.
 *
 * Revision 1.20  2003/07/31 19:29:03  ucko
 * SwapPointers: fix for Mac OS (classic and X) and AIX.
 *
 * Revision 1.19  2003/06/27 17:27:44  ucko
 * +SwapPointers
 * ===========================================================================
 */

#endif//NCBIATOMIC__HPP
