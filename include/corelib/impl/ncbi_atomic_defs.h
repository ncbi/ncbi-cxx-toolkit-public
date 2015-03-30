#ifndef CORELIB___NCBI_ATOMIC_DEFS__HPP
#define CORELIB___NCBI_ATOMIC_DEFS__HPP

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
*/

/** @file ncbi_atomic_defs.h
 *  (Highly!) platform-specific configuration for low-level atomic
 *  operations (reference-count manipulation, pointer swapping).
 */

#include <ncbiconf.h>

/** @addtogroup Threads
 *
 * @{
 */

#if (defined(HAVE_SCHED_YIELD) || defined(NCBI_OS_MSWIN)) \
    &&  !defined(NCBI_NO_THREADS)
#  if defined(__cplusplus)  &&  defined(NCBI_OS_DARWIN)
/* Mac OS X 10.2 and older failed to protect sched.h properly. :-/ */
extern "C" {
#    include <sched.h>
}
#  elif !defined(NCBI_OS_MSWIN)
#    include <sched.h>
#  endif
#  if defined(NCBI_OS_MSWIN)
#    define NCBI_SCHED_YIELD()      ::SwitchToThread()
#    define NCBI_SCHED_SPIN_INIT()  int spin_counter = 0
#    define NCBI_SCHED_SPIN_YIELD() if ( !(++spin_counter & 3) ) ::SwitchToThread()
#  else
#    define NCBI_SCHED_YIELD()      sched_yield()
#    define NCBI_SCHED_SPIN_INIT()  int spin_counter = 0
#    define NCBI_SCHED_SPIN_YIELD() if ( !(++spin_counter & 3) ) sched_yield()
#  endif
#else
#  define NCBI_SCHED_YIELD()
#  define NCBI_SCHED_SPIN_INIT()
#  define NCBI_SCHED_SPIN_YIELD()
#endif

#undef NCBI_COUNTER_UNSIGNED
#undef NCBI_COUNTER_RESERVED_VALUE
#undef NCBI_COUNTER_ASM_OK
#undef NCBI_COUNTER_USE_ASM
#undef NCBI_COUNTER_ADD
#undef NCBI_COUNTER_NEED_MUTEX
#undef NCBI_SWAP_POINTERS
#undef NCBI_SWAP_POINTERS_CONDITIONALLY
#undef NCBI_SWAP_POINTERS_EXTERN
#undef NCBI_SLOW_ATOMIC_SWAP

#if defined(NCBI_COMPILER_GCC) || defined(NCBI_COMPILER_WORKSHOP) || (defined(NCBI_COMPILER_KCC) && defined(NCBI_OS_LINUX)) || defined(NCBI_COMPILER_ICC)
#  define NCBI_COUNTER_ASM_OK 1
#endif

#ifdef __cplusplus
#  include <cstdlib> /* determine C++ standard library version */
#endif

#if defined(NCBI_TCHECK)
#  define NCBI_SWAP_POINTERS_EXTERN 1
#endif

#if defined(NCBI_HAVE_CXX11)
#  include <atomic>
#endif

/**
 * Define platform specific atomic-operations macros/values.
 *
 * TNCBIAtomicValue "type" is defined based on facilities available for a
 * compiler/platform. TNCBIAtomicValue is used in the CAtomicCounter class
 * for defining the internal represntation of the counter.
 *
 * Where possible NCBI_COUNTER_ADD is defined in terms of compiler/platform
 * specific features, favoring inline assembly over standard library calls
 * as function-call overhead can be appreciable, particularly on x86.
 */
#ifdef NCBI_NO_THREADS
   typedef unsigned int TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  define NCBI_COUNTER_ADD(p, d) ((*p) += d)
#elif (defined(NCBI_COMPILER_GCC) || defined(NCBI_COMPILER_ICC))  &&  \
  ((defined(__sparc) && !defined(__sparcv9))  ||  \
   ((defined(__i386) || defined(__sparc) || defined(__x86_64))  &&  \
    !defined(__GLIBCPP__) /* < 3.0 or >= 3.4 */)  &&  \
   (!defined(__GLIBCXX__)  ||  !defined(NCBI_TCHECK)))
#  if defined(__x86_64)
#    define NCBI_COUNTER_64_BIT
#  endif
#  ifdef NCBI_COUNTER_64_BIT
   typedef size_t TNCBIAtomicValue;
#  else
   typedef unsigned int TNCBIAtomicValue;
#  endif
#  define NCBI_COUNTER_UNSIGNED 1
#  define NCBI_COUNTER_USE_ASM 1
#  if defined(__sparc)  &&  !defined(__sparcv9)
#    define NCBI_COUNTER_RESERVED_VALUE 0x3FFFFFFF
#  endif
#elif defined(NCBI_HAVE_CXX11)
    typedef unsigned int TNCBIAtomicValue;
