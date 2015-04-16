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
 * Authors:  Aaron Ucko, Vladimir Ivanov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbierror.hpp>
#include "ncbisys.hpp"

#if   defined(NCBI_OS_UNIX)
#  include <errno.h>
#  include <fcntl.h>
#  include <signal.h>
#  include <stdio.h>
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#elif defined(NCBI_OS_MSWIN)
#  include <corelib/ncbitime.hpp>  // for CStopWatch
#  include <process.h>
#  include <tlhelp32.h>
#  pragma warning (disable : 4191)
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Process


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// CProcess::CExitInfo
//

// CExitInfo process state
enum EExitInfoState {
    eExitInfo_Unknown = 0,
    eExitInfo_Alive,
    eExitInfo_Terminated
};


#define EXIT_INFO_CHECK                                         \
  if ( !IsPresent() ) {                                         \
      NCBI_THROW(CCoreException, eCore,                         \
                 "CProcess::CExitInfo state is unknown. "       \
                 "Please check CExitInfo::IsPresent() first."); \
  }


CProcess::CExitInfo::CExitInfo(void)
{
    state  = eExitInfo_Unknown;
    status = 0;
}


bool CProcess::CExitInfo::IsPresent(void) const
{
    return state != eExitInfo_Unknown;
}


bool CProcess::CExitInfo::IsAlive(void) const
{
    EXIT_INFO_CHECK;
    return state == eExitInfo_Alive;
}


bool CProcess::CExitInfo::IsExited(void) const
{
    EXIT_INFO_CHECK;
    if (state != eExitInfo_Terminated) {
        return false;
    }
#if   defined(NCBI_OS_UNIX)
    return WIFEXITED(status) != 0;
#elif defined(NCBI_OS_MSWIN)
    // The process always terminates with exit code
    return true;
#endif
}


bool CProcess::CExitInfo::IsSignaled(void) const
{
    EXIT_INFO_CHECK;
    if (state != eExitInfo_Terminated) {
        return false;
    }
#if   defined(NCBI_OS_UNIX)
    return WIFSIGNALED(status) != 0;
#elif defined(NCBI_OS_MSWIN)
    // The process always terminates with exit code
    return false;
#endif
}


int CProcess::CExitInfo::GetExitCode(void) const
{
    if ( !IsExited() ) {
        return -1;
    }
#if   defined(NCBI_OS_UNIX)
    return WEXITSTATUS(status);
#elif defined(NCBI_OS_MSWIN)
    return status;
#endif
}


