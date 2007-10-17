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
 * Authors:  Anton Lavrentiev
 *
 * File Description:   UNIX specifics
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_os_unix.hpp>
#include <corelib/error_codes.hpp>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>


#define NCBI_USE_ERRCODE_X   Corelib_Unix


BEGIN_NCBI_SCOPE


bool Daemonize(const char* logfile, TDaemonFlags flags)
{
    int fdin  = ::dup(STDIN_FILENO);
    int fdout = ::dup(STDOUT_FILENO);
    int fderr = ::dup(STDERR_FILENO);

    try {
        if (flags & fDaemon_KeepStdin) {
            int nullr = ::open("/dev/null", O_RDONLY);
            if (nullr < 0)
                throw "Error opening /dev/null for reading";
            if (nullr != STDIN_FILENO) {
                int error = ::dup2(nullr, STDIN_FILENO);
                int x_errno = errno;
                ::close(nullr);
                if (error < 0) {
                    errno = x_errno;
                    throw "Error redirecting stdin";
                }
            }
        }
        if (flags & fDaemon_KeepStdout) {
            int nullw = ::open("/dev/null", O_WRONLY);
            if (nullw < 0)
                throw "Error opening /dev/null for writing";
            ::fflush(stdout);
            if (nullw != STDOUT_FILENO) {
                int error = ::dup2(nullw, STDOUT_FILENO);
                int x_errno = errno;
                ::close(nullw);
                if (error < 0) {
                    ::dup2(fdin, STDIN_FILENO);
                    errno = x_errno;
                    throw "Error redirecting stdout";
                }
            }
        }
        if (logfile) {
            int fd = (!*logfile ? ::open("/dev/null", O_WRONLY | O_APPEND) :
                      ::open(logfile, O_WRONLY | O_APPEND | O_CREAT, 0666));
            if (fd < 0)
                throw "Unable to open logfile for stderr";
            ::fflush(stderr);
            if (fd != STDERR_FILENO) {
                int error = ::dup2(fd, STDERR_FILENO);
                int x_errno = errno;
                ::close(fd);
                if (error < 0) {
                    ::dup2(fdin,  STDIN_FILENO);
                    ::dup2(fdout, STDOUT_FILENO);
                    errno = x_errno;
                    throw "Error redirecting stderr";
                }
            }
        }
        pid_t pid = fork();
        if (pid == (pid_t)(-1)) {
            int x_errno = errno;
            ::dup2(fdin,  STDIN_FILENO);
            ::dup2(fdout, STDOUT_FILENO);
            ::dup2(fderr, STDERR_FILENO);
            errno = x_errno;
            throw "Cannot fork";
        }
        if (pid)
            ::_exit(0);
        if (!(flags & fDaemon_DontChroot))
            (void) ::chdir("/"); // "/" always exists
        if (!(flags & fDaemon_KeepStdin))
            ::fclose(stdin);
        ::close(fdin);
        if (!(flags & fDaemon_KeepStdout))
            ::fclose(stdout);
        ::close(fdout);
        if (!logfile)
            ::fclose(stderr);
        ::close(fderr);
        ::setsid();
        return true;
    }
    catch (const char* what) {
        int x_errno = errno;
        ERR_POST_X(1, string("[Daemonize]  ") + what);
        ::close(fdin);
        ::close(fdout);
        ::close(fderr);
        errno = x_errno;
    }
    return false;
}


END_NCBI_SCOPE
