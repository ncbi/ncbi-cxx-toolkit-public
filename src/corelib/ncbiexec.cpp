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

#include <corelib/ncbiexec.hpp>
#include <stdio.h>
#include <stdarg.h>

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


// Throw exception with diagnostic message
static void s_ThrowException(const string& what)
{
    const char* errmsg = strerror(errno);
    if ( !errmsg ) {
        errmsg = "unknown reason";
    }
    // Throw exception
    throw CExec::CException("CExec: " + what + ": " + errmsg);
}


#if defined(NCBI_OS_MSWIN)

// Real exec modes
#  define EXEC_MODE_COUNT  4
const int _exec_mode[EXEC_MODE_COUNT] =  { 
    P_OVERLAY, P_WAIT, P_NOWAIT, P_DETACH 
};

// Convert CExec class mode to the real mode
static int s_GetRealMode(const CExec::EMode mode)
{
    if ( mode < 0  ||  mode >= EXEC_MODE_COUNT ) 
        _TROUBLE;
    return _exec_mode[mode];
}
#endif


#if defined(NCBI_OS_UNIX)

// Type function to call
enum ESpawnFunc {eV, eVE, eVP, eVPE};

static int s_SpawnUnix(const ESpawnFunc func, const CExec::EMode mode, 
                       const char *cmdname, const char *const *argv, 
                       const char *const *envp = 0)
{
    // Replace the current process image with a new process image.
    if (mode == CExec::eOverlay) {
        switch (func) {
        case eV:
        case eVP:
            return execv(cmdname, const_cast<char**>(argv));
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
        }
        int status;
        switch (func) {
        case eV:
            status = execv(cmdname, const_cast<char**>(argv));
        case eVP:
            status = execvp(cmdname, const_cast<char**>(argv));
        case eVE:
        case eVPE:
            status = execve(cmdname, const_cast<char**>(argv),
                            const_cast<char**>(envp));
        }
        _exit(status);
    }
    // "pid" contains the childs pid
    if (mode == CExec::eWait) {
        return CExec::Wait(pid);
    }
    return pid;
}


// Get exec arguments
#define GET_EXEC_ARGS \
    int xcnt = 2; \
    va_list vargs; \
    va_start(vargs, argv); \
    while ( va_arg(vargs, const char*) ) xcnt++; \
    va_end(vargs); \
    const char **args = new const char*[xcnt]; \
    typedef ArrayDeleter<const char*> TArgsDeleter; \
    AutoPtr<const char*, TArgsDeleter> p_args(args); \
    if ( !args ) return -1; \
    args[0] = cmdname; \
    args[1] = argv; \
    va_start(vargs, argv); \
    int xi = 1; \
    while ( args[xi]  &&  xi < xcnt) { \
        xi++; \
        args[xi] = va_arg(vargs, const char*); \
    } \
    args[xi] = 0

#endif


// Check status of exec call
#define CHECK_EXEC_STATUS(procname) \
    if (status == -1 ) { \
        s_ThrowException((string)procname + "() error"); \
    } \
    return status


int CExec::System(const char *cmdline)
{ 
    int status;
#if defined(NCBI_OS_MSWIN)
    status = system(cmdline); 
#elif defined(NCBI_OS_UNIX)
    status = system(cmdline);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    if (status == -1 ) {
        s_ThrowException("System() error");
    }
#if defined(NCBI_OS_UNIX)
    return cmdline ? WEXITSTATUS(status) : status;
#else
    return status;
#endif
}


int CExec::SpawnL(const EMode mode, const char *cmdname, const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    status = spawnv(s_GetRealMode(mode), cmdname, &cmdname);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    status = s_SpawnUnix(eV, mode, cmdname, args);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnL");
}


int CExec::SpawnLE(const EMode mode, const char *cmdname,  const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    const char **envp;
    envp = &cmdname;
    while (*envp++);
    status = spawnve(s_GetRealMode(mode), cmdname, &cmdname, (char**)*envp);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
    status = s_SpawnUnix(eVE, mode, cmdname, args, envp);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnLE");
}


int CExec::SpawnLP(const EMode mode, const char *cmdname, 
                   const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    status = spawnvp(s_GetRealMode(mode), cmdname, &cmdname);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    status = s_SpawnUnix(eVP, mode, cmdname, args);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnLP");
}


int CExec::SpawnLPE(const EMode mode, const char *cmdname,
                    const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    const char **envp;
    envp = &cmdname;
    while (*envp++);
    status = spawnvpe(s_GetRealMode(mode), cmdname, &cmdname, (char**)*envp);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
    status = s_SpawnUnix(eVPE, mode, cmdname, args, envp);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnLPE");
}


int CExec::SpawnV(const EMode mode, const char *cmdname,
                  const char *const *argv)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnv(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eV, mode, cmdname, argv);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnV");
}


int CExec::SpawnVE(const EMode mode, const char *cmdname, 
                   const char *const *argv, const char * const *envp)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnve(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVE, mode, cmdname, argv, envp);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnVE");
}


int CExec::SpawnVP(const EMode mode, const char *cmdname,
                   const char *const *argv)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnvp(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVP, mode, cmdname, argv);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnVP");
}


int CExec::SpawnVPE(const EMode mode, const char *cmdname,
                    const char *const *argv, const char * const *envp)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnvpe(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVPE, mode, cmdname, argv, envp);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    CHECK_EXEC_STATUS("SpawnVPE");
}


int CExec::Wait(const int pid)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    if ( cwait(&status, pid, 0) == -1 ) 
        return -1;
#elif defined(NCBI_OS_UNIX)
    if ( waitpid(pid, &status, 0) == -1 ) 
        return -1;
    status = WEXITSTATUS(status);
#elif defined(NCBI_OS_MAC)
    ?
#endif
    return status;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