int CProcess::CExitInfo::GetSignal(void) const
{
    if ( !IsSignaled() ) {
        return -1;
    }
#if   defined(NCBI_OS_UNIX)
    return WTERMSIG(status);
#elif defined(NCBI_OS_MSWIN)
    return -1;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//
// CProcess 
//

// Predefined timeouts (in milliseconds)
const unsigned long           kWaitPrecision        = 100;
const unsigned long CProcess::kDefaultKillTimeout   = 1000;


CProcess::CProcess(TPid process, EProcessType type)
    : m_Process(process), m_Type(type)
{
    return;
}

#ifdef NCBI_OS_MSWIN
// The helper constructor for MS Windows to avoid cast from
// TProcessHandle to TPid
CProcess::CProcess(TProcessHandle process, EProcessType type)
    : m_Process((intptr_t)process), m_Type(type)
{
    return;
}
#endif //NCBI_OS_MSWIN


#ifdef NCBI_THREAD_PID_WORKAROUND
#  ifndef NCBI_OS_UNIX
#    error "NCBI_THREAD_PID_WORKAROUND should only be defined on UNIX!"
#  endif
TPid CProcess::sx_GetPid(EGetPidFlag flag)
{
    if ( flag == ePID_GetThread ) {
        // Return real PID, do not cache it.
        return getpid();
    }

    DEFINE_STATIC_FAST_MUTEX(s_GetPidMutex);
    static TPid s_CurrentPid = 0;
    static TPid s_ParentPid = 0;

    if (CThread::GetSelf() == 0) {
        // For main thread always force caching of PIDs
        CFastMutexGuard guard(s_GetPidMutex);
        s_CurrentPid = getpid();
        s_ParentPid = getppid();
    }
    else {
        // For child threads update cached PIDs only if there was a fork
        // First call is always from the main thread (explicit or through
        // CThread::Run()), s_CurrentPid must be != 0 in any child thread.
        _ASSERT(s_CurrentPid);
        TPid pid = getpid();
        TPid thr_pid = CThread::sx_GetThreadPid();
        if (thr_pid  &&  thr_pid != pid) {
            // Thread's PID has changed - fork detected.
            // Use current PID and PPID as globals.
            CThread::sx_SetThreadPid(pid);
            CFastMutexGuard guard(s_GetPidMutex);
            s_CurrentPid = pid;
            s_ParentPid = getppid();
        }
    }
    return flag == ePID_GetCurrent ? s_CurrentPid : s_ParentPid;
}
#endif //NCBI_THREAD_PID_WORKAROUND

TProcessHandle CProcess::GetCurrentHandle(void)
{
#if   defined(NCBI_OS_MSWIN)
    return GetCurrentProcess();
#elif defined(NCBI_OS_UNIX)
    return GetCurrentPid();
#endif
}


TPid CProcess::GetCurrentPid(void)
{
#if   defined(NCBI_OS_MSWIN)
    return GetCurrentProcessId();
#elif defined NCBI_THREAD_PID_WORKAROUND
    return sx_GetPid(ePID_GetCurrent);
#elif defined(NCBI_OS_UNIX)
    return getpid();
#endif
}


TPid CProcess::GetParentPid(void)
{
#if   defined(NCBI_OS_MSWIN)
    TPid ppid = (TPid)(-1);
    // Open snapshot handle
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != INVALID_HANDLE_VALUE) {

        PROCESSENTRY32 pe;
        DWORD pid = GetCurrentProcessId();
        pe.dwSize = sizeof(PROCESSENTRY32);

        BOOL retval = Process32First(hSnapshot, &pe);
        while (retval) {
            if (pe.th32ProcessID == pid) {
                ppid = pe.th32ParentProcessID;
                break;
            }
            pe.dwSize = sizeof(PROCESSENTRY32);
            retval = Process32Next(hSnapshot, &pe);
        }

        // close snapshot handle
        CloseHandle(hSnapshot);
    }
    return ppid;
#elif defined NCBI_THREAD_PID_WORKAROUND
    return sx_GetPid(ePID_GetParent);
#elif defined(NCBI_OS_UNIX)
    return getppid();
#endif
}

#ifdef NCBI_OS_UNIX
namespace {
    class CErrnoKeeper
    {
    public:
        CErrnoKeeper() : m_Errno(errno) {}
        int GetErrno() const {return m_Errno;}
        ~CErrnoKeeper() {errno = m_Errno;}
    private:
        int m_Errno;
    };
}

static string s_ErrnoToString()
{
    CErrnoKeeper x_errno;
    const char* error = strerror(x_errno.GetErrno());
    return error != NULL && *error != '\0' ? string(error) :
            ("errno=" + NStr::NumericToString(x_errno.GetErrno()));
}
#endif

TPid CProcess::Fork(CProcess::TForkFlags flags)
{
#ifdef NCBI_OS_UNIX
    TPid pid = ::fork();
    if (pid == 0)
        // Only update PID and UID in the child process.
        CDiagContext::UpdateOnFork((flags & fFF_UpdateDiag) != 0 ?
                CDiagContext::fOnFork_ResetTimer |
                        CDiagContext::fOnFork_PrintStart : 0);
    else if (pid == (TPid) -1 && (flags & fFF_AllowExceptions) != 0) {
        NCBI_THROW_FMT(CCoreException, eCore,
                "Cannot fork: " << s_ErrnoToString());
    }

    return pid;
#else
    NCBI_THROW(CCoreException, eCore,
               "CProcess::Fork() not implemented on this platform");
#endif
}

