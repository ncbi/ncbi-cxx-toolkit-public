#ifndef COMMON___NCBICONF_IMPL__H
#define COMMON___NCBICONF_IMPL__H

/* $Id$
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
 *  Author:  Anton Lavrentiev
 *
 *
 */

/**
 * @file ncbiconf_impl.h
 *
 * Configuration macros.
 */

#ifndef FORWARDING_NCBICONF_H
#  error "The header can be used from <ncbiconf.h> only."
#endif /*!FORWARDING_NCBICONF_H*/


/** @addtogroup Portability
 *
 * @{
 */

/* Convenience macro for use when the precise vendor and version
   number don't matter. */
#if defined(NCBI_COMPILER_APPLE_CLANG)  ||  defined(NCBI_COMPILER_LLVM_CLANG)
#  define NCBI_COMPILER_ANY_CLANG 1
#endif

/* Threads configuration
 */

#undef NCBI_NO_THREADS
#undef NCBI_THREADS
#undef NCBI_POSIX_THREADS
#undef NCBI_WIN32_THREADS

#if defined(_MT)  &&  !defined(NCBI_WITHOUT_MT)
#  if defined(NCBI_OS_MSWIN)
#    define NCBI_WIN32_THREADS
#  elif defined(NCBI_OS_UNIX)
#    define NCBI_POSIX_THREADS
#  else
#    define NCBI_NO_THREADS
#  endif
#else
#  define NCBI_NO_THREADS
#endif

#if !defined(NCBI_NO_THREADS)
#  define NCBI_THREADS
#endif

/* Sync Windows/Cygwin preprocessor conditionals governing wide
 * character usage. */

#if defined(UNICODE)  &&  !defined(_UNICODE)
#  define _UNICODE 1
#elif defined(_UNICODE)  &&  !defined(UNICODE)
#  define UNICODE 1
#endif

/* New/nonstandard keywords and attributes
 */

// Check whether the argument names a C++ attribute supported in some
// fashion.  NB: For *unscoped* attributes, a nonzero value may merely
// indicate support for legacy __attribute__ syntax; please use
// NCBI_HAS_STD_ATTRIBUTE to test whether C++11-style [[...]]  syntax
// works in such cases.  This macro is good as is for attributes with
// implementation-specific scopes like gnu:: or clang::, and either
// should work for hypothetical attributes scoped by std:: or stdN::.
#if defined(__cplusplus)  &&  defined(__has_cpp_attribute)
#  define NCBI_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#  define NCBI_HAS_CPP_ATTRIBUTE(x) 0
#endif

// Likewise, for C attributes.
#if !defined(__cplusplus)  &&  defined(__has_c_attribute)
#  define NCBI_HAS_C_ATTRIBUTE(x) __has_c_attribute(x)
#else
#  define NCBI_HAS_C_ATTRIBUTE(x) 0
#endif

// Likewise, generically.
#ifdef __cplusplus
#  define NCBI_HAS_ATTRIBUTE(x) NCBI_HAS_CPP_ATTRIBUTE(x)
#elif defined(__clang__)
#  define NCBI_HAS_ATTRIBUTE(x) 0
#else
#  define NCBI_HAS_ATTRIBUTE(x) NCBI_HAS_C_ATTRIBUTE(x)
#endif

// Check whether the argument names a supported *standard* attribute
// specifiable via C++11/C23-style [[...]] syntax.  Valid only for
// unscoped attributes and for hypothetical attributes scoped by std::
// or stdN::.
#ifdef __cplusplus
#  ifdef __clang__
#    define NCBI_HAS_STD_ATTRIBUTE(x) \
    (NCBI_HAS_CPP_ATTRIBUTE(x) >= 200809L  \
     &&  NCBI_HAS_CPP_ATTRIBUTE(x) <= __cplusplus)
#  else
#    define NCBI_HAS_STD_ATTRIBUTE(x) (NCBI_HAS_CPP_ATTRIBUTE(x) >= 200809L)
#  endif
#else
#  ifdef __clang__
#    define NCBI_HAS_STD_ATTRIBUTE(x) \
    (NCBI_HAS_CPP_ATTRIBUTE(x) >= 201904L  \
     &&  NCBI_HAS_C_ATTRIBUTE(x) <= __STDC_VERSION__)
