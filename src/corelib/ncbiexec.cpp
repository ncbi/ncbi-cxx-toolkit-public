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
 * Authors:  Vladimir Ivanov
 *
 */

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <stdarg.h>
#include <corelib/ncbiexec.hpp>
#include <corelib/ncbi_process.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <process.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <errno.h>
#elif defined(NCBI_OS_MAC)
#  error "Class CExec defined only for MS Windows and UNIX platforms"
#endif


BEGIN_NCBI_SCOPE


#if defined(NCBI_OS_MSWIN)

// Convert CExec class mode to the real mode
static int s_GetRealMode(CExec::EMode mode)
{
    static const int s_Mode[] =  { 
        P_OVERLAY, P_WAIT, P_NOWAIT, P_DETACH 
    };

    int x_mode = (int) mode;
    _ASSERT(0 <= x_mode  &&  x_mode < sizeof(s_Mode)/sizeof(s_Mode[0]));
    return s_Mode[x_mode];
}
#endif


#if defined(NCBI_OS_UNIX)

// Type function to call
enum ESpawnFunc {eV, eVE, eVP, eVPE};

static int s_SpawnUnix(ESpawnFunc func, CExec::EMode mode, 
                       const char *cmdname, const char *const *argv, 
                       const char *const *envp = (const char *const*)0)
{
    // Empty environment for Spawn*E
    const char* empty_env[] = { 0 };
    if ( !envp ) {
        envp = empty_env;
    }

    // Replace the current process image with a new process image.
    if (mode == CExec::eOverlay) {
        switch (func) {
        case eV:
            return execv(cmdname, const_cast<char**>(argv));
        case eVP:
            return execvp(cmdname, const_cast<char**>(argv));
        case eVE:
        case eVPE:
            return execve(cmdname, const_cast<char**>(argv), 
                          const_cast<char**>(envp));
        }
        return -1;
    }
    // Fork child process
    pid_t pid;
    switch (pid = fork()) {
    case -1:
        // fork failed
        return -1;
    case 0:
        // Here we're the child
        if (mode == CExec::eDetach) {
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
            setsid();
        }
        int status =-1;
        switch (func) {
        case eV:
            status = execv(cmdname, const_cast<char**>(argv));
            break;
        case eVP:
            status = execvp(cmdname, const_cast<char**>(argv));
            break;
        case eVE:
        case eVPE:
            status = execve(cmdname, const_cast<char**>(argv),
                            const_cast<char**>(envp));
            break;
        }
        _exit(status);
    }
    // The "pid" contains the childs pid
    if ( mode == CExec::eWait ) {
        return CExec::Wait(pid);
    }
    return pid;
}

#endif


// On 64-bit platforms, check argument, passed into function with variable
// number of arguments, on possible using 0 instead NULL as last argument.
// Of course, the argument 'arg' can be aligned on segment boundary,
// when 4 low-order bytes are 0, but chance of this is very low.
// The function prints out a warning only in Debug mode.
#if defined(_DEBUG)  &&  SIZEOF_VOIDP > SIZEOF_INT
static void s_CheckExecArg(const char* arg)
{
#  if defined(WORDS_BIGENDIAN)
    int lo = int(((Uint8)arg >> 32) & 0xffffffffU);
    int hi = int((Uint8)arg & 0xffffffffU);
#  else
    int hi = int(((Uint8)arg >> 32) & 0xffffffffU);
    int lo = int((Uint8)arg & 0xffffffffU);
#  endif
    if (lo == 0  &&  hi != 0) {
        ERR_POST(Warning
                 << "It is possible that you used 0 instead of NULL "
                 "to terminate the argument list of a CExec::Spawn*() call.");
    }
}
#else
#  define s_CheckExecArg(x) 
#endif

// Get exec arguments
#define GET_EXEC_ARGS \
    int xcnt = 2; \
    va_list vargs; \
    va_start(vargs, argv); \
    while ( va_arg(vargs, const char*) ) xcnt++; \
    va_end(vargs); \
    const char **args = new const char*[xcnt+1]; \
    typedef ArrayDeleter<const char*> TArgsDeleter; \
    AutoPtr<const char*, TArgsDeleter> p_args(args); \
    if ( !args ) return -1; \
    args[0] = cmdname; \
    args[1] = argv; \
    va_start(vargs, argv); \
    int xi = 1; \
    while ( xi < xcnt ) { \
        xi++; \
        args[xi] = va_arg(vargs, const char*); \
        s_CheckExecArg(args[xi]); \
    } \
    args[xi] = (const char*)0


int CExec::System(const char *cmdline)
{ 
    int status;
#if defined(NCBI_OS_MSWIN)
    status = system(cmdline); 
#elif defined(NCBI_OS_UNIX)
    status = system(cmdline);
#endif
    if (status == -1) {
        NCBI_THROW(CExecException,eSystem,
                   "CExec::System: call to system failed");
    }
#if defined(NCBI_OS_UNIX)
    return cmdline ? WEXITSTATUS(status) : status;
#else
    return status;
#endif
}