#ifdef NCBI_OS_UNIX
namespace {
    class CSafeRedirect
    {
    public:
        CSafeRedirect(int fd, bool* success_flag) :
            m_OrigFD(fd),
            m_SuccessFlag(success_flag),
            m_Redirected(false)
        {
            if ((m_DupFD = ::fcntl(fd, F_DUPFD, STDERR_FILENO + 1)) < 0) {
                NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                        "Error duplicating file descriptor #" << fd <<
                        ": " << s_ErrnoToString());
            }
        }
        void Redirect(int new_fd)
        {
            if (new_fd != m_OrigFD) {
                int error = ::dup2(new_fd, m_OrigFD);
                if (error < 0) {
                    CErrnoKeeper x_errno;
                    ::close(new_fd);
                    NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                            "Error redirecting file descriptor #" << m_OrigFD <<
                            ": " << s_ErrnoToString());
                }
                ::close(new_fd);
                m_Redirected = true;
            }
        }
        ~CSafeRedirect()
        {
            CErrnoKeeper x_errno;
            if (m_Redirected && !*m_SuccessFlag)
                // Restore the original std I/O stream descriptor.
                ::dup2(m_DupFD, m_OrigFD);
            ::close(m_DupFD);
        }

    private:
        int m_OrigFD;
        int m_DupFD;
        bool* m_SuccessFlag;
        bool m_Redirected;
    };
}

TPid CProcess::x_DaemonizeEx(const char* logfile, CProcess::TDaemonFlags flags)
{
    if (!(flags & fDF_AllowThreads)) {
        if (unsigned n = CThread::GetThreadsCount()) {
            NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                    "Prohibited, there are already child threads running: " << n);
        }
    }

    bool success_flag = false;

    CSafeRedirect stdin_redirector(STDIN_FILENO, &success_flag);
    CSafeRedirect stdout_redirector(STDOUT_FILENO, &success_flag);
    CSafeRedirect stderr_redirector(STDERR_FILENO, &success_flag);

    int new_fd;

    if (flags & fDF_KeepStdin) {
        if ((new_fd = ::open("/dev/null", O_RDONLY)) < 0) {
            NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                    "Error opening /dev/null for reading: " <<
                    s_ErrnoToString());
        }
        stdin_redirector.Redirect(new_fd);
    }
    if (flags & fDF_KeepStdout) {
        if ((new_fd = ::open("/dev/null", O_WRONLY)) < 0) {
            NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                    "Error opening /dev/null for writing: " <<
                    s_ErrnoToString());
        }
        NcbiCout.flush();
        ::fflush(stdout);
        stdout_redirector.Redirect(new_fd);
    }
    if (logfile) {
        if (!*logfile) {
            if ((new_fd = ::open("/dev/null", O_WRONLY | O_APPEND)) < 0) {
                NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                        "Error opening /dev/null for appending: " <<
                        s_ErrnoToString());
            }
        } else {
            if ((new_fd = ::open(logfile,
                    O_WRONLY | O_APPEND | O_CREAT, 0666)) < 0) {
                NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                        "Unable to open logfile \"" << logfile <<
                        "\": " << s_ErrnoToString());
            }
        }
        NcbiCerr.flush();
        ::fflush(stderr);
        stderr_redirector.Redirect(new_fd);
    }
    ::fflush(NULL);
    TPid pid = Fork(fFF_UpdateDiag | fFF_AllowExceptions);
    if (pid) {
        // Parent process.
        // No need to set success_flag to true here, because
        // either this process must be terminated or
        // the descriptors must be restored.
        if ((flags & fDF_KeepParent) == 0) {
            GetDiagContext().PrintStop();
            ::_exit(0);
        }
        return (TPid) pid/*success*/;
    }
    // Child process.
    success_flag = true;
    ::setsid();
    if (flags & fDF_ImmuneTTY) {
        try {
            if (Fork() != 0)
                ::_exit(0); // Exit the second parent process
        }
        catch (CCoreException& e) {
            ERR_POST_X(2, "[Daemonize]  Failed to immune from "
                    "TTY accruals: " << e << " ... continuing anyways");
        }
    }
    if (!(flags & fDF_KeepCWD))
        if (::chdir("/") ) { /*no-op*/ };  // NB: "/" always exists
    if (!(flags & fDF_KeepStdin))
        ::fclose(stdin);
    else
        ::fflush(stdin);  // POSIX requires this
    if (!(flags & fDF_KeepStdout))
        ::fclose(stdout);
    if (!logfile)
        ::fclose(stderr);
    return (TPid)(-1)/*success*/;
}
#endif /* NCBI_OS_UNIX */

