#ifndef CORELIB___NCBITHR_CONF__HPP
#define CORELIB___NCBITHR_CONF__HPP

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
 * Author:  Eugene Vasilchenko
 *
 *
 */

/// @file ncbithr_conf.hpp
/// Multi-threading configuration.


#include <corelib/ncbistd.hpp>

#if defined(NCBI_WIN32_THREADS)
#  include <corelib/ncbi_os_mswin.hpp>
#elif defined(NCBI_POSIX_THREADS)
extern "C" {
#    include <pthread.h>
}
#    include <sys/errno.h>
#endif


/** @addtogroup Threads
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// DECLARATIONS of internal (platform-dependent) representations
//
//    TTlsKey          -- internal TLS key type
//    TThreadHandle    -- platform-dependent thread handle type
//    TThreadSystemID  -- platform-dependent thread ID type
//
//  NOTE:  all these types are intended for internal use only!
//

#if defined(NCBI_WIN32_THREADS)

/// Define internal TLS key type.
typedef DWORD  TTlsKey;

/// Define platform-dependent thread handle type.
typedef HANDLE TThreadHandle;

/// Define platform-dependent thread ID type.
typedef DWORD  TThreadSystemID;

/// Define platform-dependent result wrapper.
typedef DWORD  TWrapperRes;

/// Define platform-dependent argument wrapper.
typedef LPVOID TWrapperArg;

#elif defined(NCBI_POSIX_THREADS)

/// Define internal TLS key type.
typedef pthread_key_t TTlsKey;

/// Define platform-dependent thread handle type.
typedef pthread_t     TThreadHandle;

/// Define platform-dependent thread ID type.
typedef pthread_t     TThreadSystemID;

/// Define platform-dependent result wrapper.
typedef void* TWrapperRes;

/// Define platform-dependent argument wrapper.
typedef void* TWrapperArg;

#else

// fake

/// Define internal TLS key type.
typedef void* TTlsKey;

/// Define platform-dependent thread handle type.
typedef int   TThreadHandle;

/// Define platform-dependent thread ID type.
typedef int   TThreadSystemID;

/// Define platform-dependent result wrapper.
typedef void* TWrapperRes;

/// Define platform-dependent argument wrapper.
typedef void* TWrapperArg;

#endif

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2003/08/18 16:03:33  siyan
 * Added CORELIB___ prefix to the #ifndef/#define file inclusion check macros.
 *
 * Revision 1.6  2003/08/04 12:41:19  siyan
 * Documentation changes.
 *
 * Revision 1.5  2003/06/03 18:24:28  rsmith
 * wrap includes of sched.h (explicit and implicit) in extern "c" blocks, since Apples headers do not do it themselves.
 *
 * Revision 1.4  2003/03/31 13:30:52  siyan
 * Minor changes to doxygen support
 *
 * Revision 1.3  2003/03/31 13:16:59  siyan
 * Added doxygen support
 *
 * Revision 1.2  2002/09/20 14:14:05  vasilche
 * Fixed inconsistency of NCBI_*_THREADS macros
 *
 * Revision 1.1  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * ===========================================================================
 */
#endif
