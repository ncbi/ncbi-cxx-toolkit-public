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
#include <corelib/interprocess_lock.hpp>

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
/// @note:  All CExec:: functions work with process handles.

class NCBI_XNCBI_EXPORT CProcess
{
public:
    /// How to interpret the passed process identifier argument.
    enum EProcessType {
        ePid,     ///< A real process identifier (pid).
        eHandle   ///< A process handle.
    };

    /// Default wait time (milliseconds) between "soft" and "hard"
    /// attempts to terminate a process.
    static const unsigned long kDefaultKillTimeout;

    /// Constructor.
    CProcess(TPid process, EProcessType type = eHandle);
#if defined(NCBI_OS_MSWIN)
    // NB: On MS Windows process identifiers and process handles are different.
    CProcess(TProcessHandle process, EProcessType type = eHandle);
#endif

    /// Get type of stored process identifier.
    /// @note
    ///   On Windows process identifiers and process handles are different.
    ///   On UNIX handle/pid are the same and GetHandle()/GetPid() return
    ///   the same value.
    EProcessType GetType(void) const { return m_Type; }

    /// Get stored process handle.
    /// @sa
    ///   GetCurrentHandle
    TProcessHandle GetHandle(void) const { return (TProcessHandle)m_Process; }

    /// Get stored process identifier (pid).
    /// @sa
    ///   GetCurrentPid
    TPid GetPid(void) const { return (TPid)m_Process; }

    /// Get process handle for the current process (esp. on Windows).
    /// @note
    ///   On Windows process identifiers and process handles are different.
    ///   On UNIX handle/pid are the same and GetCurrentHandle()/GetCurrentPid()
    ///   return the same value.
    /// @sa
    ///   GetCurrentPid
    static TProcessHandle GetCurrentHandle(void);

    /// Get process identifier for the current process.
    /// @sa
    ///   GetCurrentHandle
    static TPid GetCurrentPid(void);

    /// Get process identifier for the parent of the current process.
    /// @sa
    ///   GetCurrentPid
    static TPid GetParentPid(void);

    /// Check for process existence.
    ///
    /// @return
    ///   TRUE  - if the process is still running.
    ///   FALSE - if the process does exist or has already been terminated.
    /// @note
    ///   On Unix this method returns TRUE also for "zombie" processes,
    ///   those finished executing but waiting to post their exit code.
    ///   Usually the parent process should call Wait() for such processes
    ///   and release (aka reap) them.
    /// @sa
    ///   Wait
    bool IsAlive(void) const;

    /// Terminate process.
    ///
    /// First try "soft" and then try "hard" attempt to terminate the process.
    /// @note Even the "hard" attempt does not guarantee that the process will
    /// be actually killed.  Process termination can take some time, and the
    /// process may remain "alive" even after the "hard" termination attempt.
    ///
    /// @param timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process. 
    ///   If timeout is zero then use an unsafe process termination: just
    ///   try to terminate the process without checks that it is really gone.
    /// @note
    ///   On UNIX in case of a zero or very small timeout, the killed process
    ///   may not be released immediately (and continue to persist as a zombie
    ///   process) even after this call.
    /// @return
    ///   TRUE  - if the process did not exist or was successfully terminated.
    ///   FALSE - if the process is still running, cannot be terminated, or
    ///           it is terminating right now (but is still incomplete).
    /// @sa KillGroup, KillGroupById
    bool Kill(unsigned long timeout = kDefaultKillTimeout) const;

    /// Terminate a group of processes.
    ///
    /// This method tries to terminate all processes in the group to which
    /// process, specified in the constructor, belongs.
    ///
    /// @param timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process group. 
    ///   If timeout is zero then use an unsafe process termination: just
    ///   try to terminate the group without checks that it is really gone.
    /// @note
    ///   On UNIX in case of a zero or very small timeout, some processes
    ///   may not be released (and continue to persist as zombie processes)
    ///   even after this call.
    ///   On MS Windows this method tries to terminate the process itself
    ///   and all of its children.  But in case when one of the processes,
    ///   which should be terminated, spawns a new process at the very moment
    ///   that this method gets called, the new process may not be terminated.
    /// @return
    ///   TRUE  - if the process group did not exist or was successfully
    ///           terminated.
    ///   FALSE - if the process group is still running, cannot be terminated,
    ///           or it is terminating right now (but is still incomplete).
    /// @sa Kill
    bool KillGroup(unsigned long timeout = kDefaultKillTimeout) const;