TPid CProcess::Daemonize(const char* logfile, CProcess::TDaemonFlags flags)
{
#ifdef NCBI_OS_UNIX
    if (flags & fDF_AllowExceptions)
        return CProcess::x_DaemonizeEx(logfile, flags);
    else
        try {
            return CProcess::x_DaemonizeEx(logfile, flags);
        }
        catch (CException& e) {
            CErrnoKeeper x_errno;
            ERR_POST_X(1, e);
        }
        catch (exception& e) {
            CErrnoKeeper x_errno;
            ERR_POST_X(1, e.what());
        }
#else
    NCBI_THROW(CCoreException, eCore,
               "CProcess::Daemonize() not implemented on this platform");
    /*NOTREACHED*/
#endif
    return (TPid) 0/*failure*/;
}


bool CProcess::IsAlive(void) const
{
#if   defined(NCBI_OS_UNIX)
    return kill((TPid)m_Process, 0) == 0  ||  errno == EPERM;

#elif defined(NCBI_OS_MSWIN)
    HANDLE hProcess = 0;
    if (m_Type == ePid) {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION,
                               FALSE, (TPid)m_Process);
        if (!hProcess) {
            CNcbiError::SetFromWindowsError();
            return GetLastError() == ERROR_ACCESS_DENIED;
        }
    } else {
        hProcess = (TProcessHandle)m_Process;
    }
    DWORD status = 0;
    _ASSERT(STILL_ACTIVE != 0);
    GetExitCodeProcess(hProcess, &status);
    if (m_Type == ePid) {
        CloseHandle(hProcess);
    }
    return status == STILL_ACTIVE;
#endif
}