#  else
#    define NCBI_HAS_STD_ATTRIBUTE(x) (NCBI_HAS_C_ATTRIBUTE(x) >= 201904L)
#  endif
#endif

// Allow safely checking for GCC(ish)/IBM __attribute__ support.
#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif

#if defined(__cplusplus)  &&  defined(NCBI_RESTRICT_CXX)
#  define NCBI_RESTRICT NCBI_RESTRICT_CXX
#elif !defined(__cplusplus)  &&  defined(NCBI_RESTRICT_C)
#  define NCBI_RESTRICT NCBI_RESTRICT_C
#elif __STDC_VERSION__ >= 199901 /* C99 specifies restrict */
#  define NCBI_RESTRICT restrict
#else
#  define NCBI_RESTRICT
#endif

#ifdef NCBI_DEPRECATED
#  undef NCBI_DEPRECATED
#endif
/* C++11 [[deprecated]] and legacy synonyms aren't fully interchangeable;
 * depending on the compiler and context, using one form rather than the
 * other may yield a warning or even an outright error.  */
#if __has_attribute(deprecated)
#  define NCBI_LEGACY_DEPRECATED_0      __attribute__((deprecated))
#  define NCBI_LEGACY_DEPRECATED_1(msg) __attribute__((deprecated(msg)))
#elif defined(NCBI_COMPILER_MSVC)
#  define NCBI_LEGACY_DEPRECATED_0      __declspec(deprecated)
#  define NCBI_LEGACY_DEPRECATED_1(msg) __declspec(deprecated(msg))
#else
#  define NCBI_LEGACY_DEPRECATED_0
#  define NCBI_LEGACY_DEPRECATED_1(msg)
#endif
#if NCBI_HAS_STD_ATTRIBUTE(deprecated)  || \
  (defined(__cplusplus)  &&  defined(NCBI_COMPILER_MSVC))
#  define NCBI_STD_DEPRECATED_0          [[deprecated]]
#  define NCBI_STD_DEPRECATED_1(message) [[deprecated(message)]]
#else
#  define NCBI_STD_DEPRECATED_0          NCBI_LEGACY_DEPRECATED_0
#  define NCBI_STD_DEPRECATED_1(message) NCBI_LEGACY_DEPRECATED_1(message)
#endif
#if !defined(NCBI_COMPILER_GCC)  ||  NCBI_COMPILER_VERSION >= 600
#  define NCBI_STD_DEPRECATED(message) NCBI_STD_DEPRECATED_1(message)
#else
#  define NCBI_STD_DEPRECATED(message)
#endif
#if 0
#  define NCBI_DEPRECATED NCBI_STD_DEPRECATED_0
#else
#  define NCBI_DEPRECATED NCBI_LEGACY_DEPRECATED_0
#endif

#ifndef NCBI_FORCEINLINE
#  ifdef __cplusplus
#    define NCBI_FORCEINLINE inline
#  else
#    define NCBI_FORCEINLINE
#  endif
#endif

#ifdef HAVE_ATTRIBUTE_DESTRUCTOR
#  undef HAVE_ATTRIBUTE_DESTRUCTOR
#endif
#if __has_attribute(destructor)
#  define HAVE_ATTRIBUTE_DESTRUCTOR 1
#endif

#ifndef NCBI_NORETURN
#  if NCBI_HAS_STD_ATTRIBUTE(noreturn)
#    define NCBI_NORETURN [[noreturn]]
#  elif __has_attribute(noreturn)
#    define NCBI_NORETURN __attribute__((__noreturn__))
#  elif defined(NCBI_COMPILER_MSVC)
#    define NCBI_NORETURN __declspec(noreturn)
#  else
#    define NCBI_NORETURN
#  endif
#endif

#ifdef NCBI_WARN_UNUSED_RESULT
#  undef NCBI_WARN_UNUSED_RESULT
#endif
#if NCBI_HAS_STD_ATTRIBUTE(nodiscard)
#  define NCBI_WARN_UNUSED_RESULT [[nodiscard]]
#elif NCBI_HAS_ATTRIBUTE(gnu::warn_unused_result)
#  define NCBI_WARN_UNUSED_RESULT [[gnu::warn_unused_result]]
#elif NCBI_HAS_ATTRIBUTE(clang::warn_unused_result)
#  define NCBI_WARN_UNUSED_RESULT [[clang::warn_unused_result]]
#elif __has_attribute(warn_unused_result)
#  define NCBI_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(NCBI_COMPILER_MSVC)
#  define NCBI_WARN_UNUSED_RESULT _Check_return_
#else
#  define NCBI_WARN_UNUSED_RESULT
#endif
    
