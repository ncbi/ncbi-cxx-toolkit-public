#ifndef CORELIB___NCBI_PROCESS__HPP
#define CORELIB___NCBI_PROCESS__HPP

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

/// @file ncbi_process.hpp
/// Defines a process management classes.
///
/// Defines classes:
///     CProcess
///     CPIDGuard
///
/// Implemented for: UNIX, MS-Windows


#include <corelib/ncbistr.hpp>

#if defined(NCBI_OS_UNIX)
#  include <sys/types.h>
#elif defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#else
#  error "CProcess is not implemented on this platform"
#endif


/** @addtogroup Process
 *
 * @{
 */

BEGIN_NCBI_SCOPE

/// Infinite timeout in milliseconds.
const unsigned long kInfiniteTimeoutMs = kMax_ULong;

/// Turn on/off workaround for linux PID and PPID
#if defined(NCBI_OS_LINUX)
#  define NCBI_THREAD_PID_WORKAROUND
#endif

/// Process identifier (PID) and process handle.
#if defined(NCBI_OS_UNIX)
  typedef pid_t  TPid;
  typedef TPid   TProcessHandle;
#elif defined(NCBI_OS_MSWIN)
  typedef DWORD  TPid;
  typedef HANDLE TProcessHandle;
#else
  typedef int    TPid;
  typedef TPid   TProcessHandle;
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// CProcess --
///
/// Process management functions.
///
/// Class can work with process identifiers and process handles.
/// On Unix both said terms are equivalent and correspond a pid.
/// On MS Windows they are different.
///
/// NOTE:  All CExec:: functions works with process handle.

class NCBI_XNCBI_EXPORT CProcess
{
public:
    /// How to interpret passed process identifier.
    enum EProcessType {
        ePid,     ///< As real process identifier (pid).
        eHandle   ///< As a process handle.
    };

    /// Default wait time (milliseconds) between "soft" and "hard"
    /// attempts to terminate the process.
    static const unsigned long kDefaultKillTimeout;

    /// Constructor.
    CProcess(TPid process, EProcessType type = eHandle);
#if defined(NCBI_OS_MSWIN)
    // On MS Windows process identifiers and process handles
    // are different.
    CProcess(TProcessHandle process, EProcessType type = eHandle);
#endif

    /// Get process identifier for a current running process.
    static TPid GetCurrentPid(void);

    /// Get process identifier for a parent of the current process.
    static TPid GetParentPid(void);

    /// Check process existence.
    ///
    /// @return
    ///   TRUE  - if the process is still running.
    ///   FALSE - if the process did not exist or was already terminated.
    /// @note
    ///   On Unix this method return TRUE also for "zombie" processes,
    ///   that finished working, but waiting to return it's exit status.
    ///   Usually the parent process should call Wait() for such processes
    ///   and release them.
    /// @sa
    ///   Wait
    bool IsAlive(void) const;

    /// Terminate process.
    ///
    /// First try "soft" and second "hard" attempts to terminate the process.
    /// Even "hard" terminate do not assure us that process will be
    /// terminated. Process termination can take some time, and process can
    /// be "alive" after "hard" attempt of termination.
    ///
    /// @timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process. 
    ///   If the timeout have zero value, than use unsafe process termination,
    ///   just try to terminate a process without checks that it is really
    ///   terminated.
    /// @note
    ///   On UNIX in case of zero or very small timeout the killing process
    ///   can be not released and continue to persists as zombie process
    ///   even after call of this function.
    /// @return
    ///   TRUE  - if the process did not exist or was successfully terminated.
    ///   FALSE - if the process is still running, cannot be terminated, or
    ///           it is terminating right now (but still not terminated).
    /// @sa KillGroup, KillGroupById
    bool Kill(unsigned long timeout = kDefaultKillTimeout) const;
  
    /// Terminate a group of processes.
    ///
    /// This method tries to terminate all processes in the group to which
    /// process, specified in the constructor, belongs.
    ///
    /// @timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process group. 
    ///   If the timeout have zero value, then use unsafe process termination,
    ///   just try to terminate a process group without checks that it is
    ///   really terminated.
    /// @note
    ///   On UNIX in case of zero or very small timeout the killing process
    ///   can be not released and continue to persist as zombie process
    ///   even after this call completes.
    ///   On MS Windows this method tries to terminate the process itself
    ///   and all its children.  But in the case if one of the processes,
    ///   which should be terminated, spawns a new process at the moment
    ///   of execution this method, then that new process might not be
    ///   terminated.
    /// @return
    ///   TRUE  - if the process group did not exist or was successfully
    ///           terminated.
    ///   FALSE - if the process group is still running, cannot be terminated,
    ///           or it is terminating right now (but is still not terminated).
    /// @sa Kill
    bool KillGroup(unsigned long timeout = kDefaultKillTimeout) const;