bool CProcess::Kill(unsigned long timeout) const
{
#if   defined(NCBI_OS_UNIX)

    TPid pid = (TPid)m_Process;

    // Try to kill the process with SIGTERM first
    if (kill(pid, SIGTERM) < 0  &&  errno == EPERM) {
        CNcbiError::SetFromErrno();
        return false;
    }

    // Check process termination within the timeout
    unsigned long x_timeout = timeout;
    for (;;) {
        TPid reap = waitpid(pid, static_cast<int*>(NULL), WNOHANG);
        if (reap) {
            if (reap != (TPid)(-1)) {
                _ASSERT(reap == pid);
                return true;
            }
            if (errno != ECHILD) {
                CNcbiError::SetFromErrno();
                return false;
            }
            if (kill(pid, 0) < 0)
                return true;
        }
        unsigned long x_sleep = kWaitPrecision;
        if (x_sleep > x_timeout) {
            x_sleep = x_timeout;
        }
        if ( !x_sleep ) {
             break;
        }
        SleepMilliSec(x_sleep);
        x_timeout  -= x_sleep;
    }
    _ASSERT(!x_timeout);

    // Try harder to kill the stubborn process -- SIGKILL may not be caught!
    int res = kill(pid, SIGKILL);
    if ( !timeout ) {
        return res <= 0;
    }
    SleepMilliSec(kWaitPrecision);
    // Reap the zombie (if child) up from the system
    waitpid(pid, static_cast<int*>(NULL), WNOHANG);
    // Check whether the process cannot be killed
    // (most likely due to a kernel problem)
    return kill(pid, 0) < 0;

#elif defined(NCBI_OS_MSWIN)

    // Safe process termination
    bool safe = (timeout > 0);

    // Try to kill current process?
    if ( m_Type == ePid  &&  (TPid)m_Process == GetCurrentPid() ) {
        ExitProcess(-1);
        // NOTREACHED
        return false;
    }

    HANDLE hProcess    = NULL;
    HANDLE hThread     = NULL;
    bool   enable_sync = true;

    // Get process handle
    if (m_Type == eHandle) {
        hProcess = (TProcessHandle)m_Process;

    } else {  // m_Type == ePid
        hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_TERMINATE |
                               SYNCHRONIZE, FALSE, (TPid)m_Process);
        if ( !hProcess ) {
            // Try to open with minimal access right needed
            // to terminate process.
            enable_sync = false;
            hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (TPid)m_Process);
            if (!hProcess) {
                if (GetLastError() != ERROR_ACCESS_DENIED) {
                    CNcbiError::SetFromWindowsError();
                    return false;
                }
                // If we have an administrative rights, that we can try
                // to terminate the process using SE_DEBUG_NAME privilege,
                // which system administrators normally have, but it might
                // be disabled by default. When this privilege is enabled,
                // the calling thread can open processes with any access
                // rights regardless of the security descriptor assigned
                // to the process.

                // Get current thread token 
                HANDLE hToken;
                if (!OpenThreadToken(GetCurrentThread(), 
                                     TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES,
                                     FALSE, &hToken)) {
                    if (GetLastError() != ERROR_NO_TOKEN) {
                        CNcbiError::SetFromWindowsError();
                        return false;
                    }
                    // Rrevert to the process token, if not impersonating
                    if (!OpenProcessToken(GetCurrentProcess(),
                                          TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES,
                                          &hToken)) {
                        CNcbiError::SetFromWindowsError();
                        return false;
                    }
                }

                // Try to enable the SE_DEBUG_NAME privilege

                TOKEN_PRIVILEGES tp, tp_prev;
                DWORD            tp_prev_size = sizeof(tp_prev);

                tp.PrivilegeCount = 1;
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);

                if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp),
                                           &tp_prev, &tp_prev_size)) {
                    CNcbiError::SetFromWindowsError();
                    CloseHandle(hToken);
                    return false;
                }
                if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
                    // The AdjustTokenPrivileges function cannot add new
                    // privileges to the access token. It can only enable or
                    // disable the token's existing privileges.
                    CNcbiError::SetFromWindowsError();
                    CloseHandle(hToken);
                    return false;
                }

                // Try to open process handle again
                hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (TPid)m_Process);
                
                // Restore original privilege state
                AdjustTokenPrivileges(hToken, FALSE, &tp_prev, sizeof(tp_prev),
                                      NULL, NULL);
                CloseHandle(hToken);
            }
        }
    }

    // Check process handle
    if ( !hProcess  ||  hProcess == INVALID_HANDLE_VALUE ) {
        return true;
    }
    // Terminate process
    bool terminated = false;

    CStopWatch timer;
    if ( safe ) {
        timer.Start();
    }
    // Safe process termination
    if ( safe  &&  enable_sync ) {
        // (kernel32.dll loaded at same address in each process)
        FARPROC exitproc = GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),
                                          "ExitProcess");
        if ( exitproc ) {
            hThread = CreateRemoteThread(hProcess, NULL, 0,
                                        (LPTHREAD_START_ROUTINE)exitproc,
                                        0, 0, 0);
            // Wait until process terminated, or timeout expired
            if (hThread   &&
                (WaitForSingleObject(hProcess, timeout) == WAIT_OBJECT_0)){
                terminated = true;
            }
        }
    }
    // Try harder to kill stubborn process
    if ( !terminated ) {
        if ( TerminateProcess(hProcess, -1) != 0  ||
            GetLastError() == ERROR_INVALID_HANDLE ) {
            // If process "terminated" succesfuly or error occur but
            // process handle became invalid -- process has terminated
            terminated = true;
        }
        else {
            CNcbiError::SetFromWindowsError();
        }
    }
    if (safe  &&  terminated) {
        // The process terminating now.
        // Reset flag, and wait for real process termination.

        terminated = false;
        double elapsed = timer.Elapsed() * kMilliSecondsPerSecond;
        unsigned long linger_timeout = (elapsed < timeout) ? 
            (unsigned long)((double)timeout - elapsed) : 0;

        for (;;) {
            if ( !IsAlive() ) {
                terminated = true;
                break;
            }
            unsigned long x_sleep = kWaitPrecision;
            if (x_sleep > linger_timeout) {
                x_sleep = linger_timeout;
            }
            if ( !x_sleep ) {
                break;
            }
            SleepMilliSec(x_sleep);
            linger_timeout -= x_sleep;
        }
    }
    // Close temporary process handle
    if ( hThread ) {
        CloseHandle(hThread);
    }
    if (m_Type == ePid) {
        CloseHandle(hProcess);
    }
    return terminated;

#endif
}


#ifdef NCBI_OS_MSWIN

// MS Windows:
// A helper function for terminating all processes
// in the tree within specified timeout.