#  define NCBI_ATOMIC_TYPE(t) std::atomic<t>
#  define NCBI_COUNTER_UNSIGNED 1
#  define NCBI_COUNTER_ADD(p, d) ((*p) += d)
#elif defined(NCBI_OS_SOLARIS)  &&  defined(HAVE_ATOMIC_H) /* Solaris 10+. */
#  include <atomic.h>
#  ifndef NCBI_COUNTER_ADD
     typedef uint32_t TNCBIAtomicValue;
#    define NCBI_COUNTER_UNSIGNED 1
#    define NCBI_COUNTER_ADD(p, d) atomic_add_32_nv(p, d)
#  endif
   /* Some systems have old, incomplete(!) versions of (sys/)atomic.h. :-/ */
#  ifndef _SYS_ATOMIC_H
     extern
#    ifdef __cplusplus
       "C"
#    endif
     void *atomic_swap_ptr(volatile void *, void *);
#  endif
#  define NCBI_SWAP_POINTERS(loc, nv) atomic_swap_ptr(loc, nv)
#elif defined(NCBI_COMPILER_WORKSHOP)
#  ifdef __cplusplus
extern "C" {
#  endif
#  ifdef __sparcv9
     typedef unsigned int TNCBIAtomicValue;
#    define NCBI_COUNTER_UNSIGNED 1
     TNCBIAtomicValue NCBICORE_asm_cas(TNCBIAtomicValue new_value,
                                       TNCBIAtomicValue* address,
                                       TNCBIAtomicValue old_value);
     void* NCBICORE_asm_casx(void* new_value, void** location, void* old_value);
#    define NCBI_SWAP_POINTERS_CONDITIONALLY(loc, ov, nv) \
      (NCBICORE_asm_casx(nv, loc, ov) == ov)
#    define NCBI_SWAP_POINTERS_EXTERN 1
#  elif defined(__sparc)
     typedef unsigned int TNCBIAtomicValue;
#    define NCBI_COUNTER_RESERVED_VALUE 0x3FFFFFFF
#    define NCBI_COUNTER_UNSIGNED 1
     TNCBIAtomicValue NCBICORE_asm_swap(TNCBIAtomicValue new_value,
                                        TNCBIAtomicValue* address);
#    define NCBI_SWAP_POINTERS(loc, nv) \
      ((void*))(NCBICORE_asm_swap((TNCBIAtomicValue)(nv), \
                                  (TNCBIAtomicValue*)(loc)))
#    define NCBI_SWAP_POINTERS_EXTERN 1
#  elif defined(__x86_64)
     typedef unsigned int TNCBIAtomicValue;
#    define NCBI_COUNTER_UNSIGNED 1
     TNCBIAtomicValue NCBICORE_asm_lock_xaddl_64(TNCBIAtomicValue* address,
                                                 int delta);
     void* NCBICORE_asm_xchgq(void* new_value, void** location);
#    define NCBI_COUNTER_ADD(p, d) (NCBICORE_asm_lock_xaddl_64(p, d) + d)
#    define NCBI_COUNTER_USE_EXTERN_ASM 1
#    define NCBI_SWAP_POINTERS(loc, nv) NCBICORE_asm_xchgq(nv, loc)
#    define NCBI_SWAP_POINTERS_EXTERN 1
#  elif defined(__i386)
     typedef unsigned int TNCBIAtomicValue;
#    define NCBI_COUNTER_UNSIGNED 1
     TNCBIAtomicValue NCBICORE_asm_lock_xaddl(TNCBIAtomicValue* address,
                                              int delta);
     void* NCBICORE_asm_xchg(void* new_value, void** location);
#    define NCBI_COUNTER_ADD(p, d) (NCBICORE_asm_lock_xaddl(p, d) + d)
#    define NCBI_COUNTER_USE_EXTERN_ASM 1
#    define NCBI_SWAP_POINTERS(loc, nv) NCBICORE_asm_xchg(nv, loc)
#    define NCBI_SWAP_POINTERS_EXTERN 1
#  else
#    undef NCBI_COUNTER_ASM_OK
#  endif
#  ifdef __cplusplus
}
#  endif
#elif defined(NCBI_OS_DARWIN)  &&  defined(NCBI_COMPILER_GCC)  &&  defined(__ppc__)  &&  defined(__OPTIMIZE__)  &&  NCBI_COMPILER_VERSION >= 420  &&  NCBI_COMPILER_VERSION < 430
/* Work around a compiler bug by forcing use of an alternate implementation. */
#elif defined(_CXXCONFIG)
#  if defined(__GLIBCXX__) && __GLIBCXX__ >= 20070514 /* 4.2 */
#    include <ext/atomicity.h>
#  else
#    include <bits/atomicity.h>
#  endif
   typedef _Atomic_word TNCBIAtomicValue;