    /// Terminate a group of processes with specified ID.
    ///
    /// Note: Implemented on UNIX only, on Windows return FALSE.
    /// @pgid
    ///   Process group ID to terminate.
    ///   if "pgid" parameter is zero, terminate the process group of
    ///   the current process
    /// @timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process group. 
    ///   If the timeout have zero value, than use unsafe process termination,
    ///   just try to terminate a process group without checks that it is
    ///   really terminated.
    /// @note
    ///   On UNIX in case of zero or very small timeout the killing process
    ///   can be not released and continue to persists as zombie process
    ///   even after call of this function.
    /// @return
    ///   TRUE  - if the process group did not exist or was successfully
    ///           terminated.
    ///   FALSE - if the process group is still running, cannot be terminated,
    ///           or it is terminating right now (but still not terminated).
    /// @sa Kill
    static bool KillGroupById(TPid pgid,
                              unsigned long timeout = kDefaultKillTimeout);

    /// The extended exit information for waited process.
    /// All information about the process available only after Wait() method
    /// with specified parameter 'info' and if IsPresent() method returns
    /// TRUE. 
    class NCBI_XNCBI_EXPORT CExitInfo
    {
    public:
        /// Constructor.
        CExitInfo(void);

        /// TRUE if the object contains information about the process state.
        ///
        /// All other methods returns value only if this method returns
        /// TRUE, otherwise they trow an exception.
        bool IsPresent(void) const;
    
        /// TRUE if the process is still alive.
        bool IsAlive(void) const;
    
        /// TRUE if the process terminated normally.
        bool IsExited(void) const;
       
        /// TRUE if the process terminated by signal (UNIX only).
        bool IsSignaled(void) const;

        /// Get process exit code.
        /// Works only if IsExited() returns TRUE, otherwise return -1.
        int GetExitCode(void) const;
        
        /// Get the number of the signal that caused process to terminate.
        /// (UNIX only).
        /// Works only if IsSignaled() returns TRUE, otherwise return -1.
        int GetSignal(void) const;
        
    private:
        int state;    ///< Process state (unknown/alive/terminated).
        int status;   ///< Process status information.
       
        friend class CProcess;
    };

    /// Wait until process terminates.
    ///
    /// Wait until a process terminates or timeout expires.
    /// Return immediately if the specifed process has already terminated.
    /// @param timeout
    ///   Time-out interval in milliceconds. By default it is infinite.
    /// @param info
    ///   Extended exit information for terminated process.
    ///   Note, that if CProcess:Kill() was used to terminate the process
    ///   then the extended information may not be available.
    /// @return
    ///   - Exit code of the process, if the call completed without errors.
    ///   - (-1), if error has occurred or it is impossible to get an exit
    ///     code of the process. If 'info' parameter is specified, it is
    ///     filled with additional information about the process.
    /// @note
    ///   It is recommended to call this method for all processes started 
    ///   in eNoWait or eDetach modes (except on Windows for eDetach), because
    ///   it release "zombie" processes, that finished working and waiting
    ///   to return it's exit status. If Wait() is not called somewhere,
    ///   the child process will be completely removed from the system only
    ///   when the parent process ends.
    /// @sa
    ///   IsAlive, CExitInfo, CExec
    int Wait(unsigned long timeout = kInfiniteTimeoutMs,
             CExitInfo* info = 0) const;

    /// Fork (throw exception if the platform does not support fork),
    /// update PID and GUID used for logging.
    static TPid Fork(void);

    /// Daemonization flags
    enum FDaemonFlags {
        fDontChroot = 1,  ///< Don't change to "/"
        fKeepStdin  = 2,  ///< Keep stdin open as "/dev/null" (RO)
        fKeepStdout = 4,  ///< Keep stdout open as "/dev/null" (WO)
        fImmuneTTY  = 8   ///< Make daemon immune to opening of controlling TTY
    };
    /// Bit-wise OR of FDaemonFlags @sa FDaemonFlags
    typedef unsigned int TDaemonFlags;


