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

#if defined(NCBI_OS_UNIX)
#  include <errno.h>
#  include <fcntl.h>
#  include <signal.h>
#  include <stdio.h>
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/time.h>            // getrusage(), getrlimit()
#  include <sys/resource.h>        // getrusage(), getrlimit()
#  include <sys/times.h>           // times()
#  include <sys/wait.h>
#  if defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_DARWIN)
#    include <sys/sysctl.h>
#  endif //NCBI_OS_BSD || NCBI_OS_DARWIN
#  if defined(NCBI_OS_IRIX)
#    include <sys/sysmp.h>
#  endif //NCBI_OS_IRIX
#  include "ncbi_os_unix_p.hpp"

#elif defined(NCBI_OS_MSWIN)
#  include <corelib/ncbitime.hpp>  // CStopWatch
#  include <corelib/ncbidll.hpp>   // CDll
#  include <process.h>
#  include "ncbi_os_mswin_p.hpp"
#  pragma warning (disable : 4191)
#endif

#ifdef NCBI_OS_DARWIN
extern "C" {
#  include <mach/mach.h>
} /* extern "C" */
#endif //NCBI_OS_DARWIN


#define NCBI_USE_ERRCODE_X   Corelib_Process


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// CProcessBase -- constants initialization
//

const unsigned long CProcessBase::kDefaultKillTimeout = 1000;
const unsigned long CProcessBase::kInfiniteTimeoutMs  = kMax_ULong;
const unsigned long               kWaitPrecisionMs    = 100;



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
// CCurrentProcess 
//

#ifdef NCBI_THREAD_PID_WORKAROUND
#  ifndef NCBI_OS_UNIX
#    error "NCBI_THREAD_PID_WORKAROUND should only be defined on UNIX!"
#  endif
TPid CCurrentProcess::sx_GetPid(EGetPidFlag flag)
{
    if ( flag == ePID_GetThread ) {
        // Return real PID, do not cache it.
        return getpid();
    }

    DEFINE_STATIC_FAST_MUTEX(s_GetPidMutex);
    static TPid s_CurrentPid = 0;
    static TPid s_ParentPid = 0;

    if (CThread::IsMain()) {
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


TProcessHandle CCurrentProcess::GetHandle(void)
{
#if defined(NCBI_OS_MSWIN)
    return ::GetCurrentProcess();
#elif defined(NCBI_OS_UNIX)
    return GetPid();
#endif
}


TPid CCurrentProcess::GetPid(void)
{
#if defined(NCBI_OS_MSWIN)
    return ::GetCurrentProcessId();
#elif defined NCBI_THREAD_PID_WORKAROUND
    return sx_GetPid(ePID_GetCurrent);
#elif defined(NCBI_OS_UNIX)
    return getpid();
#endif
}


TPid CCurrentProcess::GetParentPid(void)
{
#if defined(NCBI_OS_MSWIN)
    PROCESSENTRY32 entry;
    if (CWinFeature::FindProcessEntry(::GetCurrentProcessId(), entry)) {
        return entry.th32ParentProcessID;
    }
    CNcbiError::SetFromWindowsError();
    return 0;

#elif defined NCBI_THREAD_PID_WORKAROUND
    return sx_GetPid(ePID_GetParent);

#elif defined(NCBI_OS_UNIX)
    return getppid();
#endif
}



/////////////////////////////////////////////////////////////////////////////
//
// CCurrentProcess - Forks & Daemons
//

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
    return (error != NULL && *error != '\0') ? 
                string(error) :
                ("errno=" + NStr::NumericToString(x_errno.GetErrno()));
}
#endif

TPid CCurrentProcess::Fork(CProcess::TForkFlags flags)
{
#ifdef NCBI_OS_UNIX
    TPid pid = ::fork();
    if (pid == 0)
        // Only update PID and UID in the child process.
        CDiagContext::UpdateOnFork((flags & fFF_UpdateDiag) != 0 ?
                                   CDiagContext::fOnFork_ResetTimer | CDiagContext::fOnFork_PrintStart : 0);
    else if (pid == (TPid)(-1) && (flags & fFF_AllowExceptions) != 0) {
        NCBI_THROW_FMT(CCoreException, eCore,
                       "CCurrentProcess::Fork(): Cannot fork: " << s_ErrnoToString());
    }
    return pid;
#else
    NCBI_THROW(CCoreException, eCore,
               "CCurrentProcess::Fork() not implemented on this platform");
#endif
}

#ifdef NCBI_OS_UNIX
namespace {
    class CSafeRedirect
    {
    public:
        CSafeRedirect(int fd, bool* success_flag) :
            m_OrigFD(fd), m_SuccessFlag(success_flag), m_Redirected(false)
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
            if (m_Redirected && !*m_SuccessFlag) {
                // Restore the original std I/O stream descriptor.
                ::dup2(m_DupFD, m_OrigFD);
            }
            ::close(m_DupFD);
        }

    private:
        int   m_OrigFD;
        int   m_DupFD;
        bool* m_SuccessFlag;
        bool  m_Redirected;
    };
}