#  if defined(__GLIBCXX__) && __GLIBCXX__ >= 20040419 /* 3.4; redundant? */
#    define NCBI_COUNTER_ADD(p, d) (__gnu_cxx::__exchange_and_add(p, d) + d)
#  else
#    define NCBI_COUNTER_ADD(p, d) (__exchange_and_add(p, d) + d)
#  endif
#elif defined(NCBI_COMPILER_COMPAQ)
#  include <machine/builtins.h>
   typedef int TNCBIAtomicValue;
#  define NCBI_COUNTER_ADD(p, d) (__ATOMIC_ADD_LONG(p, d) + d)
#  define NCBI_SWAP_POINTERS(loc, nv) \
    ((void*)(__ATOMIC_EXCH_QUAD((loc), (long)(nv))))
#endif

#if defined(NCBI_NO_THREADS)
/* Already handled, but checked again here to avoid extra indentation */
#elif defined(NCBI_OS_IRIX)
#  include <mutex.h>
#  ifndef NCBI_COUNTER_ADD
     typedef __uint32_t TNCBIAtomicValue;
#    define NCBI_COUNTER_UNSIGNED 1
#    define NCBI_COUNTER_ADD(p, d) add_then_test32(p, d)
#  endif
#  define NCBI_SWAP_POINTERS(loc, nv) \
    ((void*) (test_and_set((unsigned long*)(loc), (unsigned long)(nv))))
#elif defined(NCBI_OS_AIX)
#  include <sys/atomic_op.h>
#  ifndef NCBI_COUNTER_ADD
     typedef int TNCBIAtomicValue;
#    define NCBI_COUNTER_ADD(p, d) (fetch_and_add(p, d) + d)
#  endif
#  define NCBI_SWAP_POINTERS_CONDITIONALLY(loc, ov, nv) \
    (compare_and_swap((atomic_p)(loc), (int*)(&(ov)), (int)(nv)) != FALSE)
#elif defined(NCBI_OS_DARWIN)  &&  0  /* even CarbonCore is deprecated */
#  include <CarbonCore/DriverSynchronization.h>
#  if !defined(NCBI_COUNTER_ADD)  &&  !defined(NCBI_COUNTER_USE_ASM)
     typedef SInt32 TNCBIAtomicValue;
#    define NCBI_COUNTER_ADD(p, d) (AddAtomic(d, p) + d)
#  endif
#  if SIZEOF_VOIDP == 4
#    define NCBI_SWAP_POINTERS_CONDITIONALLY(loc, ov, nv) \
      CompareAndSwap((UInt32)(ov), (UInt32)(nv), (UInt32*)(loc))
#  endif
#elif defined(NCBI_OS_MSWIN)
#  include <corelib/impl/ncbi_os_mswin.h>
#  if !defined(NCBI_COUNTER_ADD)  &&  !defined(NCBI_COUNTER_USE_ASM)
#   if NCBI_PLATFORM_BITS >= 64
#     define NCBI_COUNTER_64_BIT
#   endif
#   ifdef NCBI_COUNTER_64_BIT
     typedef LONGLONG TNCBIAtomicValue;
#    define NCBI_COUNTER_ADD(p, d) (InterlockedExchangeAdd64(p, d) + d)
#   else
     typedef LONG TNCBIAtomicValue;
#    define NCBI_COUNTER_ADD(p, d) (InterlockedExchangeAdd(p, d) + d)
#   endif
#  endif
#  define NCBI_SWAP_POINTERS(loc, nv) (InterlockedExchangePointer(loc, nv))
#elif !defined(NCBI_COUNTER_ADD)  &&  !defined(NCBI_COUNTER_USE_ASM)
   typedef unsigned int TNCBIAtomicValue;
#  define NCBI_COUNTER_UNSIGNED 1
#  if defined (NCBI_COUNTER_ASM_OK) && (defined(__i386) || defined(__sparc) || defined(__x86_64))
#    define NCBI_COUNTER_USE_ASM 1
#  else
#    define NCBI_COUNTER_NEED_MUTEX 1
#  endif
#endif

#if !defined(NCBI_SWAP_POINTERS)  &&  !defined(NCBI_SWAP_POINTERS_CONDITIONALLY)  &&  !defined(NCBI_NO_THREADS)  &&  (!defined(NCBI_COUNTER_ASM_OK)  ||  (!defined(__i386) && !defined(__powerpc__) && !defined(__powerpc64__) && !defined(__ppc__) && !defined(__ppc64__) && !defined(__sparc) && !defined(__x86_64)))
#  define NCBI_SWAP_POINTERS_EXTERN 1
#  define NCBI_SLOW_ATOMIC_SWAP 1
#endif

#ifndef NCBI_ATOMIC_TYPE
#  define NCBI_ATOMIC_TYPE(t) t
#endif

/* @} */

#endif /* CORELIB___NCBI_ATOMIC_DEFS__HPP */