    /// Terminate a group of processes with specified ID.
    ///
    /// Note: Implemented on UNIX only, on Windows returns FALSE.
    /// @param pgid
    ///   Process group ID to terminate.
    ///   if "pgid" parameter is zero, terminate the process group of
    ///   the current process
    /// @param timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process group. 
    ///   If timeout is zero then use an unsafe process termination: just
    ///   try to terminate the process group without checks whether it is
    ///   really gone.
    /// @note
    ///   On UNIX in case of a zero or very small timeout, some processes from
    ///   the process group to be killed may not be released immediately
    ///   (and continue to persist as zombie processes) even after this call.
    /// @return
    ///   TRUE  - if the process group did not exist or was successfully
    ///           terminated.
    ///   FALSE - if the process group is still running, cannot be terminated,
    ///           or it is terminating right now (but is still incomplete).
    /// @sa Kill
    static bool KillGroupById(TPid pgid,
                              unsigned long timeout = kDefaultKillTimeout);

    /// Extended exit information for waited process.
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
        /// All other methods' return values are good only if this method
        /// returns TRUE, otherwise they may throw exceptions.
        bool IsPresent(void) const;

        /// TRUE if the process is still alive.
        bool IsAlive(void) const;

        /// TRUE if the process terminated normally.
        bool IsExited(void) const;

        /// TRUE if the process terminated by a signal (UNIX only).
        bool IsSignaled(void) const;

        /// Get process exit code.
        /// Works only if IsExited() returns TRUE, otherwise returns -1.
        int GetExitCode(void) const;

        /// Get the signal number that has caused the process to terminate
        /// (UNIX only).
        /// Works only if IsSignaled() returns TRUE, otherwise returns -1.
        int GetSignal(void) const;

    private:
        int state;    ///< Process state (unknown/alive/terminated).
        int status;   ///< Process status information.

        friend class CProcess;
    };

    /// Wait until process terminates.
    ///
    /// Wait until the process terminates or timeout expires.
    /// Return immediately if the specifed process has already terminated.
    /// @param timeout
    ///   Time interval in milliceconds (infinite by default) to wait.
    /// @param info
    ///   Extended exit information for terminated process.
    ///   Note that if CProcess:Kill() was used to terminate the process
    ///   then the extended information may not be available.
    /// @return
    ///   - Exit code of the process, if the call completed without errors.
    ///   - (-1) if an error occurred or it was impossible to get the exit
    ///     code of the process.  If 'info' parameter is specified, then it
    ///     is filled with additional information about the process.
    /// @note
    ///   It is recommended to call this method for all processes started 
    ///   in eNoWait or eDetach modes (except on Windows for eDetach), because
    ///   it releases "zombie" processes, i.e. those finished running and
    ///   waiting for their parent to obtain their exit code.  If Wait() is
    ///   not called somewhere, then the child process will be completely
    ///   removed from the system only when its parent process ends.
    /// @sa
    ///   IsAlive, CExitInfo, CExec
    int Wait(unsigned long timeout = kInfiniteTimeoutMs,
             CExitInfo* info = 0) const;

    /// Forking flags.
    enum FForkFlags {
        fFF_UpdateDiag      = 1,  ///< Reset diagnostics timer and log an
                                  ///< app-start message in the child process.
        fFF_AllowExceptions = 32, ///< Throw an exception if fork(2) failed.
    };
    /// Bit-wise OR of FForkFlags @sa FForkFlags
    typedef unsigned TForkFlags;

    /// Fork the process. Update PID and GUID used for logging.
    ///
    /// @return In the parent process, the call returns the child process ID.
    ///         In the child process, the call returns zero.
    ///         In case of an error, unless the fFF_AllowExceptions flag is
    ///         given, the call returns -1.
    ///
    /// @throw If the fFF_AllowExceptions flag is specified,
    ///        throws a CCoreException in case of a fork(2) failure.
    ///        If the platform does not support process forking,
    ///        an exception is always thrown regardless of whether the
    ///        fFF_AllowExceptions flag is specified.
    ///
    static TPid Fork(TForkFlags flags = fFF_UpdateDiag);


    /// Daemonization flags
    enum FDaemonFlags {
        fDF_KeepCWD         = 1,    ///< Don't change CWD to "/"
        fDF_KeepStdin       = 2,    ///< Keep stdin open as "/dev/null" (RO)
        fDF_KeepStdout      = 4,    ///< Keep stdout open as "/dev/null" (WO)
        fDF_ImmuneTTY       = 8,    ///< Make daemon immune to re-acquiring
                                    ///< a controlling terminal
        fDF_KeepParent      = 16,   ///< Do not exit the parent process
                                    ///< but return
        fDF_AllowExceptions = 32,   ///< Throw an exception in case
                                    ///< of an error.
        fDF_AllowThreads    = 64,   ///< Do not fail if pre-existing threads are detected

        fDontChroot = fDF_KeepCWD,    ///< @deprecated Use fDF_KeepCWD instead
        fKeepStdin  = fDF_KeepStdin,  ///< @deprecated Use fDF_KeepStdin instead
        fKeepStdout = fDF_KeepStdout, ///< @deprecated Use fDF_KeepStdout
        fImmuneTTY  = fDF_ImmuneTTY,  ///< @deprecated Use fDF_ImmuneTTY instead
        fKeepParent = fDF_KeepParent  ///< @deprecated Use fDF_KeepParent
    };
    /// Bit-wise OR of FDaemonFlags @sa FDaemonFlags
    typedef unsigned int TDaemonFlags;

    /// Go daemon.
    ///
    /// Return TPid(-1) in the daemon thread (the parent thread doesn't return
    /// unless fKeepParent is set), or TPid(daemon) in the parent thread.
    /// On error return 0 in the parent thread (no daemon created), see errno.
    ///
    /// Reopen stderr/cerr in the daemon thread if "logfile" specified as
    /// non-NULL (stderr will open to "/dev/null" if "logfile" has been passed
    /// as ""), otherwise stderr is closed in the daemon thread.
    ///
    /// Unless instructed by the "flags" parameter, the daemon thread has its
    /// stdin/cin and stdout/cout closed, and the current working directory
    /// changed to the root directory ("/").
    ///
    /// If kept open, stdin and stdout are both redirected to "/dev/null".
    /// Opening a terminal device as a controlling terminal is allowed, unless
    /// fImmuneTTY is specified in the flags, which then causes a second
    /// fork() so that the resultant process won't be allowed to open a TTY as
    /// its controlling TTY (but only with an explicit O_NOCTTY, see open(2)),
    /// thus protecting the process from any blocking via TTY signaling.
    ///
    /// Note that this call is somewhat destructive and may not be able
    /// to restore the process that called it to a state prior to the call
    /// in case of an error.  So that calling process can find the standard
    //  file pointers (and sometimes descriptors) clobbered up.
    static TPid Daemonize(const char* logfile = 0, TDaemonFlags flags = 0);

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