// If 'timer' is specified we use safe process termination.
static bool s_KillGroup(DWORD pid,
                        CStopWatch *timer, unsigned long &timeout)
{
    // Open snapshot handle.
    // We cannot use one shapshot for recursive calls, 
    // because it is not reentrant.
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    // Terminate all children first
    if (!Process32First(hSnapshot, &pe)) {
        return false;
    }
    do {
        if (pe.th32ParentProcessID == pid) {
            // Safe termination -- update timeout
            if ( timer ) {
                double elapsed = timer->Elapsed() * kMilliSecondsPerSecond;
                timeout = (elapsed < timeout) ?
                    (unsigned long)((double)timeout - elapsed) : 0;
                if ( !timeout ) {
                    CloseHandle(hSnapshot);
                    return false;
                }
            }
            bool res = s_KillGroup(pe.th32ProcessID, timer, timeout);
            if ( !res ) {
                CloseHandle(hSnapshot);
                return false;
            }
        }
    }
    while (Process32Next(hSnapshot, &pe)); 

    // Terminate the specified process

    // Safe termination -- update timeout
    if ( timer ) {
        double elapsed = timer->Elapsed() * kMilliSecondsPerSecond;
        timeout = (elapsed < timeout) ?
            (unsigned long)((double)timeout - elapsed) : 0;
        if ( !timeout ) {
            CloseHandle(hSnapshot);
            return false;
        }
    }
    bool res = CProcess(pid, CProcess::ePid).Kill(timeout);

    // Close snapshot handle
    CloseHandle(hSnapshot);
    return res;
}

#endif //NCBI_OS_MSWIN


bool CProcess::KillGroup(unsigned long timeout) const
{
#if   defined(NCBI_OS_UNIX)

    TPid pgid = getpgid((TPid)m_Process);
    if (pgid == (TPid)(-1)) {
        // TRUE if PID does not match any process
        return errno == ESRCH;
    }
    return KillGroupById(pgid, timeout);

#elif defined(NCBI_OS_MSWIN)

    // Convert the process handle to process ID if needed
    TPid pid = 0;
    if (m_Type == eHandle) {
        // Some OS like Windows 2000 and WindowsXP (w/o SP1) don't
        // have GetProcessId() function. Try to load it directy from 
        // KERNEL32.DLL
        static bool  s_TryGetProcessId = true;
        typedef DWORD (STDMETHODCALLTYPE FAR* LPFN_GETPROCESSID)(HANDLE process);
        static LPFN_GETPROCESSID s_GetProcessId = NULL;

        if ( s_TryGetProcessId  &&  !s_GetProcessId ) {
            s_GetProcessId  = (LPFN_GETPROCESSID)GetProcAddress(
                                    GetModuleHandleA("KERNEL32.DLL"),
                                    "GetProcessId");
            s_TryGetProcessId = false;
        }
        if ( !s_GetProcessId ) {
            // GetProcessId() is not available on this platform
            return false;
        }
        pid = s_GetProcessId((TProcessHandle)m_Process);

    } else {  // m_Type == ePid
        pid = (TPid)m_Process;
    }
    if (!pid) {
        return false;
    }
    // Use safe process termination if timeout > 0
    unsigned long x_timeout = timeout;
    CStopWatch timer;
    if ( timeout ) {
        timer.Start();
    }
    // Kill process tree
    bool result = s_KillGroup(pid, (timeout > 0) ? &timer : 0, x_timeout);
    return result;

#endif
}


bool CProcess::KillGroupById(TPid pgid, unsigned long timeout)
{
#if   defined(NCBI_OS_UNIX)

    // Try to kill the process group with SIGTERM first
    if (kill(-pgid, SIGTERM) < 0  &&  errno == EPERM) {
        CNcbiError::SetFromErrno();
        return false;
    }

    // Check process group termination within the timeout 
    unsigned long x_timeout = timeout;
    for (;;) {
        // Reap the zombie (if group leader is a child) up from the system
        TPid reap = waitpid(pgid, static_cast<int*>(NULL), WNOHANG);
        if (reap) {
            if (reap != (TPid)(-1)) {
                _ASSERT(reap == pgid);
                return true;
            }
            if (errno != ECHILD) {
                CNcbiError::SetFromErrno();
                return false;
            }
            if (kill(-pgid, 0) < 0) {
                return true;
            }
        }
        unsigned long x_sleep = kWaitPrecision;
        if (x_sleep > x_timeout) {
            x_sleep = x_timeout;
        }
        if ( !x_sleep ) {
             break;
        }
        SleepMilliSec(x_sleep);
        x_timeout  -= x_sleep;
    }
    _ASSERT(!x_timeout);

    // Try harder to kill the stubborn processes -- SIGKILL may not be caught!
    int res = kill(-pgid, SIGKILL);
    if ( !timeout ) {
        return res <= 0;
    }
    SleepMilliSec(kWaitPrecision);
    // Reap the zombie (if group leader is a child) up from the system
    waitpid(pgid, static_cast<int*>(NULL), WNOHANG);
    // Check whether the process cannot be killed
    // (most likely due to a kernel problem)
    return kill(-pgid, 0) < 0;

#elif defined(NCBI_OS_MSWIN)

    // Cannot be implemented, use non-static version of KillGroup()
    // for specified process.
    return false;

#endif
}


