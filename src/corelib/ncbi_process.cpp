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
 * Authors:  Aaron Ucko, Vladimir Ivanov
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_safe_static.hpp>


#if defined(NCBI_OS_UNIX)
#  include <sys/types.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <errno.h>
#  include <unistd.h>
#elif defined(NCBI_OS_MSWIN)
#  include <process.h>
#  include <tlhelp32.h>
#endif


BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//
// CProcess 
//

// Predefined timeouts (in microseconds)
const unsigned long kWaitPrecision = 100; 
const unsigned long CProcess::kDefaultKillTimeout = 1000;


CProcess::CProcess(long process, EProcessType type)
    : m_Process(process), m_Type(type)
{
    return;
}

#if defined(NCBI_OS_MSWIN)
// The helper constructor for MS Windows to avoid cast from HANDLE to long

CProcess::CProcess(HANDLE process, EProcessType type)
    : m_Process((long)process), m_Type(type)
{
    return;
}

#endif


TPid CProcess::GetCurrentPid(void)
{
#if defined(NCBI_OS_UNIX)
    return getpid();
#elif defined(NCBI_OS_MSWIN)
    return GetCurrentProcessId();
#else
#  error "Not implemented on this platform"
#endif
}


TPid CProcess::GetParentPid(void)
{
#if defined(NCBI_OS_UNIX)
    return getppid();
#elif defined(NCBI_OS_MSWIN)
    TPid ppid = (TPid)(-1);
    // open snapshot handle
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
#else
#  error "Not implemented on this platform"
#endif
}


bool CProcess::IsAlive(void) const
{
#if defined(NCBI_OS_UNIX)
    if ( kill((TPid)m_Process, 0) < 0  &&  errno != EPERM ) {
        return false;
    }
    return true;

#elif defined(NCBI_OS_MSWIN)
    HANDLE hProcess = 0;
    if (m_Type == ePid) {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, m_Process);
        if (!hProcess) {
            return GetLastError() == ERROR_ACCESS_DENIED;
        }
    } else {
        hProcess = (HANDLE)m_Process;
    }
    DWORD status = 0;
    _ASSERT(STILL_ACTIVE != 0);
    GetExitCodeProcess(hProcess, &status);
    if (m_Type == ePid) {
        CloseHandle(hProcess);
    }
    return status == STILL_ACTIVE;

#else
#  error "Not implemented on this platform"
#endif
}


bool CProcess::Kill(unsigned long timeout) const
{
#if defined(NCBI_OS_UNIX)
    TPid pid = (TPid)m_Process;
    int status;

    // Try to kill process
    if (kill(pid, SIGTERM) == -1  &&  errno == EPERM) {
        return false;
    }
    // Check process termination within timeout
    do {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            // Release zombie process from the system
            waitpid(pid, &status, 0);
            return true;
        }
        unsigned long x_sleep = kWaitPrecision;
        if (timeout < kWaitPrecision) {
            x_sleep = timeout;
        }
        if ( x_sleep ) {
            timeout -= x_sleep;
            SleepMilliSec(x_sleep);
        }
    } while (timeout);

    // Try harder to kill stubborn process
    if (kill(pid, SIGKILL) == -1  &&  errno == EPERM) {
        return false;
    }
    // Release zombie process from the system
    if (waitpid(pid, &status, WNOHANG) == pid) {
        waitpid(pid, &status, 0);
    }
    return true;

