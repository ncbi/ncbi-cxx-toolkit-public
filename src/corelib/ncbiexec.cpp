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
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/error_codes.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <process.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <errno.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <fcntl.h>
#elif defined(NCBI_OS_MAC)
#  error "Class CExec defined only for MS Windows and UNIX platforms"
#endif


#define NCBI_USE_ERRCODE_X   Corelib_System


BEGIN_NCBI_SCOPE



TExitCode CExec::CResult::GetExitCode(void)
{
    if ( (m_Flags & fExitCode) == 0 ) {
        NCBI_THROW(CExecException, eResult,
                   "CExec:: CResult contains process handle, not exit code");
    }
    return m_Result.exitcode;
}

TProcessHandle CExec::CResult::GetProcessHandle(void)
{
    if ( (m_Flags & fHandle) == 0 ) {
        NCBI_THROW(CExecException, eResult,
                   "CExec:: CResult contains process exit code, not handle");
    }
    return m_Result.handle;
}

CExec::CResult::operator intptr_t(void) const
{
    switch (m_Flags) {
        case fExitCode:
            return (intptr_t)m_Result.exitcode;
        case fHandle:
            return (intptr_t)m_Result.handle;
        default:
            NCBI_THROW(CExecException, eResult,
                       "CExec:: CResult undefined concersion");
    }
    // Not reached
    return 0;
}


#if defined(NCBI_OS_MSWIN)

// Convert CExec class mode to the real mode
static int s_GetRealMode(CExec::EMode mode)
{
    static const int s_Mode[] =  { 
        P_OVERLAY, P_WAIT, P_NOWAIT, P_DETACH 
    };

    // Translate only master modes and ignore all additional modes on MS Windows.
    int x_mode = (int) mode & CExec::fModeMask;
    _ASSERT(0 <= x_mode  &&  x_mode < sizeof(s_Mode)/sizeof(s_Mode[0]));
    return s_Mode[x_mode];
}
#endif


#if defined(NCBI_OS_UNIX)

// Type function to call
enum ESpawnFunc {eV, eVE, eVP, eVPE};

