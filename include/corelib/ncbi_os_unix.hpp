#ifndef CORELIB___NCBI_OS_UNIX__HPP
#define CORELIB___NCBI_OS_UNIX__HPP

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
 * Author:  Anton Lavrentiev
 *
 */

/// @file ncbi_os_unix.hpp
/// UNIX-specifics
///


#include <corelib/ncbi_process.hpp>
#ifndef NCBI_OS_UNIX
#  error "ncbi_os_unix.hpp must be used on UNIX platforms only"
#endif

#ifdef NCBI_COMPILER_GCC
#  warning "This header currently defines a deprecated feature only; \
please consider using <corelib/ncbi_process.hpp> instead"
#endif

BEGIN_NCBI_SCOPE


/// Daemonization flags
enum FDaemonFlags {
    fDaemon_DontChroot = CProcess::fDontChroot,
    fDaemon_KeepStdin  = CProcess::fKeepStdin,
    fDaemon_KeepStdout = CProcess::fKeepStdout,
    fDaemon_ImmuneTTY  = CProcess::fImmuneTTY
};
/// Bit-wise OR of FDaemonFlags @sa FDaemonFlags
typedef unsigned int TDaemonFlags;

inline
NCBI_DEPRECATED
bool Daemonize(const char* logfile = 0, TDaemonFlags flags = 0)
{
    return CProcess::Daemonize(logfile, flags);
} 


END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_OS_UNIX__HPP */