#elif defined(NCBI_OS_MSWIN)

    HANDLE hProcess    = NULL;
    HANDLE hThread     = NULL;
    bool   enable_sync = true;

    // Get process handle
    if (m_Type == ePid) {
        hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_TERMINATE |
                               SYNCHRONIZE, FALSE, m_Process);
        if ( !hProcess ) {
            enable_sync = false;
            hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, m_Process);
            if (!hProcess   &&  GetLastError() == ERROR_ACCESS_DENIED) {
                return false;
            }
        }
    } else {
        hProcess = (HANDLE)m_Process;
    }
    // Check process handle
    if ( !hProcess  ||  hProcess == INVALID_HANDLE_VALUE ) {
        return true;
    }

    // Terminate process
    bool terminated = false;
    try {
        // Safe process termination
        // (kernel32.dll loaded at same address in each process)
        FARPROC exitproc = GetProcAddress(GetModuleHandle("KERNEL32.DLL"),
                                          "ExitProcess");
        if ( exitproc ) {
            hThread = CreateRemoteThread(hProcess, NULL, 0,
                                         (LPTHREAD_START_ROUTINE)exitproc,
                                         0, 0, 0);
        }
        // Wait for process terminated, or timeout expired
        if (enable_sync  &&  timeout) {
            if (WaitForSingleObject(hProcess, timeout) == WAIT_OBJECT_0) {
                throw true;
            }
        }
        // Try harder to kill stubborn process
        if ( TerminateProcess(hProcess, -1) != 0  ||
             GetLastError() == ERROR_INVALID_HANDLE ) {
            // If process terminated succesfuly or error occur but
            // process handle became invalid -- process has terminated
            terminated = true;
        }
    }
    catch (bool e) {
        terminated = e;
    }
    // Close opened temporary process handle
    if ( hThread ) {
        CloseHandle(hThread);
    }
    if (m_Type == ePid ) {
        CloseHandle(hProcess);
    }
    return terminated;

#else
#  error "Not implemented on this platform"
#endif
}


int CProcess::Wait(unsigned long timeout) const
{
#if defined(NCBI_OS_UNIX)
    TPid  pid     = (TPid)m_Process;
    int   options = (timeout == kMax_ULong /*infinite*/) ? 0 : WNOHANG;
    int   status;

    // Check process termination within timeout (or infinite)
    do {
        TPid ws = waitpid(pid, &status, options);
        if (ws > 0) {
            // Process has terminated.
            if ( options ) {
                // Release zombie process from the system.
                waitpid(pid, &status, 0);
            }
            return WEXITSTATUS(status);
        } 
        else if (ws < 0  &&  errno != EINTR ) {
            // Some error
            break;
        }
        // Process is still running
        unsigned long x_sleep = kWaitPrecision;
        if (timeout != kMax_ULong /*infinite*/) {
            if (timeout < kWaitPrecision) {
                x_sleep = timeout;
            }
            timeout -= x_sleep;
        }
        if ( x_sleep ) {
            SleepMilliSec(x_sleep);
        }
    } while (timeout); 

    return -1;

#elif defined(NCBI_OS_MSWIN)
    HANDLE hProcess;
    bool   enable_sync = true;
    // Get process handle
    if (m_Type == ePid) {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                               FALSE, m_Process);
        if ( !hProcess ) {
            enable_sync = false;
            hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, m_Process);
            if (!hProcess   &&  GetLastError() == ERROR_ACCESS_DENIED) {
                return false;
            }
        }
    } else {
        hProcess = (HANDLE)m_Process;
    }
    DWORD status = -1;
    try {
        // Is process still running?
        if ( !hProcess  || hProcess == INVALID_HANDLE_VALUE ) {
            throw -1;
        }
        if ( GetExitCodeProcess(hProcess, &status)  &&
             status != STILL_ACTIVE ) {
            throw (int)status;
        }
        // Wait for process termination, or timeout expired
        if (enable_sync  &&  timeout) {
            DWORD tv = (timeout == kMax_ULong) ? INFINITE : (DWORD)timeout;
            if (WaitForSingleObject(hProcess, tv) != WAIT_OBJECT_0) {
                throw -1;
            }
        }
        // Get process exit code
        if ( !GetExitCodeProcess(hProcess, &status) ) {
            throw -1;
        }
    }
    catch (int e) {
        status = e;
    }
    if (m_Type == ePid ) {
        CloseHandle(hProcess);
    }
    return (int)status;
#else
#  error "Not implemented on this platform"
#endif
}



/////////////////////////////////////////////////////////////////////////////
//
// CPIDGuard
//

// Protective mutex
DEFINE_STATIC_FAST_MUTEX(s_PidGuardMutex);