TPid s_Daemonize(const char* logfile, CCurrentProcess::TDaemonFlags flags)
{
    if (!(flags & CCurrentProcess::fDF_AllowThreads)) {
        if (unsigned n = CThread::GetThreadsCount()) {
            NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                           "Prohibited, there are already child threads running: " << n);
        }
    }
    bool success_flag = false;

    CSafeRedirect stdin_redirector (STDIN_FILENO,  &success_flag);
    CSafeRedirect stdout_redirector(STDOUT_FILENO, &success_flag);
    CSafeRedirect stderr_redirector(STDERR_FILENO, &success_flag);

    int new_fd;

    if (flags & CCurrentProcess::fDF_KeepStdin) {
        if ((new_fd = ::open("/dev/null", O_RDONLY)) < 0) {
            NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                           "Error opening /dev/null for reading: " << s_ErrnoToString());
        }
        stdin_redirector.Redirect(new_fd);
    }
    if (flags & CCurrentProcess::fDF_KeepStdout) {
        if ((new_fd = ::open("/dev/null", O_WRONLY)) < 0) {
            NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                           "Error opening /dev/null for writing: " << s_ErrnoToString());
        }
        NcbiCout.flush();
        ::fflush(stdout);
        stdout_redirector.Redirect(new_fd);
    }
    if (logfile) {
        if (!*logfile) {
            if ((new_fd = ::open("/dev/null", O_WRONLY | O_APPEND)) < 0) {
                NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                               "Error opening /dev/null for appending: " << s_ErrnoToString());
            }
        } else {
            if ((new_fd = ::open(logfile, O_WRONLY | O_APPEND | O_CREAT, 0666)) < 0) {
                NCBI_THROW_FMT(CCoreException, eCore, "[Daemonize] "
                               "Unable to open logfile \"" <<  logfile << "\": " << s_ErrnoToString());
            }
        }
        NcbiCerr.flush();
        ::fflush(stderr);
        stderr_redirector.Redirect(new_fd);
    }
    ::fflush(NULL);
    TPid pid = CCurrentProcess::Fork(CCurrentProcess::fFF_UpdateDiag | 
                                     CCurrentProcess::fFF_AllowExceptions);
    if (pid) {
        // Parent process.
        // No need to set success_flag to true here, because
        // either this process must be terminated or
        // the descriptors must be restored.
        if ((flags & CCurrentProcess::fDF_KeepParent) == 0) {
            GetDiagContext().PrintStop();
            ::_exit(0);
        }
        return pid; /*success*/
    }
    // Child process.
    success_flag = true;
    ::setsid();
    if (flags & CCurrentProcess::fDF_ImmuneTTY) {
        try {
            if (CCurrentProcess::Fork() != 0) {
                ::_exit(0); // Exit the second parent process
            }
        }
        catch (CCoreException& e) {
            ERR_POST_X(2, "[Daemonize] Failed to immune from "
                          "TTY accruals: " << e << " ... continuing anyways");
        }
    }
    if (!(flags & CCurrentProcess::fDF_KeepCWD))
        if (::chdir("/") ) { /*no-op*/ };  // "/" always exists
    if (!(flags & CCurrentProcess::fDF_KeepStdin))
        ::fclose(stdin);
    else
        ::fflush(stdin);  // POSIX requires this
    if (!(flags & CCurrentProcess::fDF_KeepStdout))
        ::fclose(stdout);
    if (!logfile)
        ::fclose(stderr);
    return (TPid)(-1); /*success*/
}
#endif /* NCBI_OS_UNIX */


TPid CCurrentProcess::Daemonize(const char* logfile, CCurrentProcess::TDaemonFlags flags)
{
#ifdef NCBI_OS_UNIX
    if (flags & CCurrentProcess::fDF_AllowExceptions)
        return s_Daemonize(logfile, flags);
    else
        try {
            return s_Daemonize(logfile, flags);
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
               "CCurrentProcess::Daemonize() not implemented on this platform");
    /*NOTREACHED*/
#endif
    return (TPid) 0; /*failure*/
}


/////////////////////////////////////////////////////////////////////////////
//
// CProcess 
//

CProcess::CProcess(void) 
    : m_Process(CCurrentProcess::GetPid()), 
      m_Type(ePid), 
      m_IsCurrent(eTriState_True)
{
    return;
}


CProcess::CProcess(TPid process, EType type)
    : m_Process(process),
      m_Type(type),
      m_IsCurrent(eTriState_Unknown)
{
    // We don't try to determine that passed "process" represent
    // the current process here, probably this information will not be needed.
    // Otherwise, IsCurrent() will take care about this.
    return;
}

#ifdef NCBI_OS_MSWIN

// The helper constructor for MS Windows to avoid cast from
// TProcessHandle to TPid
CProcess::CProcess(TProcessHandle process, EType type)
    : m_Process((intptr_t)process), 
      m_Type(type), 
      m_IsCurrent(eTriState_Unknown)
{
    return;
}

TProcessHandle CProcess::x_GetHandle(DWORD desired_access, DWORD* errcode) const
{
    TProcessHandle hProcess = NULL;
    if (GetType() == eHandle) {
        hProcess = GetHandle();
        if (!hProcess  ||  hProcess == INVALID_HANDLE_VALUE) {
            CNcbiError::Set(CNcbiError::eBadAddress);
            return NULL;
        }
    } else {
        hProcess = ::OpenProcess(desired_access, FALSE, GetPid());
        if (!hProcess) {
            if (errcode) {
                *errcode = ::GetLastError();
                CNcbiError::SetWindowsError(*errcode);
            } else {
                CNcbiError::SetFromWindowsError();
            }
        }
    }
    return hProcess;
}