    /// Go daemon.
    ///
    /// Return true in the daemon thread (parent thread doesn't return).
    /// Return false on error (no daemon created), see errno.
    ///
    /// Reopen stderr/cerr in daemon thread if "logfile" specified as non-NULL
    /// (stderr will open to "/dev/null" if "logfile" has been passed as ""),
    /// otherwise stderr is closed in the daemon thread.
    /// NB: Always check stderr for errors of failed redirection!
    ///
    /// Unless instructed by "flags" parameter, the daemon thread has its
    /// stdin/cin and stdout/cout closed, and current directory changed
    /// to root directory ("/").
    ///
    /// If kept open, stdin and stdout are both redirected to "/dev/null".
    /// Opening a terminal device as a controlling terminal is allowed, unless
    /// fImmuneTTY is specified in the flags, which then causes a second
    /// fork() so that the resultant process won't be allowed to open a TTY as
    /// its controlling TTY (but only with an explicit O_NOCTTY, see open(2)),
    /// thus protecting the process from any blocking via TTY signalling.
    ///
    /// Note that this call is somewhat destructive and may not be able
    /// to restore the process that called it to a state prior to the call
    /// in case of an error.  So that calling process can find std file
    /// pointers (and sometimes descriptors) screwed up.
    static bool Daemonize(const char* logfile = 0, TDaemonFlags flags = 0);

private:
#if defined NCBI_THREAD_PID_WORKAROUND
    // Flags for internal x_GetPid()
    enum EGetPidFlag {
        // get real or cached PID depending on thread ID
        ePID_GetCurrent,
        // get parent PID - real or cached depending on thread ID
        ePID_GetParent,
        // get real PID (of the thread) but do not cache it
        ePID_GetThread
    };
    static TPid sx_GetPid(EGetPidFlag flag);
    friend class CThread;
#endif

    // itptr_t type can store both: pid and process handle.
    intptr_t     m_Process;  ///< Process identifier.
    EProcessType m_Type;     ///< Type of process identifier.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CPIDGuardException --
///

class NCBI_XNCBI_EXPORT CPIDGuardException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eStillRunning, ///< The process listed in the file is still around.
        eWrite         ///< Unable to write into the PID file.
    };

    virtual const char* GetErrCodeString(void) const;

    /// Constructor.
    CPIDGuardException(const CDiagCompileInfo& info,
                       const CException* prev_exception, EErrCode err_code,
                       const string& message, TPid pid = 0, 
                       EDiagSev severity = eDiag_Error)
        : CException(info, prev_exception, CException::eInvalid,
                     message),
          m_PID(pid)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CPIDGuardException, CException);

public:
    virtual void ReportExtra(ostream& out) const
    {
        out << "pid " << m_PID;
    }

    TPid GetPID(void) const throw() { return m_PID; }

protected:
    virtual void x_Assign(const CException& src)
    {
        CException::x_Assign(src);
        m_PID = dynamic_cast<const CPIDGuardException&>(src).m_PID;
    }

private:
    TPid  m_PID;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CPIDGuard -- 
///
/// Define process guard.
///
/// If file already exists, CPIDGuard try to check that the process with PID
/// specified in the file is running or not. If guarded process is still
/// running, CPIDGuard throw an exception. Otherwise, it create file with
/// current PID. CPIDGuard use reference counters in the PID file.
/// This means that some CPIDGuard objects can be created for the same PID
/// and file name.
///
/// NOTE: This method to protect PID file works only within one process.
///       CPIDGuard know nothing about PID file modification or deletion 
///       by other processes. Be aware.

class NCBI_XNCBI_EXPORT CPIDGuard
{
public:
    /// If "filename" contains no path, make it relative to "dir"
    /// (which defaults to /tmp on Unix and %HOME% on Windows).
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    CPIDGuard(const string& filename, const string& dir = kEmptyStr);

    /// Destructor.
    ///
    /// Just calls Release();
    ~CPIDGuard(void);

    /// Returns non-zero if there was a stale file.
    TPid GetOldPID(void) { return m_OldPID; }

    /// Release PID.
    ///
    /// Decrease reference counter for current PID and remove the file
    /// if it is not used more (reference counter is 0).
    void Release(void);

    /// Remove the file.
    ///
    /// Remove PID file forcibly, ignoring any reference counter.
    void Remove(void);

    /// Update PID in the file.
    ///
    /// @param pid
    ///   The new process ID to store (defaults to the current PID);
    ///   useful when the real work occurs in a child process that outlives
    ///   the parent.
    void UpdatePID(TPid pid = 0);

private:
    string  m_Path;     //< File path to store PID.
    TPid    m_OldPID;   //< Old PID read from file.
    TPid    m_NewPID;   //< New PID wroted to file.
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___NCBI_PROCESS__HPP */