int CProcess::Wait(unsigned long timeout, CExitInfo* info) const
{
    int  status;

    // Reset extended information
    if (info) {
        info->state  = eExitInfo_Unknown;
        info->status = 0;
    }

#if   defined(NCBI_OS_UNIX)

    TPid pid     = (TPid)m_Process;
    int  options = timeout == kInfiniteTimeoutMs ? 0 : WNOHANG;

    // Check process termination (with timeout or indefinitely)
    for (;;) {
        TPid ws = waitpid(pid, &status, options);
        if (ws > 0) {
            // terminated
            _ASSERT(ws == pid);
            if (info) {
                info->state  = eExitInfo_Terminated;
                info->status = status;
            }
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        } else if (ws == 0) {
            // still running
            _ASSERT(timeout != kInfiniteTimeoutMs);
            if ( !timeout ) {
                if (info) {
                    info->state = eExitInfo_Alive;
                }
                break;
            }
            unsigned long x_sleep = kWaitPrecision;
            if (x_sleep > timeout) {
                x_sleep = timeout;
            }
            SleepMilliSec(x_sleep);
            timeout    -= x_sleep;
        } else if (errno != EINTR) {
            CNcbiError::SetFromErrno();
            // error
            break;
        }
    }
    return -1;

#elif defined(NCBI_OS_MSWIN)

    HANDLE hProcess;
    bool   enable_sync = true;

    // Get process handle
    if (m_Type == ePid) {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                               FALSE, (TPid)m_Process);
        if ( !hProcess ) {
            enable_sync = false;
            hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (TPid)m_Process);
            if (!hProcess   &&  GetLastError() == ERROR_ACCESS_DENIED) {
                CNcbiError::SetFromWindowsError();
                return -1;
            }
        }
    } else {
        hProcess = (TProcessHandle)m_Process;
        if (!hProcess  ||  hProcess == INVALID_HANDLE_VALUE) {
            CNcbiError::Set(CNcbiError::eBadAddress);
            return -1;
        }
    }

    status = -1;
    DWORD x_status;
    // Is process still running?
    if (GetExitCodeProcess(hProcess, &x_status)) {
        if (x_status == STILL_ACTIVE) {
            if (enable_sync  &&  timeout) {
                DWORD tv = (timeout == kInfiniteTimeoutMs
                            ? INFINITE
                            : (DWORD)timeout);
                DWORD ws = WaitForSingleObject(hProcess, tv);
                switch(ws) {
                case WAIT_TIMEOUT:
                    // still running
                    _ASSERT(x_status == STILL_ACTIVE);
                    break;
                case WAIT_OBJECT_0:
                    if (GetExitCodeProcess(hProcess, &x_status)) {
                        if (x_status != STILL_ACTIVE) {
                            // terminated
                            status = 0;
                        } // else still running
                        break;
                    }
                    /*FALLTHRU*/
                default:
                    // error
                    x_status = 0;
                    break;
                }
            } // else still running
        } else {
            // terminated
            status = 0;
        }
    } else {
        // error
        x_status = 0;
    }

    if (status < 0) {
        if (info  &&  x_status == STILL_ACTIVE) {
            info->state  = eExitInfo_Alive;
        }
    } else {
        if (info) {
            info->state  = eExitInfo_Terminated;
            info->status = x_status;
        }
        status = x_status;
    }

    if (m_Type == ePid) {
        CloseHandle(hProcess);
    }
    return status;

#endif
}



/////////////////////////////////////////////////////////////////////////////
//
// CPIDGuard
//
// NOTE: CPIDGuard know nothing about PID file modification or deletion 
//       by other processes behind this API. Be aware.


CPIDGuard::CPIDGuard(const string& filename, const string& dir)
    : m_PID(0)
{
    string real_dir;
    CDirEntry::SplitPath(filename, &real_dir, 0, 0);
    if (real_dir.empty()) {
        if (dir.empty()) {
            real_dir = CDir::GetTmpDir();
        } else {
            real_dir = dir;
        }
        m_Path = CDirEntry::MakePath(real_dir, filename);
    } else {
        m_Path = filename;
    }
    // Create guard for MT-Safe protect	
    m_MTGuard.reset(new CInterProcessLock(m_Path + ".guard"));
    // Update PID
    UpdatePID();
}