void CProcess::x_CloseHandle(TProcessHandle handle) const
{
    if (GetType() == ePid) {
        ::CloseHandle(handle);
    }
}

TPid CProcess::x_GetPid(void) const
{
    if (GetType() == eHandle) {
        return ::GetProcessId(GetHandle());
    }
    return GetPid();
}

#endif //NCBI_OS_MSWIN


bool CProcess::IsCurrent(void)
{
    if (m_IsCurrent == eTriState_True) {
        return true;
    }
    bool current = false;
    if (GetType() == ePid  &&  GetPid() == CCurrentProcess::GetPid()) {
        current = true;
    }
#if defined(NCBI_OS_MSWIN)
    else 
    if (GetType() == eHandle  &&  GetHandle() == CCurrentProcess::GetHandle()) {
        current = true;
    }
#endif          
    m_IsCurrent = current ? eTriState_True : eTriState_False;
    return current;
}


bool CProcess::IsAlive(void) const
{
#if defined(NCBI_OS_UNIX)
    return kill(GetPid(), 0) == 0  ||  errno == EPERM;

#elif defined(NCBI_OS_MSWIN)
    DWORD status;
    HANDLE hProcess = x_GetHandle(PROCESS_QUERY_INFORMATION, &status);
    if (!hProcess) {
        return status == ERROR_ACCESS_DENIED;
    }
    _ASSERT(STILL_ACTIVE != 0);
    ::GetExitCodeProcess(hProcess, &status);
    x_CloseHandle(hProcess);
    return status == STILL_ACTIVE;
#endif
}


bool CProcess::Kill(unsigned long timeout)
{
#if defined(NCBI_OS_UNIX)

    TPid pid = GetPid();

    // Try to kill the process with SIGTERM first
    if (kill(pid, SIGTERM) < 0  &&  errno == EPERM) {
        CNcbiError::SetFromErrno();
        return false;
    }

    // Check process termination within the timeout
    unsigned long x_timeout = timeout;
    for (;;) {
        TPid reap = waitpid(pid, 0, WNOHANG);
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
        unsigned long x_sleep = kWaitPrecisionMs;
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
    SleepMilliSec(kWaitPrecisionMs);
    // Reap the zombie (if child) up from the system
    waitpid(pid, 0, WNOHANG);

    // Check whether the process cannot be killed
    // (most likely due to a kernel problem)
    return kill(pid, 0) < 0;

#elif defined(NCBI_OS_MSWIN)

    // Safe process termination
    bool safe = (timeout > 0);

    // Try to kill current process?
    if ( IsCurrent() ) {
        // Just exit
        ::ExitProcess(-1);
        // NOTREACHED
        return false;
    }

    HANDLE hProcess    = NULL;
    HANDLE hThread     = NULL;
    bool   allow_wait  = true;

    // Get process handle

    hProcess = x_GetHandle(PROCESS_CREATE_THREAD | PROCESS_TERMINATE | SYNCHRONIZE);
    if (!hProcess) {
        if (GetType() != ePid) {
            return false;
        }
        // SYNCHRONIZE access right is required to wait for the process to terminate
        allow_wait = false;
        // Try to open with minimal access right needed to terminate process.
        DWORD err;
        hProcess = x_GetHandle(PROCESS_TERMINATE, &err);
        if (!hProcess) {
            if (err != ERROR_ACCESS_DENIED) {
                return false;
            }
            // If we have an administrative rights, that we can try
            // to terminate the process using SE_DEBUG_NAME privilege,
            // which system administrators normally have, but it might
            // be disabled by default. When this privilege is enabled,
            // the calling thread can open processes with any access
            // rights regardless of the security descriptor assigned
            // to the process.

            // TODO: Use CWinSecurity to adjust privileges

            // Get current thread token 
            HANDLE hToken;
            if (!::OpenThreadToken(::GetCurrentThread(),
                                    TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, FALSE, &hToken)) {
                err = ::GetLastError();
                if (err != ERROR_NO_TOKEN) {
                    CNcbiError::SetWindowsError(err);
                    return false;
                }
                // Revert to the process token, if not impersonating
                if (!::OpenProcessToken(::GetCurrentProcess(),
                                        TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken)) {
                    CNcbiError::SetFromWindowsError();
                    return false;
                }
            }

            // Try to enable the SE_DEBUG_NAME privilege

            TOKEN_PRIVILEGES tp, tp_prev;
            DWORD            tp_prev_size = sizeof(tp_prev);

            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            ::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);

            if (!::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), &tp_prev, &tp_prev_size)) {
                CNcbiError::SetFromWindowsError();
                ::CloseHandle(hToken);
                return false;
            }
            // To determine that successful call to the AdjustTokenPrivileges()
            // adjusted all of the specified privileges, check GetLastError():
            err = ::GetLastError();
            if (err == ERROR_NOT_ALL_ASSIGNED) {
                // The AdjustTokenPrivileges function cannot add new
                // privileges to the access token. It can only enable or
                // disable the token's existing privileges.
                CNcbiError::SetWindowsError(err);
                ::CloseHandle(hToken);
                return false;
            }

            // Try to open process handle again
            hProcess = ::OpenProcess(PROCESS_TERMINATE, FALSE, GetPid());
                
            // Restore original privilege state
            ::AdjustTokenPrivileges(hToken, FALSE, &tp_prev, sizeof(tp_prev), NULL, NULL);
            ::CloseHandle(hToken);
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
    if ( safe  &&  allow_wait ) {
        // kernel32.dll loaded at same address in each process,
        // so call ::ExitProcess() there.
        FARPROC exitproc = ::GetProcAddress(::GetModuleHandleA("KERNEL32.DLL"), "ExitProcess");
        if ( exitproc ) {
            hThread = ::CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)exitproc, 0, 0, 0);
            // Wait until process terminated, or timeout expired
            if (hThread   &&
                (::WaitForSingleObject(hProcess, timeout) == WAIT_OBJECT_0)){
                terminated = true;
            }
        }
    }
    // Try harder to kill stubborn process
    if ( !terminated ) {
        if ( ::TerminateProcess(hProcess, -1) != 0 ) {
            DWORD err = ::GetLastError();
            if (err == ERROR_INVALID_HANDLE) {
                // If process "terminated" successfully or error occur but
                // process handle became invalid -- process has terminated
                terminated = true;
            }
            else {
                CNcbiError::SetWindowsError(err);
            }
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
            unsigned long x_sleep = kWaitPrecisionMs;
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
        ::CloseHandle(hThread);
    }
    x_CloseHandle(hProcess);
    return terminated;

#endif
}