// NOTE: This method to protect PID file works only within one process.
//       CPIDGuard know nothing about PID file modification or deletion 
//       by other processes. Be aware.


CPIDGuard::CPIDGuard(const string& filename, const string& dir)
    : m_OldPID(0), m_NewPID(0)
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
    UpdatePID();
}


CPIDGuard::~CPIDGuard(void)
{
    Release();
}


void CPIDGuard::Release(void)
{
    if ( !m_Path.empty() ) {
        // MT-Safe protect
        CFastMutexGuard LOCK(s_PidGuardMutex);

        // Read info
        TPid pid = 0;
        unsigned int ref = 0;
        CNcbiIfstream in(m_Path.c_str());
        if ( in.good() ) {
            in >> pid >> ref;
            in.close();
            if ( m_NewPID != pid ) {
                // We do not own this file more
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
                               + strerror(errno));
                }
            } else {
                // Remove the file
                CDirEntry(m_Path).Remove();
            }
        }
        m_Path.erase();
    }
}


void CPIDGuard::Remove(void)
{
    if ( !m_Path.empty() ) {
        // MT-Safe protect
        CFastMutexGuard LOCK(s_PidGuardMutex);
        // Remove the file
        CDirEntry(m_Path).Remove();
        m_Path.erase();
    }
}


void CPIDGuard::UpdatePID(TPid pid)
{
    if (pid == 0) {
        pid = CProcess::GetCurrentPid();
    }

    // MT-Safe protect
    CFastMutexGuard LOCK(s_PidGuardMutex);

    // Read old PID
    unsigned int ref = 1;
    CNcbiIfstream in(m_Path.c_str());
    if ( in.good() ) {
        in >> m_OldPID >> ref;
        if ( m_OldPID == pid ) {
            // Guard the same PID. Just increase the reference counter.
            ref++;
        } else {
            if ( CProcess(m_OldPID,CProcess::ePid).IsAlive() ) {
                NCBI_THROW2(CPIDGuardException, eStillRunning,
                            "Process is still running", m_OldPID);
            }
            ref = 1;
        }
    }
    in.close();

    // Write new PID
    CNcbiOfstream out(m_Path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( out.good() ) {
        out << pid << endl << ref << endl;
    }
    if ( !out.good() ) {
        NCBI_THROW(CPIDGuardException, eWrite,
                   "Unable to write into PID file " + m_Path + ": "
                   + strerror(errno));
    }
    // Save updated pid
    m_NewPID = pid;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/02/11 13:20:40  dicuccio
 * Compiler fix: CreateToolhelp32Snapshot() return value compared to
 * INVALID_HANDLE_VALUE, not -1
 *
 * Revision 1.10  2005/02/11 04:39:49  lavr
 * +GetParentPid()
 *
 * Revision 1.9  2004/07/12 16:46:56  ivanov
 * Added implementation CPIDGuard::Remove()
 *
 * Revision 1.8  2004/05/18 17:01:15  ivanov
 * CProcess::
 *     + WaitForAlive(), WaitForTerminate().
 *     Fixed UNIX version Wait() to correct work with infinite timeouts.
 * CPIDGuard::
 *     Fixed CPIDGuard to use reference counters in PID file.
 *     Added CPIDGuard::Remove().
 *
 * Revision 1.7  2004/05/14 13:59:26  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.6  2004/03/04 19:08:36  ivanov
 * Fixed CProcess::IsAlive()
 *
 * Revision 1.5  2003/12/04 18:45:35  ivanov
 * Added helper constructor for MS Windows to avoid cast from HANDLE to long
 *
 * Revision 1.4  2003/12/03 17:03:01  ivanov
 * Fixed Kill() to handle zero timeouts
 *
 * Revision 1.3  2003/10/01 20:23:26  ivanov
 * Added const specifier to CProcess class member functions
 *
 * Revision 1.2  2003/09/25 17:18:21  ucko
 * UNIX: +<unistd.h> for getpid()
 *
 * Revision 1.1  2003/09/25 16:53:41  ivanov
 * Initial revision. CPIDGuard class moved from ncbi_system.cpp.
 *
 * ===========================================================================
 */