#ifdef NCBI_OS_UNIX
    static TPid x_DaemonizeEx(const char* logfile, TDaemonFlags flags);
#endif

    // itptr_t type can store both pid and process handle.
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
        : CException(info, prev_exception, message, severity),
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
/// CPIDGuard -- Process guard.
///
/// If file already exists, CPIDGuard try to check that the process with PID
/// specified in the file is running or not. If guarded process is still
/// running, CPIDGuard throw an exception. Otherwise, it create file with
/// current PID. CPIDGuard use reference counters in the PID file.
/// This means that some CPIDGuard objects can be created for the same PID
/// and file name.
///
/// @note
///   If you need something just to prevent run some application twice,
///   please look to CInterProcessLock class.
/// @note
///   CPIDGuard know nothing about PID file modification or deletion
///   by other processes directly, without using this API. Be aware.
/// 

class NCBI_XNCBI_EXPORT CPIDGuard
{
public:
    /// Create/check PID file.
    ///
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    ///
    /// @filename
    ///   Name of the file to store PID. 
    ///   If "filename" contains path, it should be absolute
    ///   and point to existing directory.
    ///   If "filename" contains no path, that $TMPDIR or /tmp will be
    ///   used on Unix, and %HOME% on Windows to store it.
    CPIDGuard(const string& filename);

    /// Create/check PID file.
    ///
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    ///
    /// @filename
    ///   Name of the file to store PID. 
    ///   If should not include any path, relative or absolute.
    /// @dir
    ///   An absolute path to the existing directory on the file system
    ///   to store PID file "filename".
    ///   If "dir" is empty and "filename" doesn't contains path,
    ///   that $TMPDIR or /tmp will be used on Unix, and %HOME% on Windows.
    CPIDGuard(const string& filename, const string& dir);

    /// Destructor.
    ///
    /// Just calls Release();
    ~CPIDGuard(void);

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

    /// Returns non-zero if there was a stale file.
    /// Always return 0, because new implementation don't allow
    /// any stale file left on the file system.
    ///
    /// @deprecated
    NCBI_DEPRECATED
    TPid GetOldPID(void) { return 0; }

private:
    string  m_Path;  //< File path to store PID.
    TPid    m_PID;   //< Sored PID.
    auto_ptr<CInterProcessLock> m_MTGuard;  //< MT-Safe protection guard.
    auto_ptr<CInterProcessLock> m_PIDGuard; //< Guard to help with "PID reuse" problem.
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___NCBI_PROCESS__HPP */