#ifdef NCBI_OS_MSWIN

// MS Windows:
// A helper function for terminating all processes
// in the tree within specified timeout.
// If 'timer' is specified, use safe process termination.

static bool s_Win_KillGroup(DWORD pid, CStopWatch *timer, unsigned long &timeout)
{
    // Open snapshot handle.
    // We cannot use one snapshot for recursive calls, 
    // because it is not reentrant.
    HANDLE const snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    // Terminate all children first
    if (!::Process32First(snapshot, &entry)) {
        ::CloseHandle(snapshot);
        return false;
    }
    do {
        if (entry.th32ParentProcessID == pid) {
            // Safe termination -- update timeout
            if ( timer ) {
                double elapsed = timer->Elapsed() * kMilliSecondsPerSecond;
                timeout = (elapsed < timeout) ? (unsigned long)((double)timeout - elapsed) : 0;
                if ( !timeout ) {
                    ::CloseHandle(snapshot);
                    return false;
                }
            }
            bool res = s_Win_KillGroup(entry.th32ProcessID, timer, timeout);
            if ( !res ) {
                ::CloseHandle(snapshot);
                return false;
            }
        }
    }
    while (::Process32Next(snapshot, &entry)); 

    // Terminate the specified process

    // Safe termination -- update timeout
    if ( timer ) {
        double elapsed = timer->Elapsed() * kMilliSecondsPerSecond;
        timeout = (elapsed < timeout) ? (unsigned long)((double)timeout - elapsed) : 0;
        if ( !timeout ) {
            ::CloseHandle(snapshot);
            return false;
        }
    }
    bool res = CProcess(pid, CProcess::ePid).Kill(timeout);

    // Close snapshot handle
    ::CloseHandle(snapshot);
    return res;
}

#endif //NCBI_OS_MSWIN


bool CProcess::KillGroup(unsigned long timeout) const
{
#if defined(NCBI_OS_UNIX)

    TPid pgid = getpgid(GetPid());
    if (pgid == (TPid)(-1)) {
         CNcbiError::SetFromErrno();
        // TRUE if PID does not match any process
        return errno == ESRCH;
    }
    return KillGroupById(pgid, timeout);

#elif defined(NCBI_OS_MSWIN)

    TPid pid = x_GetPid();
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
    bool result = s_Win_KillGroup(pid, (timeout > 0) ? &timer : 0, x_timeout);
    return result;

#endif
}