int CExec::SpawnL(EMode mode, const char *cmdname, const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    status = spawnv(s_GetRealMode(mode), cmdname, &cmdname);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    status = s_SpawnUnix(eV, mode, cmdname, args);
#endif
    if (status == -1) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnL");
    }
    return status;
}


int CExec::SpawnLE(EMode mode, const char *cmdname,  const char *argv, ...)
{
    int status;
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
#if defined(NCBI_OS_MSWIN)
    status = spawnve(s_GetRealMode(mode), cmdname, args, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVE, mode, cmdname, args, envp);
#endif
    if (status == -1) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnLE");
    }
    return status;
}


int CExec::SpawnLP(EMode mode, const char *cmdname, const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    status = spawnvp(s_GetRealMode(mode), cmdname, &cmdname);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    status = s_SpawnUnix(eVP, mode, cmdname, args);
#endif
    if (status == -1) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnLP");
    }
    return status;
}


int CExec::SpawnLPE(EMode mode, const char *cmdname, const char *argv, ...)
{
    int status;
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
#if defined(NCBI_OS_MSWIN)
    status = spawnve(s_GetRealMode(mode), cmdname, args, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVPE, mode, cmdname, args, envp);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnLPE");
    }
    return status;
}


int CExec::SpawnV(EMode mode, const char *cmdname, const char *const *argv)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnv(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eV, mode, cmdname, argv);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnV");
    }
    return status;
}


int CExec::SpawnVE(EMode mode, const char *cmdname, 
                   const char *const *argv, const char * const *envp)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnve(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVE, mode, cmdname, argv, envp);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnVE");
    }
    return status;
}


int CExec::SpawnVP(EMode mode, const char *cmdname, const char *const *argv)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnvp(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVP, mode, cmdname, argv);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnVP");
    }
    return status;
}


int CExec::SpawnVPE(EMode mode, const char *cmdname,
                    const char *const *argv, const char * const *envp)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnvpe(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVPE, mode, cmdname, argv, envp);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnVPE");
    }
    return status;
}


int CExec::Wait(int handle, unsigned long timeout)
{
    return CProcess(handle, CProcess::eHandle).Wait(timeout);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2004/08/19 17:08:58  ucko
 * More cleanups to s_CheckExecArg; in particular, perform the sizeof
 * test at compilation-time rather than runtime.
 *
 * Revision 1.21  2004/08/19 17:01:22  ucko
 * Fix typos in s_CheckExecArg.
 *
 * Revision 1.20  2004/08/19 12:20:11  ivanov
 * Lines wrapped at 79th column.
 *
 * Revision 1.19  2004/08/19 12:18:02  ivanov
 * Added s_CheckExecArg() to check argument, passed into function with
 * variable number of arguments, on possible using 0 instead NULL as last
 * argument on 64-bit platforms.
 *
 * Revision 1.18  2004/08/18 16:00:50  ivanov
 * Use NULL instead 0 where necessary to avoid problems with 64bit platforms
 *
 * Revision 1.17  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.16  2003/10/01 20:22:05  ivanov
 * Formal code rearrangement
 *
 * Revision 1.15  2003/09/25 17:02:20  ivanov
 * CExec::Wait():  replaced all code with CProcess::Wait() call
 *
 * Revision 1.14  2003/09/16 17:48:08  ucko
 * Remove redundant "const"s from arguments passed by value.
 *
 * Revision 1.13  2003/08/12 16:57:55  ivanov
 * s_SpawnUnix(): use execv() instead execvp() for the eV-mode
 *
 * Revision 1.12  2003/04/23 21:02:48  ivanov
 * Removed redundant NCBI_OS_MAC preprocessor directives
 *
 * Revision 1.11  2003/02/24 19:56:05  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.10  2002/08/15 18:26:29  ivanov
 * Changed s_SpawnUnix() -- empty environment, setsid() for eDetach mode
 *
 * Revision 1.9  2002/07/17 15:12:34  ivanov
 * Changed method of obtaining parameters in the SpawnLE/LPE functions
 * under MS Windows
 *
 * Revision 1.8  2002/07/16 15:04:22  ivanov
 * Fixed warning about uninitialized variable in s_SpawnUnix()
 *
 * Revision 1.7  2002/07/15 16:38:50  ivanov
 * Fixed bug in macro GET_EXEC_ARGS -- write over array bound
 *
 * Revision 1.6  2002/07/11 14:18:26  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.5  2002/06/30 03:22:14  vakatov
 * s_GetRealMode() -- formal code rearrangement to avoid a warning
 *
 * Revision 1.4  2002/06/11 19:28:31  ivanov
 * Use AutoPtr<char*> for exec arguments in GET_EXEC_ARGS
 *
 * Revision 1.3  2002/06/04 19:43:20  ivanov
 * Done s_ThrowException static
 *
 * Revision 1.2  2002/05/31 20:49:33  ivanov
 * Removed excrescent headers
 *
 * Revision 1.1  2002/05/30 16:29:13  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
