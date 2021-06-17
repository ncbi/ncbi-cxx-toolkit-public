#ifndef COMMON___NCBI_SANITIZERS__H
#define COMMON___NCBI_SANITIZERS__H
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
 * Author:  Vladimir Ivanov
 *
 */

/// @file ncbi_sanitizers.h
/// Common macro to suppress memory leaks if run under LeakSanitizer.


#if defined(__has_feature)
#  if __has_feature(memory_sanitizer)
#      define NCBI_USE_LSAN
#  endif
#else
// Fallback for older compilers
#  if defined(__SANITIZE_ADDRESS__)
#      define NCBI_USE_LSAN
#  endif
#endif

/////////////////////////////////////////////////////////////////////////////
///
/// LeakSanitazer
///

#if defined(NCBI_USE_LSAN)
#  include <sanitizer/lsan_interface.h>

/// Disable/enable LeakSanitizer.
///
/// Allocations made between calls to NCBI_LSAN_DISABLE and NCBI_LSAN_ENABLE
/// will be treated as non-leaks. Disable/enable pairs may be nested, 
/// but always should match.
/// @sa NCBI_LSAN_DISABLE_GUARD

#  define NCBI_LSAN_DISABLE __lsan_disable()
#  define NCBI_LSAN_ENABLE  __lsan_enable()

/// Disable LeakSanitizer for a current scope.
///
/// All memory allocations between NCBI_LSAN_DISABLE_GUARD and the rest
/// of the current scope will be treated as non-leaks.
/// @sa NCBI_LSAN_DISABLE, NCBI_LSAN_ENABLE

#  define NCBI_LSAN_DISABLE_GUARD \
    __lsan::ScopedDisabler _lsan_scoped_disabler

#else
#  define NCBI_LSAN_ENABLE         ((void)0)
#  define NCBI_LSAN_DISABLE        ((void)0)
#  define NCBI_LSAN_DISABLE_GUARD  ((void)0)
#endif


#endif /*COMMON___NCBI_SANITIZERS__H */