#if NCBI_HAS_STD_ATTRIBUTE(fallthrough)
#  define NCBI_FALLTHROUGH [[fallthrough]]
#elif NCBI_HAS_ATTRIBUTE(gnu::fallthrough)
#  define NCBI_FALLTHROUGH [[gnu::fallthrough]]
#elif NCBI_HAS_ATTRIBUTE(clang::fallthrough)
#  define NCBI_FALLTHROUGH [[clang::fallthrough]]
#elif __has_attribute(fallthrough)
#  define NCBI_FALLTHROUGH __attribute__ ((fallthrough))
#else
#  define NCBI_FALLTHROUGH
#endif

#ifdef NCBI_PACKED
#  undef NCBI_PACKED
#endif
#if 0 // NCBI_HAS_STD_ATTRIBUTE(packed) -- speculative
#  define NCBI_PACKED [[packed]]
#elif NCBI_HAS_ATTRIBUTE(gnu::packed)
#  define NCBI_PACKED [[gnu::packed]]
#elif __has_attribute(packed)
#  define NCBI_PACKED __attribute__((packed))
#else
#  define NCBI_PACKED
#endif

/* Definition of packed enum type, to save some memory */
/* enum EMyEnum NCBI_PACKED_ENUM_TYPE(Type) { ... } NCBI_PACKED_ENUM_END(); */
#ifndef NCBI_PACKED_ENUM_TYPE
#  define NCBI_PACKED_ENUM_TYPE(type) : type
#endif
#ifndef NCBI_PACKED_ENUM_END
#  define NCBI_PACKED_ENUM_END()
#endif

#ifndef NCBI_UNUSED
#  if NCBI_HAS_STD_ATTRIBUTE(maybe_unused)
#    define NCBI_UNUSED [[maybe_unused]]
#  elif NCBI_HAS_ATTRIBUTE(gnu::unused)
#    define NCBI_UNUSED [[gnu::unused]]
#  elif __has_attribute(unused)
#    define NCBI_UNUSED __attribute__((unused))
#  else
#    define NCBI_UNUSED
#  endif
#endif

#if defined(__SSE4_2__)  ||  defined(__AVX__)
#  define NCBI_SSE 42
#elif defined(__SSE4_1__)
#  define NCBI_SSE 41
#elif defined(__SSSE3__)
#  define NCBI_SSE 40
#elif defined(__SSE3__)
#  define NCBI_SSE 30
#elif defined(__SSE2__)  ||  defined(_M_AMD64)  ||  defined(_M_X64) \
    ||  (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#  define NCBI_SSE 20
#elif defined(__SSE__)  ||  (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#  define NCBI_SSE 10
#endif

#ifdef __cplusplus
#  define NCBI_HAVE_CXX11 1
#  define HAVE_IS_SORTED 1
#  define HAVE_NULLPTR 1
#  define HAVE_ENUM_CLASS 1

#  if defined(NCBI_COMPILER_MSVC) && defined(_MSVC_LANG)
#    if (_MSVC_LANG >= 201700)
#      define NCBI_HAVE_CXX17 1
#    endif
#  else
#    if (__cplusplus >= 201700)
#      define NCBI_HAVE_CXX17 1
#    endif
#  endif
#endif


/* Whether there is a proven sufficient support for the 'thread_local'.
 * NOTE that this is a very conservative estimation which can be extended if
 * needed (after proper vetting and testing, of course) to additional
 * platforms. FYI, the known (or at least suspected) issues with some other
 * platforms are:
 *  - Clang  - may not work well on MacOS (runtime; may depend on LIBC)
 *  - VS2015 - may not export well from DLLs
 */
#if (defined(_MSC_VER) && _MSC_VER >= 1914) || \
    (defined(NCBI_COMPILER_GCC) && NCBI_COMPILER_VERSION >= 730)
#  define HAVE_THREAD_LOCAL 1
#endif



#include <common/ncbi_skew_guard.h>


/* @} */

#endif  /* COMMON___NCBICONF_IMPL__H */