static int 
s_SpawnUnix(ESpawnFunc func, CExec::EMode full_mode, 
            const char *cmdname, const char *const *argv, 
            const char *const *envp = (const char *const*)0)
{
    // Empty environment for Spawn*E
    const char* empty_env[] = { 0 };
    if ( !envp ) {
        envp = empty_env;
    }

    // Get master mode
    CExec::EMode mode = (CExec::EMode)(full_mode & (int)CExec::fModeMask);

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
    
    // Create temporary pipe to get status of execution
    // of the child process
    int status_pipe[2];
    if (pipe(status_pipe) < 0) {
        NCBI_THROW(CExecException, eSpawn,
                   "CExec:: Failed to create status pipe");
    }
    fcntl(status_pipe[0], F_SETFL, 
    fcntl(status_pipe[0], F_GETFL, 0) & ~O_NONBLOCK);
    fcntl(status_pipe[1], F_SETFD, 
    fcntl(status_pipe[1], F_GETFD, 0) | FD_CLOEXEC);
    
    // Fork child process
    pid_t pid;
    switch (pid = fork()) {
    case (pid_t)(-1):
        // fork failed
        return -1;
    case 0:
        // Now we are in the child process
        
        // Close unused pipe handle
        close(status_pipe[0]);
        
        if (mode == CExec::eDetach) {
            freopen("/dev/null", "r", stdin);
            freopen("/dev/null", "a", stdout);
            freopen("/dev/null", "a", stderr);
            setsid();
        } 

        if (((int)full_mode  &  CExec::fNewGroup) == CExec::fNewGroup) {
            setpgid(0, 0);
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
        // Error executing exec*(), report error code to parent process
        int errcode = errno;
        write(status_pipe[1], &errcode, sizeof(errcode));
        close(status_pipe[1]);
        _exit(status);
    }
    
    // Check status pipe.
    // If it have some data, this is an errno from the child process.
    // If EOF in status pipe, that child executed successful.
    // Retry if either blocked or interrupted
    
    close(status_pipe[1]);    
   
    // Try to read errno from forked process
    ssize_t n;
    int errcode;
    while ((n = read(status_pipe[0], &errcode, sizeof(errcode))) < 0) {
        if (errno != EINTR)
            break;
    }
    close(status_pipe[0]);
    if (n > 0) {
        // Child could not run -- rip it and exit with error
        waitpid(pid, 0, 0);
        errno = n >= sizeof(errcode) ? errcode : 0;        
        return -1;
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
        ERR_POST_X(10, Warning <<
                       "It is possible that you used 0 instead of NULL "
                       "to terminate the argument list of a CExec::Spawn*() call.");
    }
}
#else
#  define s_CheckExecArg(x) 
#endif

// Macros to get exec arguments

#if defined(NCBI_OS_MSWIN)
#  define GET_ARGS_0 \
    AutoPtr<string> p_cmdname; \
    if ( strstr(cmdname, " ") ) { \
        string* tmp = new string(string("\"") + cmdname + "\""); \
        p_cmdname.reset(tmp); \
        args[0] = tmp->c_str(); \
    } else { \
        args[0] = cmdname; \
    }
#else
#  define GET_ARGS_0 \
    args[0] = cmdname
#endif

#define GET_EXEC_ARGS \
    int xcnt = 2; \
    va_list vargs; \
    va_start(vargs, argv); \
    while ( va_arg(vargs, const char*) ) xcnt++; \
    va_end(vargs); \
    const char **args = new const char*[xcnt+1]; \
    typedef ArrayDeleter<const char*> TArgsDeleter; \
    AutoPtr<const char*, TArgsDeleter> p_args(args); \
    if ( !args ) \
        NCBI_THROW(CCoreException, eNullPtr, kEmptyStr); \
    GET_ARGS_0; \
    args[1] = argv; \
    va_start(vargs, argv); \
    int xi = 1; \
    while ( xi < xcnt ) { \
        xi++; \
        args[xi] = va_arg(vargs, const char*); \
        s_CheckExecArg(args[xi]); \
    } \
    args[xi] = (const char*)0


// Return result from Spawn method
#define RETURN_RESULT(func) \
    if (status == -1) { \
        NCBI_THROW(CExecException, eSpawn, "CExec::" #func "() failed"); \
    } \
    CResult result; \
    if ((mode & eWait) == eWait) { \
        result.m_Flags = CResult::fExitCode; \
        result.m_Result.exitcode = (TExitCode)status; \
    } else { \
        result.m_Flags = CResult::fHandle; \
        result.m_Result.handle = (TProcessHandle)status; \
    } \
    return result


TExitCode CExec::System(const char *cmdline)
{ 
    int status;
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = system(cmdline); 
#elif defined(NCBI_OS_UNIX)
    status = system(cmdline);
#endif
    if (status == -1) {
        NCBI_THROW(CExecException, eSystem,
                   "CExec::System: call to system failed");
    }
#if defined(NCBI_OS_UNIX)
    if (cmdline) {
        return WIFSIGNALED(status) ? WTERMSIG(status) + 0x80
            : WEXITSTATUS(status);
    } else {
        return status;
    }
#else
    return status;
#endif
}


CExec::CResult
CExec::SpawnL(EMode mode, const char *cmdname, const char *argv, ...)
{
    intptr_t status;
    GET_EXEC_ARGS;

#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnv(s_GetRealMode(mode), cmdname, args);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eV, mode, cmdname, args);
#endif
    RETURN_RESULT(SpawnL);
}


CExec::CResult
CExec::SpawnLE(EMode mode, const char *cmdname,  const char *argv, ...)
{
    intptr_t status;
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnve(s_GetRealMode(mode), cmdname, args, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVE, mode, cmdname, args, envp);
#endif
    RETURN_RESULT(SpawnLE);
}


CExec::CResult
CExec::SpawnLP(EMode mode, const char *cmdname, const char *argv, ...)
{
    intptr_t status;
    GET_EXEC_ARGS;
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnvp(s_GetRealMode(mode), cmdname, args);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVP, mode, cmdname, args);
#endif
    RETURN_RESULT(SpawnLP);
}


CExec::CResult
CExec::SpawnLPE(EMode mode, const char *cmdname, const char *argv, ...)
{
    intptr_t status;
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnve(s_GetRealMode(mode), cmdname, args, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVPE, mode, cmdname, args, envp);
#endif
    RETURN_RESULT(SpawnLPE);
}


CExec::CResult
CExec::SpawnV(EMode mode, const char *cmdname, const char *const *argv)
{
    intptr_t status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnv(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eV, mode, cmdname, argv);
#endif
    RETURN_RESULT(SpawnV);
}


CExec::CResult
CExec::SpawnVE(EMode mode, const char *cmdname, 
               const char *const *argv, const char * const *envp)
{
    intptr_t status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnve(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVE, mode, cmdname, argv, envp);
#endif
    RETURN_RESULT(SpawnVE);
}


CExec::CResult
CExec::SpawnVP(EMode mode, const char *cmdname, const char *const *argv)
{
    intptr_t status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnvp(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVP, mode, cmdname, argv);
#endif
    RETURN_RESULT(SpawnVP);
}


CExec::CResult
CExec::SpawnVPE(EMode mode, const char *cmdname,
                const char *const *argv, const char * const *envp)
{
    intptr_t status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    _flushall();
    status = spawnvpe(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVPE, mode, cmdname, argv, envp);
#endif
    RETURN_RESULT(SpawnVPE);
}


TExitCode CExec::Wait(TProcessHandle handle, unsigned long timeout)
{
    return CProcess(handle, CProcess::eHandle).Wait(timeout);
}


// Predefined timeout (in milliseconds)
const unsigned long kWaitPrecision = 100;

int CExec::Wait(list<TProcessHandle>& handles, 
                EWaitMode             mode,
                list<CResult>&        result,
                unsigned long         timeout)
{
    typedef list<TProcessHandle>::iterator THandleIt;
    result.clear();

    for (;;) {
        // Check each process
        for (THandleIt it = handles.begin(); it != handles.end(); ) {
            TProcessHandle handle = *it;
            TExitCode exitcode = Wait(handle, 0);
            if ( exitcode != -1 ) {
                CResult res;
                res.m_Flags = CResult::fBoth;
                res.m_Result.handle = handle;
                res.m_Result.exitcode = exitcode;
                result.push_back(res);
                THandleIt cur = it;
                ++it;
                handles.erase(cur);
            } else {
                ++it;
            }
        }
        if ( (mode == eWaitAny  &&  !result.empty())  ||
             (mode == eWaitAll  &&  handles.empty()) ) {
            break;
        }
        // Wait before next loop
        unsigned long x_sleep = kWaitPrecision;
        if (timeout != kInfiniteTimeoutMs) {
            if (x_sleep > timeout) {
                x_sleep = timeout;
            }
            if ( !x_sleep ) {
                break;
            }
            timeout -= x_sleep;
        }
        SleepMilliSec(x_sleep);
    }
    // Return number of terminated processes
    return (int)result.size();
}


CExec::CResult CExec::RunSilent(EMode mode, const char *cmdname,
                                const char *argv, ... /*, NULL */)
{
    intptr_t status = -1;

#if defined(NCBI_OS_MSWIN)

#  if defined(NCBI_COMPILER_MSVC)
    // This is Microsoft extention, and some compilers do not it.
    _flushall();
#  endif
    STARTUPINFO         StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    const int           kMaxCmdLength = 4096;
    string              cmdline;

    // Set startup info
    memset(&StartupInfo, 0, sizeof(StartupInfo));
    StartupInfo.cb          = sizeof(STARTUPINFO);
    StartupInfo.dwFlags     = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE;
    DWORD dwCreateFlags     = (mode == eDetach) ? 
                              DETACHED_PROCESS : CREATE_NEW_CONSOLE;

    // Compose command line
    cmdline.reserve(kMaxCmdLength);
    cmdline = cmdname;

    if (argv) {
        cmdline += " "; 
        cmdline += argv;
        va_list vargs;
        va_start(vargs, argv);
        const char* p = NULL;
        while ( (p = va_arg(vargs, const char*)) ) {
            cmdline += " "; 
            cmdline += CExec::QuoteArg(p);
        }
        va_end(vargs);
    }

    // MS Windows: ignore all extra flags.
    mode = EMode(mode & fModeMask);

    // Just check mode parameter
    s_GetRealMode(mode);

    // Run program
    if (CreateProcess(NULL, (LPSTR)cmdline.c_str(), NULL, NULL, FALSE,
                      dwCreateFlags, NULL, NULL, &StartupInfo, &ProcessInfo))
    {
        if (mode == eOverlay) {
            // destroy ourselves
            _exit(0);
        }
        else if (mode == eWait) {
            // wait running process
            WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
            DWORD exitcode = -1;
            GetExitCodeProcess(ProcessInfo.hProcess, &exitcode);
            status = exitcode;
            CloseHandle(ProcessInfo.hProcess);
        }
        else if (mode == eDetach) {
            // detached asynchronous spawn,
            // just close process handle, return 0 for success
            CloseHandle(ProcessInfo.hProcess);
            status = 0;
        }
        else if (mode == eNoWait) {
            // asynchronous spawn -- return PID
            status = (intptr_t)ProcessInfo.hProcess;
        }
        CloseHandle(ProcessInfo.hThread);
    }

#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    status = s_SpawnUnix(eV, mode, cmdname, args);

#endif
    RETURN_RESULT(RunSilent);
}


string CExec::QuoteArg(const string& arg)
{
    // Enclose argument in quotes if it is empty,
    // or contains spaces and not contains quotes.
    if ( arg.empty()  ||
        (arg.find(' ') != NPOS  &&  arg.find('"') == NPOS) ) {
        return '"' + arg + '"';
    }
    return arg;
}

bool CExec::IsExecutable(const string& path)
{
    CFile f(path);
    if (f.Exists()  &&
        f.CheckAccess(CFile::fExecute)) {
        return true;
    }
    return false;
}


string CExec::ResolvePath(const string& filename)
{
    string path = kEmptyStr;

    // Absolute path
    if ( CDirEntry::IsAbsolutePath(filename) ) {
        if ( IsExecutable(filename) ) {
            path = filename;
        }
    } else {

    // Relative path

        string tmp = filename;
#  ifdef NCBI_OS_MSWIN
        // Add default ".exe" extention to the name of executable file
        // if it running without extension
        string dir, title, ext;
        CDirEntry::SplitPath(filename, &dir, &title, &ext);
        if ( ext.empty() ) {
            tmp = CDirEntry::MakePath(dir, title, "exe");
        }
#  endif
        if ( CFile(tmp).Exists() ) {
            // Relative path from the the current directory
            tmp = CDir::GetCwd() + CDirEntry::GetPathSeparator() + tmp;
            if ( IsExecutable(tmp) ) {
                path = tmp;
            }
        } else {
            // Check on relative path with sub-directories,
            // ignore such filenames.
            size_t sep = tmp.find_first_of("/\\");
            if ( sep == NPOS ) {
                // Try to find filename among the paths of the PATH
                // environment variable.
                const char* env = getenv("PATH");
                if (env  &&  *env) {
                    list<string> split_path;
#  ifdef NCBI_OS_MSWIN
                    NStr::Split(env, ";", split_path);
#  else
                    NStr::Split(env, ":", split_path);
#  endif
                    ITERATE(list<string>, it, split_path) {
                        string p = CDirEntry::MakePath(*it, tmp);
                        if (CFile(p).Exists()  &&  IsExecutable(p)) {
                            path = p;
                            break;
                        }
                    } /* ITERATE */
                }     /* env */
            }         /* sep == NPOS*/
        }             /* CFile(tmp).Exists() */
    }
    // If found - normalize path 
    if ( !path.empty() ) {
        path = CDirEntry::NormalizePath(path);
    }
    return path;
}


const char* CExecException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eSystem: return "eSystem";
    case eSpawn:  return "eSpawn";
    case eResult: return "eResult";
    default:      return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