CPIDGuard::~CPIDGuard(void)
{
    Release();
    m_MTGuard.reset();
    m_PIDGuard.reset();
}


void CPIDGuard::Release(void)
{
    if ( m_Path.empty() ) {
        return;
    }
    // MT-Safe protect
    CGuard<CInterProcessLock> LOCK(*m_MTGuard);

    // Read info
    TPid pid = 0;
    unsigned int ref = 0;
    CNcbiIfstream in(m_Path.c_str());
        
    if ( in.good() ) {
        in >> pid >> ref;
        in.close();
        if ( m_PID != pid ) {
            // We do not own this file more.
            return;
        }
        if ( ref ) {
            ref--;
        }
        // Check reference counter
        if ( ref ) {
            // Write updated reference counter into the file
            CNcbiOfstream out(m_Path.c_str(),
                                IOS_BASE::out | IOS_BASE::trunc);
            if ( out.good() ) {
                out << pid << endl << ref << endl;
            }
            if ( !out.good() ) {
                NCBI_THROW(CPIDGuardException, eWrite,
                            "Unable to write into PID file " + m_Path +": "
                            + _T_CSTRING(NcbiSys_strerror(errno)));
            }
        } else {
            // Remove the file
            CDirEntry(m_Path).Remove();
            // Remove modification protect guard
            LOCK.Release();
            m_MTGuard->Remove();
            m_MTGuard.reset();
            // PID-file can be reused now
            if ( m_PIDGuard.get() ) {
                m_PIDGuard->Remove();
                m_PIDGuard.reset();
            }
        }
    }
    m_Path.erase();
}


void CPIDGuard::Remove(void)
{
    if ( m_Path.empty() ) {
        return;
    }
    // MT-Safe protect
    CGuard<CInterProcessLock> LOCK(*m_MTGuard);

    // Remove the PID file
    CDirEntry(m_Path).Remove();
    m_Path.erase();
    // Remove modification protect guard
    m_MTGuard->Remove();
    // PID-file can be reused now
    if ( m_PIDGuard.get() ) {
        m_PIDGuard->Remove();
        m_PIDGuard.reset();
    }
}


void CPIDGuard::UpdatePID(TPid pid)
{
    if (pid == 0) {
        pid = CProcess::GetCurrentPid();
    }
    // MT-Safe protect
    CGuard<CInterProcessLock> LOCK(*m_MTGuard);
  
    // Check PID from valid PID-files only. The file can be left on
    // the file system from previous session. And stored in the file
    // PID can be already reused by OS.
    
    bool valid_file = true;
    unsigned int ref = 1;
    
    // Create guard for PID file
    if ( !m_PIDGuard.get() ) {
        // first call to Update() ?
        m_PIDGuard.reset(new CInterProcessLock(m_Path + ".start.guard"));
        // If the guard lock is successfully obtained, the existent  PID file
        // can be a stale lock or a new unused file, so just reuse it.
        valid_file = !m_PIDGuard->TryLock();
    }

    if ( valid_file ) {
        // Read old PID
        CNcbiIfstream in(m_Path.c_str());
        if ( in.good() ) {
            TPid old_pid;
            in >> old_pid >> ref;
            if ( old_pid == pid ) {
                // Guard the same PID. Just increase the reference counter.
                ref++;
            } else {
                if ( CProcess(old_pid,CProcess::ePid).IsAlive() ) {
                    NCBI_THROW2(CPIDGuardException, eStillRunning,
                                "Process is still running", old_pid);
                }
                ref = 1;
            }
        }
        in.close();
    }

    // Write new PID
    CNcbiOfstream out(m_Path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( out.good() ) {
        out << pid << endl << ref << endl;
    }
    if ( !out.good() ) {
        NCBI_THROW(CPIDGuardException, eWrite,
                   "Unable to write into PID file " + m_Path + ": "
                   + _T_CSTRING(NcbiSys_strerror(errno)));
    }
    // Save updated pid
    m_PID = pid;
}


const char* CPIDGuardException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eStillRunning: return "eStillRunning";
    case eWrite:        return "eWrite";
    default:            return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
