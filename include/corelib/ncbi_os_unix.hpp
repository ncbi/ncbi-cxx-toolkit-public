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


#include <corelib/ncbistl.hpp>
#if !defined(NCBI_OS_UNIX)
#  error "ncbi_os_unix.hpp must be used on UNIX platforms only"
#endif

#include <unistd.h>


BEGIN_NCBI_SCOPE


/// Daemonization flags
enum FDaemonFlags {
    fDaemon_DontChroot = 1,  ///< Don't change to "/"
    fDaemon_KeepStdin  = 2,  ///< Keep stdin open as "/dev/null" (RO)
    fDaemon_KeepStdout = 4,  ///< Keep stdout open as "/dev/null" (WO)
    fDaemon_ImmuneTTY  = 8   ///< Make daemon immune to opening controlling TTY
};
/// Bit-wise OR of FDaemonFlags @sa FDaemonFlags
typedef unsigned int TDaemonFlags;


/// Go daemon.
///
/// Return true in the daemon thread.
/// Return false on error (no daemon created), errno can be used to analyze.
/// Reopen stderr/cerr in daemon thread if "logfile" specified as non-NULL
/// (stderr will open to "/dev/null" if "logfile" == ""),
/// otherwise stderr is closed in the daemon thread.
/// NB: Always check stderr for errors of failed redirection!
///
/// Unless instructed by "flags" parameter, the daemon thread has its stdin/cin
/// and stdout/cout closed, and current directory changed to root (/).
/// If kept open, stdin and stdout are both redirected to /dev/null.
/// Opening a terminal device as a controlling terminal is allowed, unless
/// fDaemon_ImmuneTTY is specified in the flags, which then causes a second
/// fork() so that the resultant process won't be allowed to open a TTY as
/// its controlling TTY (but only with explicit O_NOCTTY, see open(2)), thus
/// protecting the process from any blocking via TTY signalling.
///
/// Note that this call is somewhat destructive and may not be able
/// to restore the process that called it to a state prior to the call
/// in case of an error.  So that calling process can find std file
/// pointers (and sometimes descriptors) screwed up.

bool Daemonize(const char* logfile = 0, TDaemonFlags flags = 0);


END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_OS_UNIX__HPP */