bool CProcess::KillGroupById(TPid pgid, unsigned long timeout)
{
#if defined(NCBI_OS_UNIX)

    // Try to kill the process group with SIGTERM first
    if (kill(-pgid, SIGTERM) < 0  &&  errno == EPERM) {
        CNcbiError::SetFromErrno();
        return false;
    }

    // Check process group termination within the timeout 
    unsigned long x_timeout = timeout;
    for (;;) {
        // Reap the zombie (if group leader is a child) up from the system
        TPid reap = waitpid(pgid, 0, WNOHANG);
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
        unsigned long x_sleep = kWaitPrecisionMs;
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
    SleepMilliSec(kWaitPrecisionMs);
    // Reap the zombie (if group leader is a child) up from the system
    waitpid(pgid, 0, WNOHANG);
    // Check whether the process cannot be killed
    // (most likely due to a kernel problem)
    return kill(-pgid, 0) < 0;

#else
    // Cannot be implemented, use non-static version of KillGroup()
    // for specified process.
    CNcbiError::Set(CNcbiError::eNotSupported);
    return false;

#endif
}


int CProcess::Wait(unsigned long timeout, CExitInfo* info) const
{
    int status;

    // Reset extended information
    if (info) {
        info->state  = eExitInfo_Unknown;
        info->status = 0;
    }

#if defined(NCBI_OS_UNIX)

    TPid pid     = GetPid();
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
            unsigned long x_sleep = kWaitPrecisionMs;
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

    bool allow_wait = true;

    // Get process handle
    HANDLE hProcess = x_GetHandle(PROCESS_QUERY_INFORMATION | SYNCHRONIZE);
    if (!hProcess) {
        // SYNCHRONIZE access right is required to wait for the process to terminate
        allow_wait = false;
        // Try to open with minimal access right needed to terminate process.
        hProcess = x_GetHandle(PROCESS_TERMINATE);
        if (!hProcess) {
            return -1;
        }
    }
    status = -1;
    DWORD x_status;
    // Is process still running?
    if (::GetExitCodeProcess(hProcess, &x_status)) {
        if (x_status == STILL_ACTIVE) {
            if (allow_wait  &&  timeout) {
                DWORD tv = (timeout == kInfiniteTimeoutMs ? INFINITE : (DWORD)timeout);
                DWORD ws = ::WaitForSingleObject(hProcess, tv);
                switch(ws) {
                case WAIT_TIMEOUT:
                    // still running
                    _ASSERT(x_status == STILL_ACTIVE);
                    break;
                case WAIT_OBJECT_0:
                    if (::GetExitCodeProcess(hProcess, &x_status)) {
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
    x_CloseHandle(hProcess);

    return status;

#endif
}


#if defined(NCBI_OS_MSWIN)
bool s_Win_GetHandleTimes(HANDLE handle, double* real, double* user, double* sys, CProcess::EWhat what)
{
    // Each FILETIME structure contains the number of 100-nanosecond time units
    FILETIME ft_creation, ft_exit, ft_kernel, ft_user;
    BOOL res = FALSE;

    if (what == CProcess::eProcess) {
        res = ::GetProcessTimes(handle, &ft_creation, &ft_exit, &ft_kernel, &ft_user);
    } else
    if (what == CProcess::eThread) {
        res = ::GetThreadTimes(handle, &ft_creation, &ft_exit, &ft_kernel, &ft_user);
    }
    if (!res) {
        CNcbiError::SetFromWindowsError();
        return false;
    }
    if ( real ) {
        // Calculate real execution time using handle creation time.
        // clock() is less accurate on Windows, so use GetSystemTime*().
        // <real time> = <system time> - <creation time>
       
        FILETIME ft_sys;
        ::GetSystemTimeAsFileTime(&ft_sys);
        // Calculate a difference between FILETIME variables
        ULARGE_INTEGER i;
        __int64 v2, v1, v_diff;
        i.LowPart  = ft_creation.dwLowDateTime;
        i.HighPart = ft_creation.dwHighDateTime;
        v1 = i.QuadPart;
        i.LowPart  = ft_sys.dwLowDateTime;
        i.HighPart = ft_sys.dwHighDateTime;
        v2 = i.QuadPart;
        v_diff = v2 - v1;
        i.QuadPart = v_diff;
        *real = (i.LowPart + ((Uint8)i.HighPart << 32)) * 1.0e-7;
    }
    if ( sys ) {
        *sys = (ft_kernel.dwLowDateTime + ((Uint8) ft_kernel.dwHighDateTime << 32)) * 1e-7;
    }
    if ( user ) {
        *user = (ft_user.dwLowDateTime + ((Uint8) ft_user.dwHighDateTime << 32)) * 1e-7;
    }
    return true;
}
#endif


#if defined(NCBI_OS_LINUX)
bool s_Linux_GetTimes_ProcStat(TPid pid, double* real, double* user, double* sys, CProcess::EWhat what)
{
    // No need to set errno in this function, it have a backup that overwrite it
    if (what == CProcess::eThread) {
        return false;
    }
    clock_t tick = CSystemInfo::GetClockTicksPerSecond();
    if ( !tick ) {
        return false;
    }

    // Execution times can be extracted from /proc/<pid>/stat,
    CLinuxFeature::CProcStat ps(pid);

    // Fields numbers in that file, for process itself and children.
    // Note, all values can be zero.
    int fu = 14, fs = 15;
    if (what == CProcess::eChildren) {
        fu = 16;
        fs = 17;
    }
    if ( real  &&  (what == CProcess::eProcess) ) {
        // Real execution time can be calculated for the process only.
        // field 22, in ticks per second.
        Uint8 start = NStr::StringToUInt8(ps.at(22), NStr::fConvErr_NoThrow);
        double uptime = CSystemInfo::GetUptime();
        if (start > 0  &&  uptime > 0) {
            *real = uptime - (double)start / (double)tick;
        }
    }
    if ( user ) {
        *user = (double)NStr::StringToUInt8(ps.at(fu), NStr::fConvErr_NoThrow) / (double)tick;
    }
    if ( sys ) {
        *sys  = (double)NStr::StringToUInt8(ps.at(fs), NStr::fConvErr_NoThrow) / (double)tick;
    }
    return true;

}
#endif


bool CProcess::GetTimes(double* real, double* user, double* sys, CProcess::EWhat what)
{
    // Use CCurrentProcess::GetProcessTimes() for the current process,
    // it can give better results than "generic" version.
    if ( IsCurrent() ) {
        return CCurrentProcess::GetTimes(real, user, sys, what);
    }

    const double kUndefined = -1.0;
    if ( real ) { *real = kUndefined; }
    if ( user ) { *user = kUndefined; }
    if ( sys )  { *sys  = kUndefined; }

    if (what == eThread) {
        // We don't have information about thread times for some arbitrary process.
        CNcbiError::Set(CNcbiError::eNotSupported);
        return false;
    }

#if defined(NCBI_OS_MSWIN)

    if (what == eChildren) {
        // On Windows we have information for the process only.
        // TODO: Try to implemented this for children.
        CNcbiError::Set(CNcbiError::eNotSupported);
        return false;
    }
    // Get process handle
    HANDLE hProcess = x_GetHandle(PROCESS_QUERY_INFORMATION | SYNCHRONIZE);
    if (!hProcess) {
        return false;
    }
    bool res = s_Win_GetHandleTimes(hProcess, real, user, sys, CProcess::eProcess);
    x_CloseHandle(hProcess);
    return res;

#elif defined(NCBI_OS_UNIX)

#  if defined(NCBI_OS_LINUX)
    return s_Linux_GetTimes_ProcStat(GetPid(), real, user, sys, what);
#  else
    // TODO: Investigate how to determine this on other Unix flavors
#  endif //NCBI_OS_LINUX

#endif //NCBI_OS_UNIX

    CNcbiError::Set(CNcbiError::eNotSupported);
    return false;
}


bool CCurrentProcess::GetTimes(double* real, double* user, double* sys, EWhat what)
{
    const double kUndefined = -1.0;
    if ( real ) { *real = kUndefined; }
    if ( user ) { *user = kUndefined; }
    if ( sys )  { *sys  = kUndefined; }

#if defined(NCBI_OS_MSWIN)

    if (what == eChildren) {
        // We have information for the process/thread only.
        // TODO: Try to implemented this for children.
        CNcbiError::Set(CNcbiError::eNotSupported);
        return false;
    }
    bool res;
    if (what == eProcess) {
        res = s_Win_GetHandleTimes(::GetCurrentProcess(), real, user, sys, CProcess::eProcess);
    } else {
        res = s_Win_GetHandleTimes(::GetCurrentThread(), real, user, sys, CProcess::eThread);
    }
    return res;

#elif defined(NCBI_OS_UNIX)

    // UNIX: Real execution time can be calculated on Linux and for the process only.
    // TODO: Investigate how to determine this on other Unix flavors

#  if defined(NCBI_OS_LINUX)
    // Try Linux specific first, before going to more generic methods
    // For process and cildren only.
    if (what != eThread) {
        if ( s_Linux_GetTimes_ProcStat(0 /*current pid*/, real, user, sys, what) ) {
            return true;
        }
    }
#  endif
#  if defined(HAVE_GETRUSAGE)
    int who = RUSAGE_SELF;
    switch ( what ) {
    case eProcess:
        // who = RUSAGE_SELF
        break;
    case eChildren:
        who = RUSAGE_CHILDREN;
        break;
    case eThread:
        #ifdef RUSAGE_THREAD
            who = RUSAGE_THREAD;
            break;
        #else
            CNcbiError::Set(CNcbiError::eNotSupported);
            return false;
        #endif
    default:
        _TROUBLE;
    }
    struct rusage ru;
    memset(&ru, '\0', sizeof(ru));
    if (getrusage(who, &ru) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
    if ( user ) {
        *user = double(ru.ru_utime.tv_sec) + double(ru.ru_utime.tv_usec) / 1e6;
    }
    if ( sys ) {
        *sys = double(ru.ru_stime.tv_sec) + double(ru.ru_stime.tv_usec) / 1e6;
    }
    return true;

#  else 
    // Backup to times()
    if (what != eProcess) {
        // We have information for the current process only
        CNcbiError::Set(CNcbiError::eNotSupported);
        return false;
    }
    tms buf;
    clock_t t = times(&buf);
    if ( t == (clock_t)(-1) ) {
        CNcbiError::SetFromErrno();
        return false;
    }
    clock_t tick = CSystemInfo::GetClockTicksPerSecond();
    if ( !tick ) {
        // GetClockTicksPerSecond() already set CNcbiError
        return false;
    }
    if ( user ) {
        *user = (double)buf.tms_utime / (double)tick;
    }
    if ( sys ) {
        *sys = (double)buf.tms_stime / (double)tick;
    }
    return true;
    
#  endif //HAVE_GETRUSAGE
#endif //NCBI_OS_UNIX

    CNcbiError::Set(CNcbiError::eNotSupported);
    return false;
}


#if defined(NCBI_OS_MSWIN)

// PSAPI structure PROCESS_MEMORY_COUNTERS
struct SProcessMemoryCounters {
    DWORD  cb;
    DWORD  PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage; // same as PrivateUsage in PROCESS_MEMORY_COUNTERS_EX
    SIZE_T PeakPagefileUsage;
};

bool s_Win_GetMemoryCounters(HANDLE handle, SProcessMemoryCounters& memcounters)
{
    try {
        // Use GetProcessMemoryInfo() from psapi.dll.
        // Headers/libs to use this API directly may be missed, so we don't use it, but DLL exists usually.
        CDll psapi_dll("psapi.dll", CDll::eLoadNow, CDll::eAutoUnload);
        BOOL (STDMETHODCALLTYPE FAR * dllGetProcessMemoryInfo) 
             (HANDLE process, SProcessMemoryCounters& counters, DWORD size) = 0;
        dllGetProcessMemoryInfo = psapi_dll.GetEntryPoint_Func("GetProcessMemoryInfo", &dllGetProcessMemoryInfo);
        if (dllGetProcessMemoryInfo) {
            if ( !dllGetProcessMemoryInfo(handle, memcounters, sizeof(memcounters)) ) {
                CNcbiError::SetFromWindowsError();
                return false;
            }
            return true;
        }
    } catch (CException) {
        // Just catch all exceptions from CDll
    }
    // Cannot get entry point for GetProcessMemoryInfo()
    CNcbiError::Set(CNcbiError::eAddressNotAvailable);
    return false;
}

// On Windows we count the "working set" only.
// The "working set" of a process is the set of memory pages currently visible
// to the process in physical RAM memory. These pages are resident and available
// for an application to use without triggering a page fault. 
//
// https://docs.microsoft.com/en-us/windows/desktop/api/psapi/ns-psapi-_process_memory_counters
// https://docs.microsoft.com/en-us/windows/desktop/Memory/working-set

bool s_Win_GetMemoryUsage(HANDLE handle, CProcess::SMemoryUsage& usage)
{
    SProcessMemoryCounters mc;
    if (!s_Win_GetMemoryCounters(handle, mc)) {
        return false;
    }
    usage.total         = mc.WorkingSetSize + mc.PagefileUsage;
    usage.total_peak    = mc.PeakWorkingSetSize + mc.PeakPagefileUsage;
    usage.resident      = mc.WorkingSetSize;
    usage.resident_peak = mc.PeakWorkingSetSize;
    usage.swap          = mc.PagefileUsage;
    return true;
}
#endif


bool CProcess::GetMemoryUsage(SMemoryUsage& usage)
{
    // Use CCurrentProcess::GetMemoryUsage() for the current process,
    // it can give better results than "generic" version.
    if ( IsCurrent() ) {
        return CCurrentProcess::GetMemoryUsage(usage);
    }
    memset(&usage, 0, sizeof(usage));

#if defined(NCBI_OS_MSWIN)
    // Get process handle
    HANDLE hProcess = x_GetHandle(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
    if (!hProcess) {
        return false;
    }
    bool res = s_Win_GetMemoryUsage(hProcess, usage);
    x_CloseHandle(hProcess);
    return res;

#elif defined(NCBI_OS_LINUX)
    return CLinuxFeature::GetMemoryUsage(GetPid(), usage);
    
#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return false;
}


bool CCurrentProcess::GetMemoryUsage(SMemoryUsage& usage)
{
    memset(&usage, 0, sizeof(usage));

#if defined(NCBI_OS_MSWIN)
    return s_Win_GetMemoryUsage(::GetCurrentProcess(), usage);

#elif defined(NCBI_OS_LINUX)
    // Many getrusage() memory related info fields are unmaintained on Linux,
    // so read it directly from procfs.
    return CLinuxFeature::GetMemoryUsage(0 /*current pid*/, usage);
    
#elif defined(NCBI_OS_SOLARIS)
    Int8 len = CFile("/proc/self/as").GetLength();
    if (len > 0) {
        usage.total    = (size_t) len;
        usage.resident = (size_t) len; // conservative estimate
        return true;
    }
    CNcbiError::Set(CNcbiError::eUnknown);

#elif defined(HAVE_GETRUSAGE)
    //
    // BIG FAT NOTE !!!:
    //
    // We will use peak RSS value only (ru_maxrss) here. And even it can
    // use different size units depends on OS. Seems it is in KB usually,
    // but on Darwin it measured in bytes. All other memory related fields
    // of rusage structure are an "integral" values, measured in 
    // "kilobytes * ticks-of-execution", whatever that means. Seems no one
    // know how to use this. Probably it is good for measure relative memory
    // usage, measure what happens between 2 getrusage() calls, but it is
    // useless to get "current" memory usage. Most platforms don't maintain
    // these values at all and all fields are set 0, other like BSD, shows
    // inconsistent results in tests, and it is impossible to make a relation
    // between its values and real memory usage.
    //
    // Interesting related article to read:
    //     https://www.perlmonks.org/?node_id=626693

    #if defined(NCBI_OS_LINUX_)
        // NOTE: NCBI_OS_LINUX should go first to get more reliable results on Linux
        // We process Linux separately, so ignore it here.
        // JFY, on Linux:
        //    ru_maxrss - are in kilobytes
        //    other - unknown, values are unmaintained and docs says nothing about unit size.
        _TROUBLE;
    #endif
    
    struct rusage ru;
    memset(&ru, '\0', sizeof(ru));
    if (getrusage(RUSAGE_SELF, &ru) == 0) {

        #if defined(NCBI_OS_DARWIN)
            // ru_maxrss - in bytes
            usage.total_peak    = ru.ru_maxrss;
            usage.resident_peak = ru.ru_maxrss;
        #else
            // Use BSD rules, ru_maxrss - in kilobytes
            usage.resident_peak = ru.ru_maxrss * 1024;
            usage.total_peak    = ru.ru_maxrss * 1024;
        #endif
        #if !defined(NCBI_OS_DARWIN)
            // We have workaround on Darwin, see below.
            // For other -- exist 
           return true;
        #endif

        #if 0 /* disabled for now, probably need to revisit later */
            // calculates "kilobytes * ticks-of-execution"
            #define KBT(v) ((v) * 1024 / ticks)
            usage.total = KBT(ru.ru_ixrss + ru.ru_idrss  + ru.ru_isrss);
            if (usage.total > 0) {
                // If total is 0, all other walues doesn't matter.
                usage.total_peak    = KBT(ru.ru_ixrss + ru.ru_isrss) + maxrss;
                usage.resident      = KBT(ru.ru_idrss);
                usage.resident_peak = maxrss;
                usage.shared        = KBT(ru.ru_ixrss);
                usage.data          = KBT(ru.ru_idrss);
                usage.stack         = KBT(ru.ru_isrss);
            } else {
                // Use peak values only
                usage.total_peak    = maxrss;
                usage.resident_peak = maxrss;
            }
            #undef KBT
            #if defined(NCBI_OS_DARWIN)
                if (usage.total > 0) {
                    return true;
                }
            #else
                return true;
            #endif
        #endif // 0
    } // getrusage
    
    #if defined(NCBI_OS_DARWIN)
        // Alternative workaround for DARWIN if no getrusage()
        #ifdef MACH_TASK_BASIC_INFO
            task_flavor_t               flavor       = MACH_TASK_BASIC_INFO;
            struct mach_task_basic_info t_info;
            mach_msg_type_number_t      t_info_count = MACH_TASK_BASIC_INFO_COUNT;
        #else
            task_flavor_t               flavor       = TASK_BASIC_INFO;
            struct task_basic_info      t_info;
            mach_msg_type_number_t      t_info_count = TASK_BASIC_INFO_COUNT;
        #endif
        if (task_info(mach_task_self(), flavor, (task_info_t)&t_info, &t_info_count) == KERN_SUCCESS) {
            usage.total    = t_info.resident_size;
            usage.resident = t_info.resident_size;
            if (usage.total_peak < usage.total) {
                usage.total_peak = usage.total;
            }
            if (usage.resident_peak < usage.resident) {
                usage.resident_peak = usage.resident;
            }
            // shared memory and etc are unavailable, even with mach_task_basic_info
            return true;
        }
        CNcbiError::SetFromErrno();
    #else
        CNcbiError::Set(CNcbiError::eUnknown);
    #endif

#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return false;
}


int CProcess::GetThreadCount(void)
{
#if defined(NCBI_OS_MSWIN)
    PROCESSENTRY32 entry;
    if (CWinFeature::FindProcessEntry(x_GetPid(), entry)) {
        return entry.cntThreads;
    }
    CNcbiError::SetFromWindowsError();

#elif defined(NCBI_OS_LINUX)
    return CLinuxFeature::GetThreadCount(GetPid());

#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return -1;
}


int CCurrentProcess::GetThreadCount(void)
{
#if defined(NCBI_OS_MSWIN)
    PROCESSENTRY32 entry;
    if (CWinFeature::FindProcessEntry(::GetCurrentProcessId(), entry)) {
        return entry.cntThreads;
    }
    CNcbiError::SetFromWindowsError();

#elif defined(NCBI_OS_LINUX)
    return CLinuxFeature::GetThreadCount();

#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return -1;
}


int CProcess::GetFileDescriptorsCount(void)
{
    // Use CCurrentProcess::GetFileDescriptorsCount() for the current process,
    // it can give better results than "generic" version.
    if ( IsCurrent() ) {
        return CCurrentProcess::GetFileDescriptorsCount();
    }
#if defined(NCBI_OS_LINUX)
    return CLinuxFeature::GetFileDescriptorsCount(GetPid());
#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return -1;
}


int CCurrentProcess::GetFileDescriptorsCount(int* soft_limit, int* hard_limit)
{
#if defined(NCBI_OS_UNIX)
    int n = -1;

    // Get limits first
    struct rlimit rlim;
    rlim_t cur_limit = -1;
    rlim_t max_limit = -1;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        cur_limit = rlim.rlim_cur;
        max_limit = rlim.rlim_max;
    } else {
        // fallback to sysconf()
        cur_limit = static_cast<rlim_t>(sysconf(_SC_OPEN_MAX));
    }
    if (soft_limit) {
        *soft_limit = (cur_limit > INT_MAX) ? INT_MAX : static_cast<int>(cur_limit);
    }
    if (hard_limit) {
        *hard_limit = (max_limit > INT_MAX) ? INT_MAX : static_cast<int>(max_limit);
    }

    #if defined(NCBI_OS_LINUX)
    n = CLinuxFeature::GetFileDescriptorsCount(GetPid());
    #endif
    // Fallback to analysis via looping over all fds
    if (n < 0  &&  cur_limit > 0) {
        int max_fd = (cur_limit > INT_MAX) ? INT_MAX : static_cast<int>(cur_limit);
        for (int fd = 0;  fd < max_fd;  ++fd) {
            if (fcntl(fd, F_GETFD, 0) == -1) {
                if (errno == EBADF) {
                    continue;
                }
            }
            ++n;
        }
    }
    if (n >= 0) {
        return n;
    }
    CNcbiError::Set(CNcbiError::eUnknown);

#else
    if ( soft_limit ) { *soft_limit = -1; }
    if ( hard_limit ) { *hard_limit = -1; }
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif

    return -1;
}


/////////////////////////////////////////////////////////////////////////////
//
// CPIDGuard
//
// NOTE: CPIDGuard know nothing about PID file modification or deletion 
//       by other processes behind this API. Be aware.


CPIDGuard::CPIDGuard(const string& filename)
    : m_PID(0)
{
    string dir;
    CDirEntry::SplitPath(filename, &dir, 0, 0);
    if (dir.empty()) {
        m_Path = CDirEntry::MakePath(CDir::GetTmpDir(), filename);
    } else {
        m_Path = filename;
    }
    // Create guard for MT-Safe protect    
    m_MTGuard.reset(new CInterProcessLock(m_Path + ".guard"));
    // Update PID
    UpdatePID();
}


/// @deprecated
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
            CNcbiOfstream out(m_Path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
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
        pid = CCurrentProcess::GetPid();
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
