#ifndef NCBIATOMIC__H
#define NCBIATOMIC__H

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

/** @file ncbiatomic.h
 * Multi-threading -- atomic pointer exchange function
 *
 * MISC:
 * - NCBI_SwapPointers     -- atomic pointer swap operation
 */

#include <corelib/impl/ncbi_atomic_defs.h>

/** @addtogroup Threads
 *
 * @{
 */

#if defined(NCBI_SWAP_POINTERS_EXTERN)  ||  defined(NCBI_SWAP_POINTERS_IMPLEMENTATION)
#  ifdef __cplusplus
     extern "C"
#  endif
#elif defined(__cplusplus)
     inline
#elif defined(NCBI_COMPILER_WORKSHOP)
     static
#elif defined(__GNUC__)
     static __inline__
#elif __STDC_VERSION__ >= 199901
     inline
#elif defined(_MSC_VER)  ||  defined(__sgi)  ||  defined(HPUX)
     __inline
#else
     inline
#endif
void* NCBI_SwapPointers(void * volatile * location, void* new_value)
#if defined(NCBI_SWAP_POINTERS_EXTERN)  &&  !defined(NCBI_SWAP_POINTERS_IMPLEMENTATION)
         ;
#else
{
    void** nv_loc = (void**) location;
#  if defined(NCBI_HAVE_CXX11) && !defined(NCBI_COMPILER_MSVC)
    if (sizeof(std::atomic<void*>) == sizeof(void*)) {
        std::atomic<void*>& a = *(std::atomic<void*>*)nv_loc;
        return a.exchange(new_value);
    }
#  endif
#  ifdef NCBI_NO_THREADS
    void* old_value = *nv_loc;
    *nv_loc = new_value;
    return old_value;
#  elif defined(NCBI_SWAP_POINTERS)
    return NCBI_SWAP_POINTERS(nv_loc, new_value);
#  elif defined(NCBI_SWAP_POINTERS_CONDITIONALLY)
    int   swapped = 0;
    void* old_value;
    NCBI_SCHED_SPIN_INIT();
    while ( !swapped ) {
        old_value = *location;
        if (old_value == new_value) {
            /* nothing to do */
            break;
        }
        swapped = NCBI_SWAP_POINTERS_CONDITIONALLY(nv_loc, old_value,
                                                   new_value);
        NCBI_SCHED_SPIN_YIELD();
    }
    return old_value;
#  else
    /* inline assembly */
#    if defined(__i386)  ||  defined(__x86_64) /* same (overloaded) opcode... */
    void* old_value;
    asm volatile("lock; xchg %0, %1" : "=m" (*nv_loc), "=r" (old_value)
                 : "1" (new_value), "m" (*nv_loc));
    return old_value;
#    elif defined(__sparcv9)
    void* old_value;
    NCBI_SCHED_SPIN_INIT();
    for ( ;; ) {
        /* repeatedly try to swap values */
        old_value = *location;
        if ( old_value == new_value ) {
            /* nothing to do */
            break;
        }
        void* tmp = new_value;
        asm volatile("casx [%3], %2, %1" : "=m" (*nv_loc), "+r" (tmp)
                     : "r" (old_value), "r" (nv_loc), "m" (*nv_loc));
        if (tmp == old_value) {
            /* swap was successful */
            break;
        }
        NCBI_SCHED_SPIN_YIELD();
    }
    return old_value;
#    elif defined(__sparc)
    void* old_value;
    asm volatile("swap [%2], %1" : "=m" (*nv_loc), "=r" (old_value)
                 : "r" (nv_loc), "1" (new_value), "m" (*nv_loc));
    return old_value;
#    elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
    void* old_value;
    int   swapped = 0;
    NCBI_SCHED_SPIN_INIT();
    while ( !swapped ) {
        swapped = 0;
        asm volatile(
#if defined(__powerpc64__)  ||  defined(__ppc64__)
                     "ldarx %1,0,%4\n\tstdcx. %3,0,%4"
#else
                     "lwarx %1,0,%4\n\tstwcx. %3,0,%4"
#endif
                     "\n\tbne 0f\n\tli %2,1\n\t0:\n\tisync"
                     : "=m" (*nv_loc), "=&r" (old_value), "=r" (swapped)
                     : "r" (new_value), "r" (nv_loc), "m" (*nv_loc)
                     : "cc");
        NCBI_SCHED_SPIN_YIELD();
    }
    return old_value;
#    else
#      error "Unsupported processor type for assembly implementation!"
#    endif
#  endif
}
#endif

/* @} */

#endif /* NCBIATOMIC__H */
